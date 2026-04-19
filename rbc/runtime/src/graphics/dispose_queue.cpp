#include <rbc_graphics/dispose_queue.h>
namespace rbc {
void DisposeQueue::_destroy_elements(vstd::vector<Element> const &elements) {
    for (auto &&i : elements) {
        i.dtor(i.ptr);
        vengine_free(i.ptr);
    }
}
void DisposeQueue::on_frame_end(Stream &stream) {
    if (_elements.empty()) return;
    stream << [e = std::move(_elements)]() {
        _destroy_elements(e);
    };
}
void DisposeQueue::force_clear() {
    _destroy_elements(_elements);
    _elements.clear();
}

void DisposeQueue::on_frame_end(CommandList &cmdlist) {
    if (_elements.empty()) return;
    cmdlist.add_callback([e = std::move(_elements)]() {
        _destroy_elements(e);
    });
}
DisposeQueue::~DisposeQueue() {
    force_clear();
}
}// namespace rbc