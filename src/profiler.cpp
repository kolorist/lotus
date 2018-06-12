#include "lotus/profiler.h"

#include <thread/mutex.h>

#include "lotus/memory.h"

namespace lotus {

	// thread local data
	struct capture_info {
		u32										thread_id;
		c8										name[CAPTURE_NAME_LENGTH];

		// capture states
		event*									first_event;
		event*									last_event;
		u32										current_depth;

		// event allocator
		pool_allocator_t<event>*				event_allocator;
	};

	thread_local capture_info					s_capture_info;
	floral::mutex								s_init_mtx;

	void init_capture_for_this_thread(const u32 i_threadId, const_cstr i_captureName)
	{
		floral::lock_guard initGuard(s_init_mtx);

		s_capture_info.thread_id = i_threadId;
		strcpy(s_capture_info.name, i_captureName);
		s_capture_info.event_allocator = e_main_allocator.allocate_arena<pool_allocator_t<event>>(SIZE_MB(8));
		s_capture_info.first_event = nullptr;
		s_capture_info.last_event = nullptr;
		s_capture_info.current_depth = 0;
	}

	const event* begin_event(const_cstr i_name)
	{
		event* newEvent = s_capture_info.event_allocator->allocate<event>();

		// capture info
		s_capture_info.current_depth++;
		newEvent->time_stamp = 0;
		newEvent->clock_tick = 0;
		newEvent->depth = s_capture_info.current_depth;
		newEvent->next_event = nullptr;
		strcpy(newEvent->name, i_name);

		// linked-list structure
		if (!s_capture_info.first_event)
			s_capture_info.first_event = newEvent;

		if (s_capture_info.last_event)
			s_capture_info.last_event->next_event = newEvent;
		s_capture_info.last_event = newEvent;

		return newEvent;
	}

	void end_event(const event* i_event)
	{
		s_capture_info.current_depth--;
	}

}
