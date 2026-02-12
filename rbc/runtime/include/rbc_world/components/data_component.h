#pragma once
#include <rbc_world/component.h>
#include <rbc_world/resource_base.h>
#include <rbc_core/base.h>
#include <luisa/vstl/common.h>
#include <rbc_world/callback_serializer.h>

namespace rbc::world {
struct RBC_RUNTIME_API DataComponent final : ComponentDerive<DataComponent> {
    DECLARE_WORLD_OBJECT_FRIEND(DataComponent)
    using DataType = vstd::variant<
        int64_t,
        double,
        luisa::string,
        bool,
        RC<Resource>>;
    enum struct EventType {
        OnAwake,
        OnDestroy,
        BeforeFrame,
        BeforeRender,
        AfterFrame,
        OnTransformChange
    };
    static constexpr size_t event_count = luisa::to_underlying(EventType::OnTransformChange) + 1;
private:
    DataComponent();
    ~DataComponent();
    vstd::HashMap<
        luisa::string,
        DataType>
        _infos;
    std::array<luisa::string, event_count> _events;
public:
    // Info API
    [[nodiscard]] DataType get_info(luisa::string_view name) const;
    void set_info(luisa::string_view name, DataType const &data);
    [[nodiscard]] bool has_info(luisa::string_view name) const;
    [[nodiscard]] bool remove_info(luisa::string_view name);
    [[nodiscard]] uint64_t info_count() const noexcept;
    void clear_infos() noexcept;

    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
    void on_awake() override;
    void on_destroy() override;
    void bind_event(
        EventType event_type,
        luisa::string_view callback_name);
    void unbind_event(EventType event_type);
};
}// namespace rbc::world

RBC_RTTI(rbc::world::DataComponent);
