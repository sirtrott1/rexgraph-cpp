#include "rfc/storage/declaration_codec.hpp"
#include "rfc/storage/native_series_codec.hpp"
#include <cstdlib>
#include <optional>
#include <span>
#include <type_traits>

namespace {
template <class Decoder> auto accepted(Decoder &&decoder) -> std::optional<std::invoke_result_t<Decoder>> {
    try {
        return decoder();
    } catch (const std::invalid_argument &) {
    } catch (const std::out_of_range &) {
    } catch (const std::length_error &) {
    } catch (const std::runtime_error &) {
    }
    return std::nullopt;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    if (!size || size > 65537)
        return 0;
    const auto bytes = std::span(reinterpret_cast<const std::byte *>(data + 1), size - 1);
    rfc::composite::DeclarationLimits declarations;
    declarations.max_definitions = 64;
    declarations.max_components = 1024;
    declarations.max_depth = 16;
    declarations.max_value_nodes = 4096;
    declarations.max_encoded_bytes = 65536;
    declarations.max_integer_bits = 4096;
    declarations.max_materialized_bytes = 1u << 20;
    declarations.max_tensor_rank = 8;
    switch (data[0] % 3) {
    case 0: {
        const auto value = accepted([&] {
            rfc::storage::MappedRfcx image(bytes);
            return image.materialize();
        });
        if (!value)
            break;
        const auto encoded = rfc::storage::encode_rfcx(*value).bytes;
        rfc::storage::MappedRfcx rebuilt(encoded);
        if (rfc::storage::encode_rfcx(rebuilt.materialize()).bytes != encoded)
            std::abort();
        break;
    }
    case 1: {
        const auto value =
            accepted([&] { return rfc::storage::decode_declared_archive(bytes, declarations); });
        if (!value)
            break;
        const auto encoded = rfc::storage::encode_declared_archive(value->layout, value->values);
        if (encoded.size() != bytes.size() || !std::equal(encoded.begin(), encoded.end(), bytes.begin()))
            std::abort();
        break;
    }
    case 2: {
        rfc::storage::NativeSeriesLimits limits;
        limits.max_segments = 16;
        limits.max_checkpoints = 4;
        limits.max_archive_bytes = 65536;
        limits.temporal.max_file_bytes = 65536;
        limits.temporal.max_records = 4096;
        limits.temporal.max_dimension = 4096;
        limits.temporal.max_rank = 8;
        limits.temporal.max_integer_bytes = 512;
        limits.temporal.declarations = declarations;
        const auto value = accepted([&] { return rfc::storage::decode_native_series(bytes, limits); });
        if (!value)
            break;
        const auto encoded = rfc::storage::encode_native_series(*value);
        if (encoded.size() != bytes.size() || !std::equal(encoded.begin(), encoded.end(), bytes.begin()))
            std::abort();
        break;
    }
    }
    // bad_alloc, unexpected logic failures, signals and canonical mismatches
    // remain failures. No filesystem mutation or process-global state is used.
    return 0;
}
