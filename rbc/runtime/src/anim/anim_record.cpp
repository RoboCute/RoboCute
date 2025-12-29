#include "rbc_anim/anim_record.h"

namespace rbc {

void DeltaTimeRecord::Set(float InPrevious, float InDelta) {
    previous_time = InPrevious;
    delta = InDelta;
    is_previous_valid = true;
}

void DeltaTimeRecord::SetPrevious(float InPrevious) {
    previous_time = InPrevious;
    is_previous_valid = true;
}

}// namespace rbc