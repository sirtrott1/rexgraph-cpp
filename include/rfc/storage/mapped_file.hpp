#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace rfc::storage {

// Read-only byte image. On POSIX hosts this owns an mmap; elsewhere it falls
// back to one contiguous read. The higher RFCX layer is independent of which
// physical path supplied the bytes.
class ReadOnlyMappedFile {
public:
    ReadOnlyMappedFile() = default;
    explicit ReadOnlyMappedFile(const std::filesystem::path& path) { open(path); }
    ~ReadOnlyMappedFile() { close(); }
    // Own a defensive copy of an embedded image. This is the same reader
    // backend, not an unchecked borrowed span or a temporary disk file.
    explicit ReadOnlyMappedFile(std::span<const std::byte> bytes)
        : owned_(bytes.begin(),bytes.end()) {
        data_=owned_.data(); size_=owned_.size();
    }


    ReadOnlyMappedFile(const ReadOnlyMappedFile&) = delete;
    ReadOnlyMappedFile& operator=(const ReadOnlyMappedFile&) = delete;

    ReadOnlyMappedFile(ReadOnlyMappedFile&& other) noexcept { move_from(std::move(other)); }
    ReadOnlyMappedFile& operator=(ReadOnlyMappedFile&& other) noexcept {
        if (this != &other) {
            close();
            move_from(std::move(other));
        }
        return *this;
    }

    void open(const std::filesystem::path& path) {
        close();
#if defined(__unix__) || defined(__APPLE__)
        fd_ = ::open(path.c_str(), O_RDONLY);
        if (fd_ < 0) throw std::runtime_error("unable to open mapped file");
        struct stat st {};
        if (::fstat(fd_, &st) != 0) {
            close();
            throw std::runtime_error("unable to stat mapped file");
        }
        if (st.st_size < 0 || static_cast<std::uintmax_t>(st.st_size) > static_cast<std::uintmax_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
            close();
            throw std::runtime_error("invalid mapped file size");
        }
        size_ = static_cast<std::size_t>(st.st_size);
        if (size_ == 0) return;
        void* ptr = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (ptr == MAP_FAILED) {
            close();
            throw std::runtime_error("mmap failed");
        }
        data_ = static_cast<const std::byte*>(ptr);
        mapped_ = true;
#else
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("unable to open file");
        in.seekg(0, std::ios::end);
        const auto end = in.tellg();
        if (end < 0 || static_cast<std::uintmax_t>(end) > static_cast<std::uintmax_t>(std::numeric_limits<std::ptrdiff_t>::max()) ||
            static_cast<std::uintmax_t>(end) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) throw std::runtime_error("unable to size file");
        owned_.resize(static_cast<std::size_t>(end));
        in.seekg(0, std::ios::beg);
        if (!owned_.empty()) {
            in.read(reinterpret_cast<char*>(owned_.data()), static_cast<std::streamsize>(owned_.size()));
            if (!in) throw std::runtime_error("unable to read file");
        }
        data_ = owned_.data();
        size_ = owned_.size();
#endif
    }

    void close() noexcept {
#if defined(__unix__) || defined(__APPLE__)
        if (mapped_ && data_ != nullptr && size_ != 0) {
            ::munmap(const_cast<std::byte*>(data_), size_);
        }
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
        mapped_ = false;
#endif
        data_ = nullptr;
        size_ = 0;
        owned_.clear();
    }

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return {data_, size_}; }
    [[nodiscard]] const std::byte* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

private:
    void move_from(ReadOnlyMappedFile&& other) noexcept {
        data_ = other.data_;
        size_ = other.size_;
        owned_ = std::move(other.owned_);
#if defined(__unix__) || defined(__APPLE__)
        fd_ = other.fd_;
        mapped_ = other.mapped_;
        other.fd_ = -1;
        other.mapped_ = false;
#endif
        if (!owned_.empty()) data_ = owned_.data();
        other.data_ = nullptr;
        other.size_ = 0;
    }

    const std::byte* data_{};
    std::size_t size_{};
    std::vector<std::byte> owned_;
#if defined(__unix__) || defined(__APPLE__)
    int fd_{-1};
    bool mapped_{false};
#endif
};

} // namespace rfc::storage
