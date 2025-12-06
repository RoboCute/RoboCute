#include <rbc_render/click_manager.h>

namespace rbc {
void ClickManager::clear_requires() {
    std::lock_guard lck{_mtx};
    _requires.clear();
    _frame_selection_requires.clear();
}
void ClickManager::clear() {
    std::lock_guard lck{_mtx};
    _requires.clear();
    _results.clear();
    _frame_selection_requires.clear();
    _frame_selection_results.clear();
}
}// namespace rbc