#pragma once

#include <floral.h>

#include "events.h"
#include "lotus/detail/profiler.h"

namespace lotus {

	void										init_capture_for_this_thread(const u32 i_threadId, const_cstr i_captureName);
	void										stop_capture_for_this_thread();
	void										init_hardware_counters();
	void										stop_hardware_counters();
	void										begin_capture(const u64 i_captureIdx);
	void										end_capture(const u64 i_captureIdx);

	void										capture_counters_into(hardware_counters_t& o_counters);
	void										capture_and_fill_counters_into(hardware_counters_buffer_t& o_buffer, const size i_offset);

	template <typename t_allocator>
	void										unpack_capture(floral::fixed_array<unpacked_event, t_allocator>& o_unpackedEvents, const sidx i_captureIdx);
	template <typename t_allocator>
	void										unpack_capture(floral::fast_fixed_array<unpacked_event, t_allocator>& o_unpackedEvents, const sidx i_captureIdx);
	template <typename t_allocator, u32 t_capacity>
	void										unpack_capture(floral::ring_buffer_st<unpacked_event, t_allocator, t_capacity>& o_unpackedEvents, const sidx i_captureIdx);
	template <typename t_allocator, u32 t_capacity>
	void										unpack_capture(floral::fast_ring_buffer_st<unpacked_event, t_allocator, t_capacity>& o_unpackedEvents, const sidx i_captureIdx);

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
	static lotus::event *lotus_pevent_this_scope = lotus::allocate_event();			\
	lotus::profile_scope lotus_scope_this_scope(lotus_pevent_this_scope, ScopeName)
}

#include "profiler.hpp"
