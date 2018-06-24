#include "lotus/profiler.h"

#include <thread/mutex.h>
#include <Windows.h>

#include "lotus/memory.h"

namespace lotus {

namespace detail {
	unpacked_event_buffer_t						s_unpacked_event_buffers[THREADS_CAP];
	thread_local capture_info					s_capture_info;
}

	static sidx									s_threads_count = 0;
	static floral::mutex						s_init_mtx;

	void init_capture_for_this_thread(const u32 i_threadId, const_cstr i_captureName)
	{
		floral::lock_guard initGuard(s_init_mtx);
		// meta data
		detail::s_capture_info.event_buffer_idx = s_threads_count;
		s_threads_count++;
		detail::s_capture_info.thread_id = i_threadId;
		strcpy(detail::s_capture_info.name, i_captureName);
		detail::s_capture_info.event_allocator = e_main_allocator.allocate_arena<pool_allocator_t<event>>(SIZE_MB(8));
		detail::s_capture_info.current_depth = 0;

		// event buffer
		detail::unpacked_event_buffer_t& eventBuffer = detail::s_unpacked_event_buffers[detail::s_capture_info.event_buffer_idx];
		eventBuffer.data = e_main_allocator.allocate_array<unpacked_event>(EVENTS_CAP);
		eventBuffer.ridx = 0;
		eventBuffer.widx = 0;

		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		detail::s_capture_info.thread_frequency = freq.QuadPart;
	}

	event* allocate_event() {
		event* newEvent = detail::s_capture_info.event_allocator->allocate<event>();
		return newEvent;
	}

	const sidx _reserve_unpacked_event() {
		detail::unpacked_event_buffer_t& eb = detail::s_unpacked_event_buffers[detail::s_capture_info.event_buffer_idx];
		floral::lock_guard captureGuard(eb.mtx);
		sidx nextWriteIdx = (eb.widx + 1) % EVENTS_CAP;
		if (nextWriteIdx != eb.ridx) {
			sidx reserveIdx = eb.widx;
			eb.data[reserveIdx].ready = false;

			eb.widx = nextWriteIdx;
			return reserveIdx;
		}
		return -1;
	}

	void begin_event(event* i_event, const_cstr i_name)
	{
		sidx widx = _reserve_unpacked_event();
		if (widx >= 0) {
			LARGE_INTEGER tp;
			QueryPerformanceCounter(&tp);

			detail::s_capture_info.current_depth++;

			i_event->time_stamp = tp.QuadPart;
			i_event->depth = detail::s_capture_info.current_depth;
			i_event->name = i_name;
			i_event->widx = widx;
		}
	}

	void end_event(event* i_event)
	{
		if (i_event->widx >= 0) {
			LARGE_INTEGER tp;
			QueryPerformanceCounter(&tp);

			i_event->duration_ms = (f64)(tp.QuadPart - i_event->time_stamp) * 1000 / (f64)detail::s_capture_info.thread_frequency;
			detail::s_capture_info.current_depth--;

			detail::unpacked_event_buffer_t& eb = detail::s_unpacked_event_buffers[detail::s_capture_info.event_buffer_idx];
			unpacked_event& eve = eb.data[i_event->widx];
			eve.time_stamp = i_event->time_stamp;
			eve.duration_ms = i_event->duration_ms;
			eve.depth = i_event->depth;
			eve.name = i_event->name;
			eve.ready = true;
		}
	}

	// -----------------------------------------
	profile_scope::profile_scope(event* i_event, const_cstr i_name)
		: pevent(i_event)
	{
		begin_event(pevent, i_name);
	}

	profile_scope::~profile_scope()
	{
		end_event(pevent);
	}

}
