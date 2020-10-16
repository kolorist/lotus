#pragma once

#include "gpu_profiler.h"

#include <cstdint>

namespace hwcpipe
{
// ---------------------------------------------

const bool										initialize_gpu_counters(gpu_counter_e* i_enabledCounters, const size_t i_numCounters);
void											start();
void											stop();
void											sample();

uint64_t										get_counter_value(const gpu_counter_e i_counter);

// ---------------------------------------------
}
