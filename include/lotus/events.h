#pragma once

#include <floral.h>

#include "configs.h"
#include "memory.h"

namespace lotus {

	// this struct is copyable
	struct event {
		u64										time_stamp;
		u64										clock_tick;
		u32										depth;

		c8										name[EVENT_NAME_LENGTH];

		// linked-list structure
		event*									next_event;
	};

}
