#pragma once
#include "product_measure.hpp"
#include <cstdlib>
#include <new>
// Executable-local counters. Paired overrides are deliberately not inlined.
#if defined(__GNUC__) || defined(__clang__)
#define RFC_PRODUCT_NOINLINE __attribute__((noinline))
#else
#define RFC_PRODUCT_NOINLINE
#endif
RFC_PRODUCT_NOINLINE void* operator new(std::size_t n) {
    if(void* p=std::malloc(n?n:1)) {if(product_allocations::active){++product_allocations::count;product_allocations::bytes+=n;}return p;}
    throw std::bad_alloc();
}
RFC_PRODUCT_NOINLINE void* operator new[](std::size_t n){return ::operator new(n);}
RFC_PRODUCT_NOINLINE void operator delete(void* p) noexcept{std::free(p);}
RFC_PRODUCT_NOINLINE void operator delete[](void* p) noexcept{std::free(p);}
RFC_PRODUCT_NOINLINE void operator delete(void* p,std::size_t) noexcept{std::free(p);}
RFC_PRODUCT_NOINLINE void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#undef RFC_PRODUCT_NOINLINE
