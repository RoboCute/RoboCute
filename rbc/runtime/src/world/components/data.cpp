#include <rbc_world/components/data_component.h>
#include <rbc_world/type_register.h>

namespace rbc::world {
DataComponent::DataComponent() {}
DataComponent::~DataComponent() {}

// Info API
auto DataComponent::get_info(luisa::string_view name) const -> DataType {
    auto iter = _infos.find(name);
    if (!~iter) {
        return DataType{};
    }
    return iter.value();
}

void DataComponent::set_info(luisa::string_view name, DataType const &data) {
    _infos.try_emplace(luisa::string{name}, data);
}

bool DataComponent::has_info(luisa::string_view name) const {
    return _infos.find(name);
}

bool DataComponent::remove_info(luisa::string_view name) {
    auto iter = _infos.find(name);
    if (iter) {
        _infos.remove(iter);
        return true;
    }
    return false;
}

uint64_t DataComponent::info_count() const noexcept {
    return _infos.size();
}

void DataComponent::clear_infos() noexcept {
    _infos.clear();
}

void DataComponent::serialize_meta(ObjSerialize const &obj) const {
    // Serialize infos
    obj.ar.start_array();
    for (auto &pair : _infos) {
        obj.ar.value(pair.first);
        obj.ar.value(pair.second.index());
        pair.second.visit([&]<typename T>(T const &t) {
            if constexpr (std::is_same_v<T, RC<Resource>>) {
                vstd::Guid null_guid;
                null_guid.reset();
                obj.ar.value(t ? t->guid() : null_guid);
            } else {
                obj.ar.value(t);
            }
        });
    }
    obj.ar.end_array("infos");
    // Serialize events
    obj.ar.start_array();
    for (auto &event : _events) {
        obj.ar.value(event);
    }
    obj.ar.end_array("events");
}

void DataComponent::deserialize_meta(ObjDeSerialize const &obj) {
    // Deserialize infos
    uint64_t info_count;
    if (obj.ar.start_array(info_count, "infos")) {
        auto elem_size = info_count / 3;
        _infos.reserve(elem_size);
        for (auto i : vstd::range(elem_size)) {
            luisa::string key;
            uint64_t index;
            if (!obj.ar.value(key)) break;
            if (!obj.ar.value(index)) break;
            auto &v = _infos.emplace(std::move(key)).value();
            v.reset_as(index);
            v.visit([&]<typename T>(T &t) {
                if constexpr (std::is_same_v<T, RC<Resource>>) {
                    vstd::Guid guid;
                    if (obj.ar.value(guid)) {
                        t = load_resource(guid);
                    }
                } else {
                    obj.ar.value(t);
                }
            });
        }
        obj.ar.end_scope();
    }
    // Deserialize events
    uint64_t event_count;
    if (obj.ar.start_array(event_count, "events") && event_count == _events.size()) {
        for (auto i : vstd::range(event_count)) {
            if (!obj.ar.value(_events[i])) break;
        }
        obj.ar.end_scope();
    }
}
void DataComponent::bind_event(
    EventType event_type,
    luisa::string_view callback_name) {
    if (callback_name.empty()) {
        unbind_event(event_type);
        return;
    }
    auto idx = luisa::to_underlying(event_type);
    _events[idx] = luisa::string{callback_name};
    switch (event_type) {
        case EventType::OnAwake: {
            if (enabled()) {
                auto func_ptr = get_callback(callback_name);
                if (func_ptr) {
                    (*func_ptr)(this);
                }
            }
            break;
        }
        case EventType::BeforeFrame: {
            add_world_event(WorldEventType::BeforeFrame, [this](luisa::string name) -> rbc::coroutine {
                while (true) {
                    {
                        auto func_ptr = get_callback(name);
                        if (!func_ptr) co_return;
                        (*func_ptr)(this);
                    }
                    co_await std::suspend_always{};
                }
            }(_events[idx]));
            break;
        }
        case EventType::BeforeRender: {
            add_world_event(WorldEventType::BeforeRender, [this](luisa::string name) -> rbc::coroutine {
                while (true) {
                    {
                        auto func_ptr = get_callback(name);
                        if (!func_ptr) co_return;
                        (*func_ptr)(this);
                    }
                    co_await std::suspend_always{};
                }
            }(_events[idx]));
            break;
        }
        case EventType::AfterFrame: {
            add_world_event(WorldEventType::AfterFrame, [this](luisa::string name) -> rbc::coroutine {
                while (true) {
                    {
                        auto func_ptr = get_callback(name);
                        if (!func_ptr) co_return;
                        (*func_ptr)(this);
                    }
                    co_await std::suspend_always{};
                }
            }(_events[idx]));
            break;
        }
        default:
            break;
    }
}
void DataComponent::unbind_event(
    EventType event_type) {
    auto idx = luisa::to_underlying(event_type);
    _events[idx].clear();

    // Remove world event for event types that use them
    switch (event_type) {
        case EventType::BeforeFrame:
            remove_world_event(WorldEventType::BeforeFrame);
            break;
        case EventType::BeforeRender:
            remove_world_event(WorldEventType::BeforeRender);
            break;
        case EventType::AfterFrame:
            remove_world_event(WorldEventType::AfterFrame);
            break;
        default:
            break;
    }
}
void DataComponent::on_awake() {
    // Call OnAwake event
    auto &on_awake_name = _events[luisa::to_underlying(EventType::OnAwake)];
    if (!on_awake_name.empty()) {
        auto func_ptr = get_callback(on_awake_name);
        if (func_ptr) {
            (*func_ptr)(this);
        }
    }

    // BeforeFrame event
    auto &before_frame_name = _events[luisa::to_underlying(EventType::BeforeFrame)];
    if (!before_frame_name.empty()) {
        add_world_event(WorldEventType::BeforeFrame, [this](luisa::string name) -> rbc::coroutine {
            while (true) {
                {
                    auto func_ptr = get_callback(name);
                    if (!func_ptr) co_return;
                    (*func_ptr)(this);
                }
                co_await std::suspend_always{};
            }
        }(before_frame_name));
    }
    // BeforeRender event
    auto &before_render_name = _events[luisa::to_underlying(EventType::BeforeRender)];
    if (!before_render_name.empty()) {
        add_world_event(WorldEventType::BeforeRender, [this](luisa::string name) -> rbc::coroutine {
            while (true) {
                {
                    auto func_ptr = get_callback(name);
                    if (!func_ptr) co_return;
                    (*func_ptr)(this);
                }
                co_await std::suspend_always{};
            }
        }(before_render_name));
    }
    // AfterFrame event
    auto &after_frame_name = _events[luisa::to_underlying(EventType::AfterFrame)];
    if (!after_frame_name.empty()) {
        add_world_event(WorldEventType::AfterFrame, [this](luisa::string name) -> rbc::coroutine {
            while (true) {
                {
                    auto func_ptr = get_callback(name);
                    if (!func_ptr) co_return;
                    (*func_ptr)(this);
                }
                co_await std::suspend_always{};
            }
        }(after_frame_name));
    }
}
void DataComponent::on_destroy() {
    // Call OnDestroy event
    auto &on_destroy_name = _events[luisa::to_underlying(EventType::OnDestroy)];
    if (!on_destroy_name.empty()) {
        auto func_ptr = get_callback(on_destroy_name);
        if (func_ptr) {
            (*func_ptr)(this);
        }
    }
}
DECLARE_WORLD_OBJECT_REGISTER(DataComponent)
}// namespace rbc::world
