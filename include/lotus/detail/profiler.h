#pragma once

#include <floral.h>

#include "lotus/configs.h"
#include "lotus/memory.h"
#include "lotus/events.h"

namespace lotus {
namespace detail {

	struct unpacked_event_buffer_t {
		floral::mutex							mtx;
		unpacked_event*							data;
		sidx									ridx, widx;
	};

	extern unpacked_event_buffer_t				s_unpacked_event_buffers[THREADS_CAP];

	// thread local data
	struct capture_info {
		u32										thread_id;
		c8										name[CAPTURE_NAME_LENGTH];

		u32										current_depth;
		sidx									event_buffer_idx;

		u64										thread_frequency;
		pool_allocator_t<event>*				event_allocator;
	};

	extern thread_local capture_info			s_capture_info;

}
}
