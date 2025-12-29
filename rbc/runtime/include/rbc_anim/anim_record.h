#pragma once

namespace rbc {

struct DeltaTimeRecord {
public:
    float delta = 0.f;
    DeltaTimeRecord() = default;
    explicit DeltaTimeRecord(float InDeltaTime)
        : delta(InDeltaTime) {
    }

public:
    void Set(float InPrevious, float InDelta);
    void SetPrevious(float InPrevious);
    [[nodiscard]] bool IsPreviousValid() const { return is_previous_valid; }

private:
    float previous_time = 0.f;
    bool is_previous_valid = false;
};

}// namespace rbc