#pragma once

namespace hwcpipe
{
// ---------------------------------------------

enum class gpu_counter_e
{
	gpu_cycles = 0,
	fragment_cycles,
	tiler_cycles,
	frag_elim,
	tiles,

	shader_cycles,
	shader_arithmetic_cycles,
	shader_texture_cycles,
	varying_16_bits,
	varying_32_bits,

	external_memory_read_bytes,
	external_memory_write_bytes,

	count
};

// ---------------------------------------------
}
