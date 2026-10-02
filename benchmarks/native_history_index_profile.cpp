#include "rfc/storage/persistent_append_index.hpp"
#include <iostream>
#include <vector>
#include "exact_allocation_hooks.hpp"

// Serial directory-only allocation profile. No GMP hooks or threaded service.
int main() {
    const auto value = std::make_shared<const std::size_t>(1);
    for (const auto size : {0u, 1u, 31u, 32u, 33u, 1023u, 1024u, 32767u, 32768u, 40000u, 65536u}) {
        rfc::storage::native_series_detail::AppendIndex<std::size_t> index;
        std::vector<std::shared_ptr<const std::size_t>> flat(size, value);
        for (std::size_t i = 0; i < size; ++i) index = index.appended(value);
        counts = {}; accounting = true; const auto appended = index.appended(value); accounting = false;
        const auto tree_counts = counts;
        counts = {}; accounting = true; auto copied = flat; copied.push_back(value); accounting = false;
        const auto flat_counts = counts; const auto stats = appended.stats();
        if (tree_counts.cpp_calls != stats.last_append_pages || copied.size() != appended.size() ||
            appended.at(size) != value) return 1;
        std::cout << "{\"prior_frames\":" << size << ",\"levels\":" << stats.levels
                  << ",\"page_allocations\":" << stats.last_append_pages
                  << ",\"pointer_copies\":" << stats.last_append_pointer_copies
                  << ",\"cpp_allocations\":" << tree_counts.cpp_calls << ",\"cpp_bytes\":" << tree_counts.cpp_bytes
                  << ",\"flat_cpp_allocations\":" << flat_counts.cpp_calls << ",\"flat_cpp_bytes\":" << flat_counts.cpp_bytes << "}\n";
    }
}
