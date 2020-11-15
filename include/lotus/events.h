#pragma once

#include <floral.h>

#include "configs.h"
#include "memory.h"

namespace lotus
{

struct hardware_counters_t
{
	f32											gpu_cycles;
	f32											fragment_cycles;
	f32											tiler_cycles;
	f32											frag_elim;

	f32											shader_texture_cycles;
	f32											varying_16_bits;
	f32											varying_32_bits;

	f32											external_memory_read_bytes;
	f32											external_memory_write_bytes;
};

struct hardware_counters_buffer_t
{
	f32*										gpu_cycles;
	f32*										fragment_cycles;
	f32*										tiler_cycles;
	f32*										frag_elim;

	f32*										shader_texture_cycles;
	f32*										varying_16_bits;
	f32*										varying_32_bits;

	f32*										external_memory_read_bytes;
	f32*										external_memory_write_bytes;
};

// this struct is copyable
struct event {
	u64										time_stamp;
	u64										duration_ticks;
	f64										duration_ms;
	u32										depth;
	//const_cstr								name;
	c8										name[CAPTURE_NAME_LENGTH];

	sidx									widx;
};

// this struct is copyable
// used by users to store profile events for processing
struct unpacked_event {
	u64										time_stamp;
	u64										duration_ticks;
	f64										duration_ms;
	u32										depth;
	//const_cstr								name;
	c8										name[CAPTURE_NAME_LENGTH];

	bool									ready;
};

struct unpacked_capture {
	u32										thread_id;
	c8										name[CAPTURE_NAME_LENGTH];

	floral::inplace_array<unpacked_event, SAVED_EVENTS_COUNT>	events;
};

}
