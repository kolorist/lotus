#pragma once

#include <floral.h>

#include "events.h"
#include "lotus/detail/profiler.h"

namespace lotus {

	void										init_capture_for_this_thread(const u32 i_threadId, const_cstr i_captureName);
	void										begin_capture(const u64 i_captureIdx);
	void										end_capture(const u64 i_captureIdx);

	template <typename allocator_t>
	void										unpack_capture(floral::fixed_array<unpacked_event, allocator_t>& o_unpackedEvents, const sidx i_captureIdx);

	event*										allocate_event();
	void										begin_event(event* i_event, const_cstr i_name);
	void										end_event(event* i_event);

	// -----------------------------------------
	struct profile_scope {
		profile_scope(event* i_event, const_cstr i_name);
		~profile_scope();

		event*									pevent;
	};
	
#define PROFILE_SCOPE(ScopeName)														\
	static lotus::event *lotus_pevent_##ScopeName = lotus::allocate_event();			\
	lotus::profile_scope lotus_scope_##ScopeName(lotus_pevent_##ScopeName, #ScopeName)
}

#include "profiler.hpp"
