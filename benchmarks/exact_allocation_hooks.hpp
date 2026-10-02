#pragma once

#include <cstdlib>
#include <new>

namespace {
struct Counts {
    std::size_t cpp_calls{}, cpp_bytes{}, gmp_calls{}, gmp_bytes{}, gmp_reallocations{},
        gmp_reallocation_bytes{};
};
Counts counts;
bool accounting = false;
void record_cpp(std::size_t size) {
    if (accounting) {
        ++counts.cpp_calls;
        counts.cpp_bytes += size;
    }
}
// Keep the replacement allocator's release hook out of line. GCC otherwise
// diagnoses the valid malloc-backed replacement delete after inlining it into
// standard-library containers as a mismatched new/free pair.
[[gnu::noinline]] void release_cpp(void *p) noexcept {
    std::free(p);
}
void *gmp_allocate(std::size_t size) {
    if (accounting) {
        ++counts.gmp_calls;
        counts.gmp_bytes += size;
    }
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    std::abort();
}
void *gmp_reallocate(void *p, std::size_t, std::size_t size) {
    if (accounting) {
        ++counts.gmp_reallocations;
        counts.gmp_reallocation_bytes += size;
    }
    if (auto *q = std::realloc(p, size ? size : 1))
        return q;
    std::abort();
}
void gmp_free(void *p, std::size_t) {
    std::free(p);
}
} // namespace
void *operator new(std::size_t size) {
    record_cpp(size);
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void *p) noexcept {
    release_cpp(p);
}
void operator delete[](void *p) noexcept {
    release_cpp(p);
}
void operator delete(void *p, std::size_t) noexcept {
    release_cpp(p);
}
void operator delete[](void *p, std::size_t) noexcept {
    release_cpp(p);
}
void *operator new(std::size_t size, std::align_val_t alignment) {
    record_cpp(size);
    void *p = nullptr;
    if (posix_memalign(&p, static_cast<std::size_t>(alignment), size ? size : 1) == 0)
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}
void operator delete(void *p, std::align_val_t) noexcept {
    release_cpp(p);
}
void operator delete[](void *p, std::align_val_t) noexcept {
    release_cpp(p);
}
void operator delete(void *p, std::size_t, std::align_val_t) noexcept {
    release_cpp(p);
}
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept {
    release_cpp(p);
}
