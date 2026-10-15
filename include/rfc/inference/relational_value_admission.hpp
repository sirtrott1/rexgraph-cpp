#pragma once

#include "rfc/inference/relational_field_interface_codec.hpp"
#include "rfc/runtime/work_graph.hpp"
#include "rfc/storage/native_series_prefix.hpp"
#include <mutex>

namespace rfc::inference {
enum class RelationalDependencyRole : std::uint8_t {
    Program, Maps, Metrics, InputAsset, InputValue, InputAccession, Reconstruction,
    TargetAsset, TargetValue, Interface, Evidence, Prospective
};
struct RelationalAdmissionLimits {
    std::size_t origins{4096}, payload_bytes{64u << 20}, total_bytes{256u << 20};
};
namespace value_admission_detail {
using Bytes = std::vector<std::byte>;
inline void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
inline void bound(std::size_t value, std::size_t maximum) {
    if (value > maximum) throw std::length_error("relational admission resource bound");
}
inline bool valid(RelationalDependencyRole role) {
    return static_cast<unsigned>(role) <= static_cast<unsigned>(RelationalDependencyRole::Prospective);
}
inline constexpr std::array<std::byte, 8> origin_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'O'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> value_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'I'}, std::byte{1}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 8> accession_magic{
    std::byte{'R'}, std::byte{'F'}, std::byte{'C'}, std::byte{'V'}, std::byte{'A'}, std::byte{1}, std::byte{0}, std::byte{0}};
} // namespace value_admission_detail

// This is the trusted publisher boundary. Neither archive decoding nor ordinary
// inference constructs an origin. The publisher declares the actual value's
// publication, independently of the original bytes' publication.
class OwnedRelationalValueOrigin {
    struct State {
        std::uint64_t key;
        storage::NativeSeriesPrefix publication;
        RelationalDependencyRole role;
        value_admission_detail::Bytes receipt;
        std::size_t payload_offset, payload_size;
        mutable std::once_flag payload_once;
        mutable std::mutex payload_mutex;
        mutable value_admission_detail::Bytes payload_cache;
        State(std::uint64_t k, storage::NativeSeriesPrefix p, RelationalDependencyRole r,
            value_admission_detail::Bytes bytes, std::size_t size)
            : key(k), publication(std::move(p)), role(r), receipt(std::move(bytes)),
              payload_offset(receipt.size()-size), payload_size(size) {}
    };
public:
    OwnedRelationalValueOrigin(std::uint64_t key, storage::NativeSeriesPrefix publication,
        RelationalDependencyRole role, value_admission_detail::Bytes payload, RelationalAdmissionLimits limits = {}) {
        using namespace value_admission_detail;
        require(key && valid(role) && !payload.empty(), "actual value origin key, role and content required");
        bound(payload.size(), limits.payload_bytes);
        auto w = field_program_codec_detail::envelope(origin_magic);
        w.u64(key); w.u8(static_cast<std::uint8_t>(role));
        field_program_codec_detail::blob(w, storage::encode_native_series(publication.history()));
        field_program_codec_detail::blob(w, payload);
        auto receipt = field_program_codec_detail::finish(std::move(w));
        bound(receipt.size(), limits.total_bytes);
        // A long-lived origin retains exact receipt bytes, not the encoder's
        // spare capacity and a second eager copy of the same payload.
        Bytes compact(receipt.begin(),receipt.end());
        state_ = std::make_shared<const State>(key, std::move(publication), role, std::move(compact), payload.size());
    }
    std::uint64_t key() const { return state_->key; }
    RelationalDependencyRole role() const { return state_->role; }
    storage::NativeSeriesPrefix publication() const { return state_->publication; }
    std::span<const std::byte> payload_bytes() const & {
        return std::span<const std::byte>(state_->receipt).subspan(state_->payload_offset,state_->payload_size);
    }
    std::span<const std::byte> payload_bytes() const && = delete;
    const value_admission_detail::Bytes& payload() const & {
        const auto pin=state_;
        std::call_once(pin->payload_once,[&]{
            const std::lock_guard lock(pin->payload_mutex);
            const auto bytes=std::span<const std::byte>(pin->receipt).subspan(pin->payload_offset,pin->payload_size);
            pin->payload_cache.assign(bytes.begin(),bytes.end());
        });
        return pin->payload_cache;
    }
    const value_admission_detail::Bytes& payload() const && = delete;
    std::size_t retained_byte_count() const {
        const auto pin=state_;const std::lock_guard lock(pin->payload_mutex);
        return machine::checked_size_add(pin->receipt.capacity(),pin->payload_cache.capacity());
    }
    const value_admission_detail::Bytes& receipt() const & { return state_->receipt; }
    const value_admission_detail::Bytes& receipt() const && = delete;
    bool same_owner(const OwnedRelationalValueOrigin& other) const { return state_ == other.state_; }
private:
    std::shared_ptr<const State> state_;
};
struct RelationalOriginGrant {
    OwnedRelationalValueOrigin origin;
    runtime::WorkAvailability availability;
};
// The caller supplies actual authority separately from saved model/result bytes.
// A context may contain a currently unavailable origin: every use rechecks it.
class RelationalAvailabilityContext {
public:
    explicit RelationalAvailabilityContext(std::vector<RelationalOriginGrant> grants,
        RelationalAdmissionLimits limits = {}) {
        using namespace value_admission_detail;
        bound(grants.size(), limits.origins); std::map<std::uint64_t, RelationalOriginGrant> values;
        std::size_t total{};
        for (auto& grant : grants) {
            bound(grant.origin.payload_bytes().size(), limits.payload_bytes);
            total = machine::checked_size_add(total, grant.origin.receipt().size()); bound(total, limits.total_bytes);
            value_admission_detail::require(values.emplace(grant.origin.key(), std::move(grant)).second, "duplicate actual origin authority key");
        }
        grants_ = std::make_shared<const std::map<std::uint64_t, RelationalOriginGrant>>(std::move(values));
    }
    OwnedRelationalValueOrigin require(std::uint64_t key, RelationalDependencyRole role,
        const value_admission_detail::Bytes& payload) const {
        return require_bytes(key, role, payload);
    }
    OwnedRelationalValueOrigin require_bytes(std::uint64_t key, RelationalDependencyRole role,
        std::span<const std::byte> payload) const {
        return require_content(key,role,[&](std::span<const std::byte> bytes){
            return bytes.size()==payload.size()&&std::equal(bytes.begin(),bytes.end(),payload.begin());});
    }
    OwnedRelationalValueOrigin require_bytes(std::uint64_t key, RelationalDependencyRole role,
        const storage::ByteRange& payload) const {
        return require_content(key,role,[&](std::span<const std::byte> bytes){return payload.equals(bytes);});
    }
    void recheck(const OwnedRelationalValueOrigin& expected) const {
        const auto actual = require_bytes(expected.key(), expected.role(), expected.payload_bytes());
        value_admission_detail::require(actual.same_owner(expected), "different actual live value origin owner");
    }
private:
    template<class Matches> OwnedRelationalValueOrigin require_content(std::uint64_t key, RelationalDependencyRole role,Matches&& matches) const {
        const auto found = grants_->find(key);
        value_admission_detail::require(found != grants_->end(), "missing actual value origin authority");
        const auto& grant = found->second; const auto origin = grant.origin;
        value_admission_detail::require(origin.role() == role && matches(origin.payload_bytes()),
            "actual value origin role or complete content differs");
        const auto publication = origin.publication();
        runtime::admit_work_availability(publication.source_namespace(), publication.cutoff(), grant.availability);
        return origin;
    }
    std::shared_ptr<const std::map<std::uint64_t, RelationalOriginGrant>> grants_;
};

inline value_admission_detail::Bytes relational_program_dependency_payload(const OwnedRelationalFieldProgram& program) {
    return encode_relational_field_program(program);
}
// Passive encoding is shared by online declarations and cold parsing. These
// routines serialize coefficients; they never apply an accession or field map.
inline value_admission_detail::Bytes relational_input_value_payload(std::span<const std::byte> asset,
    const RelationalInputAccessionDeclaration& declaration, std::uint64_t use, const numeric::SparseRationalTensor& field) {
    using namespace field_interface_codec_detail;
    auto w = envelope(value_admission_detail::value_magic);
    blob(w, asset); w.u64(use); w.u16(declaration.source_grade.value);
    stalks(w, declaration.source_stalks); storage::temporal_codec_detail::tensor(w, field);
    return finish(std::move(w));
}
inline value_admission_detail::Bytes relational_input_accession_payload(std::span<const std::byte> program,
    std::span<const std::byte> asset, const RelationalInputAccessionDeclaration& declaration,
    std::uint64_t use, const numeric::SparseRationalTensor& field) {
    using namespace field_interface_codec_detail;
    auto w = envelope(value_admission_detail::accession_magic);
    blob(w, program); blob(w, asset); accession(w, declaration); w.u64(use); storage::temporal_codec_detail::tensor(w, field);
    return finish(std::move(w));
}
inline value_admission_detail::Bytes relational_input_value_payload(const RelationalAccessionInput& input) {
    const auto a = input.accession();
    return relational_input_value_payload(encode_relational_input_use_asset(a.asset()), a.declaration(), input.use(), input.source_field());
}
inline value_admission_detail::Bytes relational_input_accession_payload(const RelationalAccessionInput& input) {
    const auto a = input.accession();
    return relational_input_accession_payload(encode_relational_field_program(a.program()), encode_relational_input_use_asset(a.asset()),
        a.declaration(), input.use(), input.source_field());
}
struct RelationalProgramOrigins { std::uint64_t program{}, maps{}, metrics{}; };
struct RelationalInputOrigins { std::uint64_t asset{}, value{}, accession{}, reconstruction{}; };
struct RelationalAvailableInput { RelationalAccessionInput input; RelationalInputOrigins origins; };
struct RelationalInputSubmission {
    OwnedRelationalInputUseAsset asset;
    RelationalInputAccessionDeclaration declaration;
    std::uint64_t use{};
    numeric::SparseRationalTensor source_field;
};
struct RelationalAvailableInputSubmission { RelationalInputSubmission input; RelationalInputOrigins origins; };

namespace value_admission_detail {
inline std::vector<OwnedRelationalValueOrigin> program_origins(const OwnedRelationalFieldProgram& program,
    RelationalProgramOrigins keys, const RelationalAvailabilityContext& context) {
    const auto payload = relational_program_dependency_payload(program);
    return {context.require(keys.program, RelationalDependencyRole::Program, payload),
        context.require(keys.maps, RelationalDependencyRole::Maps, payload),
        context.require(keys.metrics, RelationalDependencyRole::Metrics, payload)};
}
inline void input_origins(std::vector<OwnedRelationalValueOrigin>& out, RelationalInputOrigins keys,
    const Bytes& asset, const Bytes& value, const Bytes& accession, const RelationalAvailabilityContext& context) {
    out.push_back(context.require(keys.asset, RelationalDependencyRole::InputAsset, asset));
    out.push_back(context.require(keys.value, RelationalDependencyRole::InputValue, value));
    out.push_back(context.require(keys.accession, RelationalDependencyRole::InputAccession, accession));
    out.push_back(context.require(keys.reconstruction, RelationalDependencyRole::Reconstruction, accession));
}
inline void input_origins(std::vector<OwnedRelationalValueOrigin>& out, const RelationalAvailableInput& input,
    const RelationalAvailabilityContext& context) {
    input_origins(out, input.origins, encode_relational_input_use_asset(input.input.accession().asset()),
        relational_input_value_payload(input.input), relational_input_accession_payload(input.input), context);
}
} // namespace value_admission_detail

class AvailableRelationalFieldResult {
    struct State {
        OwnedAccessionFieldResult result;
        RelationalProgramOrigins program;
        std::vector<RelationalInputOrigins> inputs;
        std::vector<OwnedRelationalValueOrigin> obligations;
    };
public:
    OwnedAccessionFieldResult result() const { return state_->result; }
    RelationalProgramOrigins program_origins() const { return state_->program; }
    const std::vector<RelationalInputOrigins>& input_origins() const & { return state_->inputs; }
    const std::vector<RelationalInputOrigins>& input_origins() const && = delete;
    const std::vector<OwnedRelationalValueOrigin>& obligations() const & { return state_->obligations; }
    const std::vector<OwnedRelationalValueOrigin>& obligations() const && = delete;
    void admit(const RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        for (const auto& origin : pin->obligations) { if (callback) callback(); authority.recheck(origin); }
    }
private:
    friend class PreparedAvailableRelationalProgram;
    explicit AvailableRelationalFieldResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
class PreparedAvailableRelationalProgram {
    struct State {
        OwnedRelationalFieldProgram program;
        RelationalProgramOrigins keys;
        std::vector<OwnedRelationalValueOrigin> obligations;
    };
public:
    PreparedAvailableRelationalProgram(OwnedRelationalFieldProgram program, RelationalProgramOrigins origins,
        const RelationalAvailabilityContext& context) {
        auto obligations = value_admission_detail::program_origins(program, origins, context);
        state_ = std::make_shared<const State>(State{std::move(program), origins, std::move(obligations)});
    }
    OwnedRelationalFieldProgram program() const { return state_->program; }
    RelationalProgramOrigins program_origins() const { return state_->keys; }
    const std::vector<OwnedRelationalValueOrigin>& obligations() const & { return state_->obligations; }
    const std::vector<OwnedRelationalValueOrigin>& obligations() const && = delete;
    void admit(const RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto callback = std::move(check);
        for (const auto& origin : pin->obligations) { if (callback) callback(); authority.recheck(origin); }
    }
    void check_inputs(std::vector<RelationalAvailableInputSubmission> inputs,
        const RelationalAvailabilityContext& context, std::function<void()> check = {}) const {
        const auto pin = state_; const auto frozen = std::move(inputs); const auto authority = context; const auto callback = std::move(check);
        for (const auto& origin : pin->obligations) { if (callback) callback(); authority.recheck(origin); }
        const auto program = encode_relational_field_program(pin->program);
        for (const auto& row : frozen) {
            if (callback) callback();
            const auto& input = row.input; const auto asset = encode_relational_input_use_asset(input.asset);
            const auto value = relational_input_value_payload(asset, input.declaration, input.use, input.source_field);
            const auto accession = relational_input_accession_payload(program, asset, input.declaration, input.use, input.source_field);
            std::vector<OwnedRelationalValueOrigin> checked;
            value_admission_detail::input_origins(checked, row.origins, asset, value, accession, authority);
        }
    }
    AvailableRelationalFieldResult execute_inputs(std::vector<RelationalAvailableInputSubmission> inputs,
        const RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        const auto pin = *this; const auto frozen = std::move(inputs); const auto authority = context; const auto callback = std::move(check);
        pin.check_inputs(frozen, authority, callback);
        std::vector<RelationalAvailableInput> admitted;
        for (const auto& row : frozen) admitted.push_back(pin.admit_input(row.input, row.origins, authority, limits, callback));
        return pin.execute(std::move(admitted), authority, limits, callback);
    }
    RelationalAvailableInput admit_input(RelationalInputSubmission submission, RelationalInputOrigins keys,
        const RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) const {
        const auto pin = state_; const auto authority = context; const auto frozen = std::move(submission);
        const auto callback = std::move(check); numeric::ExactActionGuard guard(limits, callback);
        for (const auto& origin : pin->obligations) { guard.step(); authority.recheck(origin); }
        const auto asset = encode_relational_input_use_asset(frozen.asset);
        const auto value = relational_input_value_payload(asset, frozen.declaration, frozen.use, frozen.source_field);
        const auto accession = relational_input_accession_payload(encode_relational_field_program(pin->program), asset,
            frozen.declaration, frozen.use, frozen.source_field);
        std::vector<OwnedRelationalValueOrigin> obligations;
        value_admission_detail::input_origins(obligations, keys, asset, value, accession, authority);
        guard.step();
        const OwnedRelationalInputAccession owned(frozen.asset, pin->program, frozen.declaration, limits, callback);
        return {owned.admit(frozen.use, frozen.source_field, limits, callback), keys};
    }
    AvailableRelationalFieldResult execute(std::vector<RelationalAvailableInput> inputs,
        const RelationalAvailabilityContext& context, numeric::ExactActionLimits limits = {},
        std::function<void()> check = {}) const {
        // Pin owned arguments before callbacks; cancellation cannot publish a child.
        const auto pin = state_; const auto authority = context; const auto frozen = std::move(inputs); const auto callback = std::move(check);
        numeric::ExactActionGuard guard(limits, callback); guard.entries(frozen.size());
        auto obligations = pin->obligations;
        for (const auto& origin : obligations) { guard.step(); authority.recheck(origin); }
        std::vector<RelationalAccessionInput> native_inputs; std::vector<RelationalInputOrigins> keys;
        for (const auto& input : frozen) {
            guard.step(); value_admission_detail::input_origins(obligations, input, authority);
            native_inputs.push_back(input.input); keys.push_back(input.origins);
        }
        auto result = execute_relational_inputs(pin->program, std::move(native_inputs), limits, callback);
        guard.step();
        return AvailableRelationalFieldResult({std::move(result), pin->keys, std::move(keys), std::move(obligations)});
    }
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::inference
