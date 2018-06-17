#pragma once

#include <floral.h>

#include "configs.h"
#include "memory.h"

namespace lotus {

	// this struct is copyable
	struct event {
		u64										time_stamp;
		f64										duration_ms;
		u32										depth;

		const_cstr								name;

		// linked-list structure
		event*									next_event;
	};

	// this struct is copyable
	// used by users to store profile events for processing
	struct unpacked_event {
		u64										time_stamp;
		f64										duration_ms;
		u32										depth;
		const_cstr								name;
	};

	struct unpacked_capture {
		u32										thread_id;
		c8										name[CAPTURE_NAME_LENGTH];

		floral::inplace_array<unpacked_event, SAVED_EVENTS_COUNT>	events;
	};

}
