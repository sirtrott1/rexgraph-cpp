#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace rfc::storage {

// Native path encoding cannot carry an embedded zero code unit. Reject instead
// of silently addressing the prefix passed through a C/OS file API.
inline void validate_file_path(const std::filesystem::path& path) {
    if (path.empty() || path.native().find(std::filesystem::path::value_type{}) !=
                        std::filesystem::path::string_type::npos)
        throw std::invalid_argument("empty or NUL-containing file path");
}

inline void write_stream_exact(std::ostream& out, std::span<const std::byte> bytes) {
    // Bounded chunks avoid narrowing a whole size_t image to streamsize.
    constexpr std::size_t chunk_size = 1u << 20;
    while (!bytes.empty()) {
        const auto n = std::min(bytes.size(), chunk_size);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(n));
        if (!out) throw std::runtime_error("file write failed");
        bytes = bytes.subspan(n);
    }
}

inline std::vector<std::byte> read_file_exact(const std::filesystem::path& path, std::size_t limit) {
    validate_file_path(path);
    if (!std::filesystem::is_regular_file(path)) throw std::invalid_argument("regular input file required");
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("cannot open input file");
    const auto end = in.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) > limit ||
        static_cast<std::uintmax_t>(end) > static_cast<std::uintmax_t>(std::numeric_limits<std::ptrdiff_t>::max()))
        throw std::length_error("input file exceeds byte budget");
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    in.seekg(0);
    for (std::size_t p = 0; p < bytes.size();) {
        const auto n = std::min(bytes.size() - p, std::size_t{1u << 20});
        in.read(reinterpret_cast<char*>(bytes.data() + p), static_cast<std::streamsize>(n));
        if (!in) throw std::runtime_error("input file truncated or unreadable");
        p += n;
    }
    if (in.peek() != std::char_traits<char>::eof() || in.bad())
        throw std::runtime_error("input file changed or unreadable at EOF");
    return bytes;
}

// Replace preserves the former publication on failure before rename. CreateNew
// uses no-replace hard-link publication, not an exists/open race. Both stage on
// the destination filesystem and observe write, flush and close errors.
// Parent directories must be trusted; external writer synchronization remains
// required. This is not fsync durability, a WAL, recovery or multi-file commit.
// A host lacking replace-rename/hard-link support fails without deleting the old
// destination. New output permissions are owner read/write only.
enum class FilePublication { ReplaceRegular, CreateNew };

namespace file_io_detail {
struct StagingDirectory {
    std::filesystem::path directory, file;
    explicit StagingDirectory(const std::filesystem::path& parent) {
        static std::atomic<std::uint64_t> serial{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 128; ++attempt) {
            auto candidate = parent / (".rfc-stage-" + std::to_string(stamp) + "-" +
                                       std::to_string(serial.fetch_add(1, std::memory_order_relaxed)));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                directory = std::move(candidate); file = directory / "image";
                try {
                    std::filesystem::permissions(directory, std::filesystem::perms::owner_all);
                } catch (...) { std::filesystem::remove(directory, error); throw; }
                return;
            }
            if (error && error != std::errc::file_exists)
                throw std::filesystem::filesystem_error("cannot create staging directory", candidate, error);
        }
        throw std::runtime_error("staging directory name budget exhausted");
    }
    ~StagingDirectory() {
        std::error_code ignored;
        if (!file.empty()) std::filesystem::remove(file, ignored);
        if (!directory.empty()) std::filesystem::remove(directory, ignored);
    }
    StagingDirectory(const StagingDirectory&) = delete;
    StagingDirectory& operator=(const StagingDirectory&) = delete;
};
inline void check_destination(const std::filesystem::path& path, FilePublication policy) {
    const auto status = std::filesystem::symlink_status(path);
    if (status.type() == std::filesystem::file_type::not_found) return;
    if (policy == FilePublication::CreateNew || !std::filesystem::is_regular_file(status))
        throw std::invalid_argument("output exists or is not a replaceable regular file");
}
} // namespace file_io_detail

template<class Producer>
inline void publish_file_exact(const std::filesystem::path& path, FilePublication policy, Producer&& produce) {
    validate_file_path(path);
    if (policy != FilePublication::ReplaceRegular && policy != FilePublication::CreateNew)
        throw std::invalid_argument("unknown file publication policy");
    file_io_detail::check_destination(path, policy);
    auto parent = path.parent_path(); if (parent.empty()) parent = ".";
    file_io_detail::StagingDirectory stage(parent);
    std::ofstream out(stage.file, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot open staged output");
    std::filesystem::permissions(stage.file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    std::forward<Producer>(produce)(out);
    out.flush(); if (!out) throw std::runtime_error("staged output flush failed");
    out.close(); if (!out) throw std::runtime_error("staged output close failed");
    // Validate again after a host producer callback. No deletion or truncation
    // of the former output is ever a step in publication.
    file_io_detail::check_destination(path, policy);
    if (policy == FilePublication::CreateNew) std::filesystem::create_hard_link(stage.file, path);
    else std::filesystem::rename(stage.file, path);
}

inline void write_file_exact(const std::filesystem::path& path, std::span<const std::byte> bytes,
                             FilePublication policy = FilePublication::ReplaceRegular) {
    publish_file_exact(path, policy, [&](std::ostream& out) { write_stream_exact(out, bytes); });
}

} // namespace rfc::storage
