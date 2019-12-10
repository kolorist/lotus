#include "hwcpipe.h"

#include "vendor/arm/mali/mali_profiler.h"

namespace hwcpipe
{

void initialize_gpu_counters(gpu_counter_e* i_enabledCounters, const size_t i_numCounters)
{
	initialize_mali_profiler(i_enabledCounters, i_numCounters);
}

void start()
{
	start_mali_profiler();
}

void stop()
{
	stop_mali_profiler();
}

void sample()
{
	sample_mali_profiler();
}

}
