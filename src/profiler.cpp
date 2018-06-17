#include "lotus/profiler.h"

#include <thread/mutex.h>

#include "lotus/memory.h"

#include <Windows.h>

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

	floral::inplace_mpsc_queue_t<unpacked_event, MAX_EVENTS_CAP>	s_events_queue;

	u64											s_frequency;	// ticks per second

	void init_capture_for_this_thread(const u32 i_threadId, const_cstr i_captureName)
	{
		floral::lock_guard initGuard(s_init_mtx);

		s_capture_info.thread_id = i_threadId;
		strcpy(s_capture_info.name, i_captureName);
		s_capture_info.event_allocator = e_main_allocator.allocate_arena<pool_allocator_t<event>>(SIZE_MB(8));
		s_capture_info.first_event = nullptr;
		s_capture_info.last_event = nullptr;
		s_capture_info.current_depth = 0;

		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		s_frequency = freq.QuadPart;
	}

	event* allocate_event() {
		event* newEvent = s_capture_info.event_allocator->allocate<event>();
		return newEvent;
	}

	void begin_event(event* i_event, const_cstr i_name)
	{
		LARGE_INTEGER tp;
		QueryPerformanceCounter(&tp);
		// capture info
		s_capture_info.current_depth++;

		i_event->time_stamp = tp.QuadPart;
		i_event->depth = s_capture_info.current_depth;
		i_event->next_event = nullptr;
		i_event->name = i_name;

		// linked-list structure
		if (!s_capture_info.first_event)
			s_capture_info.first_event = i_event;

		if (s_capture_info.last_event)
			s_capture_info.last_event->next_event = i_event;
		s_capture_info.last_event = i_event;
	}

	void end_event(event* i_event)
	{
		LARGE_INTEGER tp;
		QueryPerformanceCounter(&tp);

		i_event->duration_ms = (f64)(tp.QuadPart - i_event->time_stamp) * 1000 / (f64)s_frequency;
		s_capture_info.current_depth--;

		unpacked_event eve;
		eve.time_stamp = i_event->time_stamp;
		eve.duration_ms = i_event->duration_ms;
		eve.depth = i_event->depth;
		eve.name = i_event->name;
		s_events_queue.push(eve);
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

	// -----------------------------------------
	void __debug_event_queue_print()
	{
		unpacked_event eve;
		printf("unpacked events: \n");
		while (s_events_queue.try_pop_into(eve)) {
			printf("%s (dur: %f)\n",
					eve.name,
					eve.duration_ms);
		}
		printf("\n");
	}

}
