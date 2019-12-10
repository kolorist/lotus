#pragma once

namespace hwcpipe
{
// ---------------------------------------------

enum class gpu_counter_e
{
	gpu_cycles = 0,
	fragment_cycles,
	tiler_cycles,

	varying_16_bits,
	varying_32_bits,

	count
};

// ---------------------------------------------
}
