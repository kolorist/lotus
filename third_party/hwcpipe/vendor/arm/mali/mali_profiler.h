#pragma once

#include "gpu_profiler.h"

#include <cstdint>

namespace hwcpipe
{
// ---------------------------------------------
const bool										initialize_mali_profiler(gpu_counter_e* i_enabledCounters, const size_t i_numCounters);
void											start_mali_profiler();
void											stop_mali_profiler();
void											sample_mali_profiler();
// ---------------------------------------------
}
