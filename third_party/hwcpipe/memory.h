#pragma once

#include <cstdint>

namespace hwcpipe
{

typedef void* (*alloc_func_t)(size_t i_size);
typedef void (*free_func_t)(void* i_ptr);

void											set_allocators(alloc_func_t i_allocFunc, free_func_t i_freeFunc);

namespace memory
{
void*											allocate(size_t i_size);
void											free(void* i_ptr);
}

}
