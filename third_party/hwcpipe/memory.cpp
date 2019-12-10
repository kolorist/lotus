#include "memory.h"

namespace hwcpipe
{

static alloc_func_t s_allocFunc = nullptr;
static free_func_t s_freeFunc = nullptr;

void set_allocators(alloc_func_t i_allocFunc, free_func_t i_freeFunc)
{
	s_allocFunc = i_allocFunc;
	s_freeFunc = i_freeFunc;
}

namespace memory
{

void* allocate(size_t i_size)
{
	return s_allocFunc(i_size);
}

void free(void* i_ptr)
{
	return s_freeFunc(i_ptr);
}

}

}
