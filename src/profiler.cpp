#include "lotus/profiler.h"

#include "lotus/memory.h"

#include <floral/thread/mutex.h>
#if defined(PLATFORM_WINDOWS)
#include <Windows.h>
#endif

#include <hwcpipe/memory.h>
#include <hwcpipe/hwcpipe.h>

namespace lotus
{

namespace detail
{
	unpacked_event_buffer_t						s_unpacked_event_buffers[THREADS_CAP];
	thread_local capture_info					s_capture_info;
}

static sidx										s_threads_count = 0;
static floral::mutex							s_init_mtx;
static bool										s_hardware_counter_ready = false;
static freelist_arena_t*						s_hwcArena = nullptr;

static void* hwcpipe_alloc(size_t i_size)
{
	return s_hwcArena->allocate(i_size);
}

static void hwcpipe_free(void* i_ptr)
{
	s_hwcArena->free(i_ptr);
}

void init_capture_for_this_thread(const u32 i_threadId, const_cstr i_captureName)
{
	floral::lock_guard initGuard(s_init_mtx);
	// meta data
	detail::s_capture_info.event_buffer_idx = s_threads_count;
	s_threads_count++;
	detail::s_capture_info.thread_id = i_threadId;
	strcpy(detail::s_capture_info.name, i_captureName);
	detail::s_capture_info.event_allocator = e_main_allocator.allocate_arena<pool_allocator_t<event>>(SIZE_MB(8));
	detail::s_capture_info.current_depth = 0;

	// event buffer
	detail::unpacked_event_buffer_t& eventBuffer = detail::s_unpacked_event_buffers[detail::s_capture_info.event_buffer_idx];
	eventBuffer.data = e_main_allocator.allocate_array<unpacked_event>(EVENTS_CAP);
	eventBuffer.ridx = 0;
	eventBuffer.widx = 0;
	
#if defined(PLATFORM_WINDOWS)
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	detail::s_capture_info.thread_frequency = freq.QuadPart;
#else
	detail::s_capture_info.thread_frequency = 0;
#endif
}

void stop_capture_for_this_thread()
{
	floral::lock_guard initGuard(s_init_mtx);
	// event buffer
	detail::unpacked_event_buffer_t& eventBuffer = detail::s_unpacked_event_buffers[detail::s_capture_info.event_buffer_idx];
	e_main_allocator.free(eventBuffer.data);
	eventBuffer.data = nullptr;
	eventBuffer.ridx = 0;
	eventBuffer.widx = 0;
	
	detail::s_capture_info.event_buffer_idx = 0;
	s_threads_count--;
	detail::s_capture_info.thread_id = 0;
	strcpy(detail::s_capture_info.name, "<invalid>");
	e_main_allocator.free(detail::s_capture_info.event_allocator);
	detail::s_capture_info.current_depth = 0;
}

void init_hardware_counters()
{
#if defined(PLATFORM_POSIX)
	floral::lock_guard initGuard(s_init_mtx);
	if (s_hardware_counter_ready)
	{
		return;
	}
	s_hwcArena = e_main_allocator.allocate_arena<freelist_arena_t>(SIZE_MB(1));
	hwcpipe::gpu_counter_e enabledGpuCounters[] = {
		hwcpipe::gpu_counter_e::gpu_cycles,
		hwcpipe::gpu_counter_e::fragment_cycles,
		hwcpipe::gpu_counter_e::tiler_cycles,

		hwcpipe::gpu_counter_e::varying_16_bits,
		hwcpipe::gpu_counter_e::varying_32_bits,
	};
	hwcpipe::set_allocators(&hwcpipe_alloc, &hwcpipe_free);
	hwcpipe::initialize_gpu_counters(enabledGpuCounters, sizeof(enabledGpuCounters) / sizeof(hwcpipe::gpu_counter_e));
	hwcpipe::start();
	s_hardware_counter_ready = true;
#endif
}

void stop_hardware_counters()
{
#if defined(PLATFORM_POSIX)
	floral::lock_guard initGuard(s_init_mtx);
	hwcpipe::stop();
	s_hardware_counter_ready = false;
#endif
}

void capture_counters_into(hardware_counters_t& o_counters)
{
	hwcpipe::sample();
	o_counters.gpu_cycles = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::gpu_cycles);
	o_counters.fragment_cycles = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::fragment_cycles);
	o_counters.tiler_cycles = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::tiler_cycles);

	o_counters.varying_16_bits = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::varying_16_bits);
	o_counters.varying_32_bits = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::varying_32_bits);
}


void capture_and_fill_counters_into(hardware_counters_buffer_t& o_buffer, const size i_offset)
{
	hwcpipe::sample();
	o_buffer.gpu_cycles[i_offset] = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::gpu_cycles);
	o_buffer.fragment_cycles[i_offset] = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::fragment_cycles);
	o_buffer.tiler_cycles[i_offset] = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::tiler_cycles);

	o_buffer.varying_16_bits[i_offset] = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::varying_16_bits);
	o_buffer.varying_32_bits[i_offset] = (f32)hwcpipe::get_counter_value(hwcpipe::gpu_counter_e::varying_32_bits);
}

event* allocate_event() {
	event* newEvent = detail::s_capture_info.event_allocator->allocate<event>();
	return newEvent;
}

const sidx _reserve_unpacked_event() {
	detail::unpacked_event_buffer_t& eb = detail::s_unpacked_event_buffers[detail::s_capture_info.event_buffer_idx];
	floral::lock_guard captureGuard(eb.mtx);
	sidx nextWriteIdx = (eb.widx + 1) % EVENTS_CAP;
	if (nextWriteIdx != eb.ridx) {
		sidx reserveIdx = eb.widx;
		eb.data[reserveIdx].ready = false;

		eb.widx = nextWriteIdx;
		return reserveIdx;
	}
	return -1;
}

void begin_event(event* i_event, const_cstr i_name)
{
	sidx widx = _reserve_unpacked_event();
	if (widx >= 0) {
		detail::s_capture_info.current_depth++;
#if defined(PLATFORM_WINDOWS)
		LARGE_INTEGER tp;
		QueryPerformanceCounter(&tp);
		i_event->time_stamp = tp.QuadPart;
#else
		i_event->time_stamp = 0;
#endif
		i_event->depth = detail::s_capture_info.current_depth;
		i_event->name = i_name;
		i_event->widx = widx;
	}
}

void end_event(event* i_event)
{
	if (i_event->widx >= 0) {
#if defined(PLATFORM_WINDOWS)			
		LARGE_INTEGER tp;
		QueryPerformanceCounter(&tp);
		i_event->duration_ms = (f64)(tp.QuadPart - i_event->time_stamp) * 1000 / (f64)detail::s_capture_info.thread_frequency;
#else
		i_event->duration_ms = 0;
#endif
		detail::s_capture_info.current_depth--;

		detail::unpacked_event_buffer_t& eb = detail::s_unpacked_event_buffers[detail::s_capture_info.event_buffer_idx];
		unpacked_event& eve = eb.data[i_event->widx];
		eve.time_stamp = i_event->time_stamp;
		eve.duration_ms = i_event->duration_ms;
		eve.depth = i_event->depth;
		eve.name = i_event->name;
		eve.ready = true;
	}
}

// -----------------------------------------
profile_scope::profile_scope(event* i_event, const_cstr i_name)
	: pevent(i_event)
{
	begin_event(pevent, i_name);
}

profile_scope::~profile_scope()
{
	end_event(pevent);
}

}
