#include <rbc_graphics/dispose_queue.h>
namespace rbc {
void DisposeQueue::on_frame_end(Stream& stream) {
	if (_elements.empty()) return;
	stream << [e = std::move(_elements)]() {
		for (auto&& i : e) {
			i.dtor(i.ptr);
			vengine_free(i.ptr);
		}
	};
}
void  DisposeQueue::force_clear() {
	for (auto&& i : _elements) {
		i.dtor(i.ptr);
		vengine_free(i.ptr);
	}
	_elements.clear();
}

void DisposeQueue::on_frame_end(CommandList& cmdlist) {
	if (_elements.empty()) return;
	cmdlist.add_callback([e = std::move(_elements)]() {
		for (auto&& i : e) {
			i.dtor(i.ptr);
			vengine_free(i.ptr);
		}
	});
}
}// namespace rbc