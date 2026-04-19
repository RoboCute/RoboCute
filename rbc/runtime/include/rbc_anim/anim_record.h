#pragma once

namespace rbc {

struct DeltaTimeRecord {
public:
    DeltaTimeRecord() = default;
    explicit DeltaTimeRecord(float delta_time)
        : _delta(delta_time) {
    }

public:
    [[nodiscard]] float delta() const { return _delta; }
    void set(float previous_time, float delta_time);
    void set_previous(float previous_time);
    [[nodiscard]] bool is_previous_valid() const { return _is_previous_valid; }

private:
    float _delta = 0.f;
    float _previous_time = 0.f;
    bool _is_previous_valid = false;
};

}// namespace rbc