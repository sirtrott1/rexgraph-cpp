#pragma once
#include <atomic>
#include <cstdlib>
#include <new>
namespace {
std::atomic_bool accounting{};
std::atomic_size_t cpp_calls{},cpp_bytes{};
void record_cpp(std::size_t size) {
    if(accounting.load(std::memory_order_relaxed)) {
        cpp_calls.fetch_add(1,std::memory_order_relaxed);
        cpp_bytes.fetch_add(size,std::memory_order_relaxed);
    }
}
[[gnu::noinline]] void release_cpp(void* p) noexcept { std::free(p); }
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
