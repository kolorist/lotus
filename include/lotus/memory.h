#pragma once

#include <floral.h>
#include <helich.h>

namespace lotus {
	// main allocator
	typedef helich::allocator<helich::stack_scheme, helich::no_tracking_policy>	linear_allocator_t;

	extern linear_allocator_t					e_main_allocator;

	// allocators for each threads
	template <typename t_object>
	using pool_allocator_t = helich::fixed_allocator<helich::pool_scheme, sizeof(t_object), helich::no_tracking_policy>;
}
