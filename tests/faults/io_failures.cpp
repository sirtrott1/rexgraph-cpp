#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
std::atomic<unsigned long> events{};
bool fail(int fd, const char* operation) {
    const char* root = std::getenv("RFC_IO_FAULT_ROOT");
    const char* selected = std::getenv("RFC_IO_FAULT_OPERATION");
    if (!root || !selected || std::strcmp(selected, operation)) return false;
    char link[64], path[PATH_MAX];
    std::snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    const auto n = ::readlink(link, path, sizeof path - 1);
    if (n < 0) return false;
    path[n] = '\0';
    const auto length = std::strlen(root);
    if (length > static_cast<std::size_t>(n)) return false;
    if (std::strncmp(path, root, length) || (path[length] != '/' && path[length] != '\0')) return false;
    const auto* configured = std::getenv("RFC_IO_FAULT_AFTER");
    const auto at = configured ? std::strtoul(configured, nullptr, 10) : 1;
    if (events.fetch_add(1) + 1 != at) return false;
    constexpr char marker[] = "RFC injected physical I/O failure\n";
    (void)::syscall(SYS_write, STDERR_FILENO, marker, sizeof marker - 1);
    const auto* error = std::getenv("RFC_IO_FAULT_ERROR");
    errno = error && std::strcmp(error, "ENOSPC") == 0 ? ENOSPC : EIO;
    return true;
}
}
extern "C" ssize_t write(int fd, const void* bytes, size_t size) {
    if (fail(fd, "write")) return -1;
    return static_cast<ssize_t>(::syscall(SYS_write, fd, bytes, size));
}
extern "C" int fsync(int fd) {
    if (fail(fd, "fsync")) return -1;
    return static_cast<int>(::syscall(SYS_fsync, fd));
}
