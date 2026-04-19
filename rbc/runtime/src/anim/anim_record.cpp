#include "rbc_anim/anim_record.h"

namespace rbc {

void DeltaTimeRecord::set(float previous_time, float delta_time) {
    _previous_time = previous_time;
    _delta = delta_time;
    _is_previous_valid = true;
}

void DeltaTimeRecord::set_previous(float previous_time) {
    _previous_time = previous_time;
    _is_previous_valid = true;
}

}// namespace rbc