#pragma once

// Opt-in Linux storage adapter. Never included by the mathematical umbrellas.
#if !defined(__linux__)
#error "The durable native store currently requires Linux"
#endif

#include "rfc/storage/file_io.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

namespace rfc::storage::linux_durable_detail {

[[noreturn]] inline void system_error(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}
class Descriptor {
public:
    explicit Descriptor(int value = -1) : value_(value) {}
    ~Descriptor() { if (value_ >= 0) ::close(value_); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
    Descriptor& operator=(Descriptor&& other) noexcept {
        if (this != &other) {
            if (value_ >= 0) ::close(value_);
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    int get() const { return value_; }
    void close_checked() {
        const int old = std::exchange(value_, -1);
        // Linux releases the descriptor even on close errors; do not retry it.
        if (::close(old) != 0) system_error("durable file close");
    }
private:
    int value_;
};
inline Descriptor directory(const std::filesystem::path& path) {
    validate_file_path(path);
    const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) system_error("open durable directory");
    return Descriptor(fd);
}
inline Descriptor child_directory(int parent, const char* name) {
    const int fd = ::openat(parent, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) system_error("open durable child directory");
    return Descriptor(fd);
}
inline void synchronize(int fd) {
    while (::fsync(fd) != 0) { if (errno != EINTR) system_error("durable fsync"); }
}
inline Descriptor regular(int parent, const std::string& name, int flags) {
    const int fd = ::openat(parent, name.c_str(), flags | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (fd < 0) system_error("open durable file");
    Descriptor out(fd);
    struct stat info{};
    if (::fstat(fd, &info) != 0) system_error("stat durable file");
    if (!S_ISREG(info.st_mode)) throw std::invalid_argument("durable regular file required");
    return out;
}
inline void write(int fd, std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        const auto n = ::write(fd, bytes.data(), std::min(bytes.size(), std::size_t{1u << 20}));
        if (n < 0) { if (errno == EINTR) continue; system_error("durable write"); }
        if (n == 0) throw std::runtime_error("zero-progress durable write");
        bytes = bytes.subspan(static_cast<std::size_t>(n));
    }
}
inline std::vector<std::byte> read(int parent, const std::string& name, std::size_t limit) {
    auto fd = regular(parent, name, O_RDONLY);
    struct stat info{};
    if (::fstat(fd.get(), &info) != 0) system_error("stat durable input");
    if (info.st_size < 0 || static_cast<std::uintmax_t>(info.st_size) > limit ||
        static_cast<std::uintmax_t>(info.st_size) > static_cast<std::uintmax_t>(std::numeric_limits<std::ptrdiff_t>::max()))
        throw std::length_error("durable input byte budget");
    std::vector<std::byte> bytes(static_cast<std::size_t>(info.st_size));
    for (std::size_t pos = 0; pos < bytes.size();) {
        const auto n = ::read(fd.get(), bytes.data() + pos, std::min(bytes.size() - pos, std::size_t{1u << 20}));
        if (n < 0) { if (errno == EINTR) continue; system_error("durable read"); }
        if (n == 0) throw std::runtime_error("truncated durable input");
        pos += static_cast<std::size_t>(n);
    }
    std::byte extra{};
    ssize_t n;
    do { n = ::read(fd.get(), &extra, 1); } while (n < 0 && errno == EINTR);
    if (n != 0) throw std::runtime_error("durable input changed or unreadable");
    fd.close_checked();
    return bytes;
}
inline Descriptor coordinator(int parent) {
    auto fd = regular(parent, "LOCK", O_RDWR | O_CREAT);
    if (::flock(fd.get(), LOCK_EX | LOCK_NB) != 0) system_error("durable coordinator already held");
    synchronize(fd.get()); synchronize(parent);
    return fd;
}
inline std::array<std::uint64_t, 2> random_name() {
    std::array<std::uint64_t, 2> out{};
    std::span<std::byte> bytes = std::as_writable_bytes(std::span(out));
    while (!bytes.empty()) {
        const auto n = ::getrandom(bytes.data(), bytes.size(), 0);
        if (n < 0) { if (errno == EINTR) continue; system_error("durable object name"); }
        if (n == 0) throw std::runtime_error("zero-progress durable object name");
        bytes = bytes.subspan(static_cast<std::size_t>(n));
    }
    if (out[0] == 0 && out[1] == 0) return random_name();
    return out;
}
} // namespace rfc::storage::linux_durable_detail
