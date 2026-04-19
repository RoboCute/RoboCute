#include <rbc_graphics/texture/hdri.h>
#include <luisa/core/fiber.h>
#include <algorithm>
#include <numeric>
namespace rbc
{
namespace detail
{

template <typename Value, std::enable_if_t<std::is_unsigned_v<Value> && (sizeof(Value) == 4u || sizeof(Value) == 8u), int> = 0>
[[nodiscard]] constexpr auto next_pow2(Value v) noexcept
{
#ifdef __cpp_lib_int_pow2
    return std::bit_ceil(v);
#else
    v--;
    v |= v >> 1u;
    v |= v >> 2u;
    v |= v >> 4u;
    v |= v >> 8u;
    v |= v >> 16u;
    if constexpr (sizeof(Value) == 8u) { v |= v >> 32u; }
    return v + 1u;
#endif
}

void create_alias_table(luisa::span<const float> values, luisa::span<HDRI::AliasEntry> table, luisa::span<float> pdf, bool parallel)
{
    const auto inplace = parallel ? 1ull : std::numeric_limits<size_t>::max();
    const auto sum = [&]() {
        std::atomic<double> d = 0;
        luisa::fiber::parallel(values.begin(), values.end(), 1024, [&](auto&& begin, auto&& end) {
			const double dd = std::accumulate(begin, end, 0.0, [](double a, float b) { return a + std::abs(b); });
			d += dd; }, inplace);
        return d.load();
    }();
    if (sum == 0.) [[unlikely]]
    {
        const auto n = static_cast<double>(values.size());
        const auto p = static_cast<float>(1.0 / n);
        std::fill(pdf.begin(), pdf.end(), p);
        size_t idx = 0;
        std::generate(table.begin(), table.end(), [&]() mutable {
            return HDRI::AliasEntry{ p, static_cast<uint>(idx++) };
        });
        return;
    }
    else [[likely]]
    {
        const auto inv_sum = 1.0 / sum;
        luisa::fiber::parallel<uint64_t>(0ull, values.size(), 1024, [&](auto begin, auto end) {
			std::transform(values.begin() + begin, values.begin() + end,
			               pdf.begin() + begin,
			               [inv_sum](float v) {
			                   return static_cast<float>(std::abs(v) * inv_sum);
			               }); }, inplace);
    }
    const auto ratio = static_cast<double>(values.size()) / sum;
    luisa::vector<uint> over;
    luisa::vector<uint> under;
    over.push_back_uninitialized(next_pow2(values.size()));
    under.push_back_uninitialized(next_pow2(values.size()));
    std::atomic_size_t over_size = 0;
    std::atomic_size_t under_size = 0;
    luisa::fiber::parallel<uint64_t>(0ull, values.size(), 1024, [&](size_t beg, size_t end) {
		for (auto i = beg; i < end; i++) {
			const auto p = static_cast<float>(values[i] * ratio);
			table[i] = {p, static_cast<uint>(i)};
			if (p > 1.f) {
				over[over_size++] = i;
			} else {
				under[under_size++] = i;
			}
		} }, inplace);
    over.resize(over_size.load());
    under.resize(under_size.load());
    while (!over.empty() && !under.empty())
    {
        auto o = over.back();
        auto u = under.back();
        over.pop_back();
        under.pop_back();
        table[o].prob -= 1.0f - table[u].prob;
        table[u].alias = o;
        if (table[o].prob > 1.0f)
        {
            over.push_back(o);
        }
        else if (table[o].prob < 1.0f)
        {
            under.push_back(o);
        }
    }
    for (auto i : over)
    {
        table[i] = { 1.0f, i };
    }
    for (auto i : under)
    {
        table[i] = { 1.0f, i };
    }
}
} // namespace detail
HDRI::HDRI()
{
    ShaderManager::instance()->load("hdri/weight.bin", _weight);
}
void HDRI::compute_scalemap(
    Device& device,
    CommandList& cmdlist,
    Image<float> const& img,
    uint2 size,
    Buffer<float>& buffer_cache,
    vstd::function<void(luisa::vector<float>&&)> callback
)
{
    const auto pixel_count = size.x * size.y;
    if (!buffer_cache)
    {
        buffer_cache = device.create_buffer<float>(pixel_count);
    }
    else if (buffer_cache.size() < pixel_count)
    {
        cmdlist.add_callback([b = std::move(buffer_cache)]() {});
        buffer_cache = device.create_buffer<float>(pixel_count);
    }
    const auto buffer = buffer_cache.view(0, pixel_count);
    luisa::vector<float> scale_map;
    scale_map.push_back_uninitialized(pixel_count);
    cmdlist
        << (*_weight)(img, buffer).dispatch(size)
        << buffer.copy_to(luisa::span(scale_map));
    cmdlist.add_callback([callback = std::move(callback), scale_map = std::move(scale_map)]() mutable {
        callback(std::move(scale_map));
    });
}
auto HDRI::compute_alias_table(luisa::span<float> scale_map, uint2 size) -> AliasTable
{
    const auto pixel_count = size.x * size.y;
    {
        std::atomic<double> sum_scale = 0;
        luisa::fiber::parallel(scale_map.begin(), scale_map.end(), 4096, [&](auto beg, auto end) {
            const double d = std::accumulate(beg, end, 0.0);
            sum_scale += d;
        });
        const auto average_scale = static_cast<double>(sum_scale.load() / pixel_count);
        luisa::fiber::parallel(scale_map.begin(), scale_map.end(), 1024, [&](auto beg, auto end) {
            std::for_each(beg, end, [average_scale](float& v) {
                v = std::max<float>(v - average_scale, 0.f);
            });
        });
    }
    luisa::vector<float> row_averages(size.y);
    luisa::vector<float> pdfs(pixel_count);
    luisa::vector<HDRI::AliasEntry> aliases(size.y + pixel_count);
    luisa::fiber::parallel<uint64_t>(0u, size.y, 8, [&](uint beg, uint end) {
        for (auto i = beg; i != end; ++i)
        {
            auto sum = 0.;
            const auto values = luisa::span{ scale_map }.subspan(
                i * size.x, size.x
            );
            for (auto v : values)
            {
                sum += v;
            }
            row_averages[i] = static_cast<float>(sum * (1.0 / size.x));
            rbc::detail::create_alias_table(
                values,
                luisa::span{ aliases }.subspan(size.y + i * size.x, size.x),
                luisa::span{ pdfs }.subspan(i * size.x, size.x), false
            );
        }
    });
    luisa::vector<float> pdf_table;
    pdf_table.push_back_uninitialized(row_averages.size());
    rbc::detail::create_alias_table(row_averages, luisa::span{ aliases }.subspan(0, size.y), pdf_table, true);
    luisa::fiber::parallel<uint64_t>(0u, size.y, 32, [&](auto beg, auto end) {
        for (auto y = beg; y < end; ++y)
        {
            const auto offset = y * size.x;
            const auto pdf_y = pdf_table[y];
            const auto scale = static_cast<float>(pdf_y * pixel_count);
            auto row = luisa::span{ pdfs }.subspan(offset, size.x);
            std::for_each(row.begin(), row.end(), [&](float& v) { v *= scale; });
        }
    });
    return { std::move(aliases), std::move(pdfs) };
}
auto HDRI::precompute(
    Image<float> const& hdr,
    Device& device,
    Stream& stream
) -> AliasTable
{
    const auto size = hdr.size();
    const auto pixel_count = size.x * size.y;
    const auto buffer = device.create_buffer<float>(pixel_count);
    luisa::vector<float> scale_map;
    scale_map.push_back_uninitialized(pixel_count);
    stream
        << (*_weight)(hdr, buffer).dispatch(size)
        << buffer.view().copy_to(luisa::span(scale_map)) << synchronize();
    return compute_alias_table(scale_map, size);
}
} // namespace rbc