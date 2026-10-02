#pragma once

#include "rfc/backend/packed_binary.hpp"
#include "rfc/runtime/work_graph.hpp"
#include <array>

namespace rfc::backend {
// The named declaration establishes shared slot coordinates. Size alone never
// authenticates a result-to-input binding, including for equal-valued planes.
class PackedWorkCarrier {
public:
    static PackedWorkCarrier declare_slots(std::size_t slots) {return PackedWorkCarrier(std::make_shared<const std::size_t>(slots));}
    std::size_t slots() const {if(!slots_)throw std::invalid_argument("moved packed work carrier");return *slots_;}
    bool same_carrier(const PackedWorkCarrier& other) const noexcept {return slots_ && slots_==other.slots_;}
private:
    explicit PackedWorkCarrier(std::shared_ptr<const std::size_t> slots):slots_(std::move(slots)){}
    std::shared_ptr<const std::size_t> slots_;
};
struct PackedWorkBinding {runtime::WorkKey producer;std::size_t output{};};
struct PackedWorkSource {std::uint64_t source_namespace{},sequence{};runtime::WorkValue publication;};
struct PackedWorkDeviceReport {
    std::string name;std::array<std::uint8_t,16> uuid{};bool physical_gpu{};
    std::size_t selected_index{},gpu_words{},cpu_words{},cpu_workers{};
    std::uint64_t upload_ns{},submit_ns{},cpu_ns{},completion_ns{},total_ns{};
};
struct PackedWorkResult {
    runtime::WorkKey key;
    PackedWorkCarrier carrier;
    PreparedPackedBinary operation;
    std::vector<machine::PackedBits> outputs;
    std::vector<PackedWorkBinding> input_bindings;
    std::vector<std::shared_ptr<const PackedWorkResult>> predecessors;
    std::vector<PackedWorkSource> sources;
    std::optional<PackedWorkDeviceReport> device;
};
namespace binary_work_detail {
inline std::size_t result_bytes(const PreparedPackedBinary& operation) {
    return machine::checked_size_mul(machine::checked_size_mul(operation.words(),operation.outputs().size()),sizeof(std::uint64_t));
}
inline runtime::WorkDemand demand(const PreparedPackedBinary& operation,std::size_t workers,bool copy_inputs=false) {
    auto bytes=machine::checked_size_add(result_bytes(operation),machine::checked_size_mul(
        machine::checked_size_mul(workers,operation.program().register_count()),sizeof(std::uint64_t)));
    if(copy_inputs)bytes=machine::checked_size_add(bytes,machine::checked_size_mul(
        machine::checked_size_mul(operation.words(),operation.planes().aspect_count()),sizeof(std::uint64_t)));
    return {workers,bytes,result_bytes(operation)};
}
inline std::vector<machine::PackedBits> cpu(const PreparedPackedBinary& operation,std::size_t workers,std::size_t tile_words,
    std::size_t result_limit,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
    const auto d=demand(operation,workers);auto reservation=assigned.reserve(workers,d.buffer_bytes);
    auto result=operation.empty_result(result_limit);
    runtime::CpuTileExecutor executor({workers,assigned.capacity(),assigned.policy().tile_limit});
    executor.run_tiles(machine::ceil_div(operation.words(),tile_words),[&](auto tile,auto,const auto& token){
        token.check();const auto first=tile*tile_words;operation.cpu_words(first,first+std::min(tile_words,operation.words()-first),result,stop);token.check();
    });
    return result;
}
inline void validate(const PreparedPackedBinary& operation,const PackedWorkCarrier& carrier,std::size_t workers,std::size_t tile_words,std::size_t limit) {
    if(!workers || !tile_words || carrier.slots()!=operation.planes().slots())throw std::invalid_argument("packed work carrier/worker/tile contract");
    if(result_bytes(operation)>limit)throw std::length_error("packed work result byte budget");
}
inline void availability(const std::vector<PackedWorkSource>& sources,const std::optional<runtime::WorkAvailability>& cutoff) {
    if(cutoff && sources.empty())throw std::invalid_argument("publication cutoff needs a declared native source");
    for(const auto& source:sources)runtime::admit_work_availability(source.source_namespace,source.sequence,cutoff);
}
} // namespace binary_work_detail
inline runtime::WorkJob make_cpu_binary_work(runtime::WorkKey key,const PreparedPackedBinary& operation,
    const PackedWorkCarrier& carrier,std::size_t workers=1,std::size_t tile_words=64,
    std::vector<runtime::WorkKey> after={},std::size_t result_limit=std::size_t{1}<<30) {
    binary_work_detail::validate(operation,carrier,workers,tile_words,result_limit);
    return runtime::WorkJob::make<PackedWorkResult>(key,binary_work_detail::demand(operation,workers),std::move(after),
        [key,operation,carrier,workers,tile_words,result_limit](const runtime::WorkInputs&,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            return PackedWorkResult{key,carrier,operation,binary_work_detail::cpu(operation,workers,tile_words,result_limit,assigned,stop),{},{},{},{}};
        });
}
class PackedWorkRecipe {
public:
    PackedWorkRecipe(PackedWorkCarrier carrier,composite::BinaryProgram program,composite::AspectSchema schema,
        std::vector<PackedWorkBinding> inputs,std::vector<std::uint16_t> outputs,PackedBinaryLimits limits={})
        :carrier_(std::move(carrier)),schema_(std::move(schema)),inputs_(std::move(inputs)),limits_(limits),
         prototype_(program,composite::AspectPlanes(schema_,carrier_.slots(),limits.retained_bytes/8),std::move(outputs),limits) {
        if(inputs_.size()!=schema_.size())throw std::invalid_argument("every derived packed aspect needs an explicit binding");
        for(const auto& input:inputs_)if(!input.producer.id || !input.producer.generation)throw std::invalid_argument("packed input generation absent");
    }
    std::vector<runtime::WorkKey> dependencies() const {
        std::set<runtime::WorkKey> keys;for(const auto& input:inputs_)keys.insert(input.producer);return {keys.begin(),keys.end()};
    }
    const PreparedPackedBinary& prototype() const & noexcept {return prototype_;}
    const PreparedPackedBinary& prototype() const &&=delete;
    const PackedWorkCarrier& carrier() const & noexcept {return carrier_;}
    const PackedWorkCarrier& carrier() const &&=delete;
    struct Input {
        PreparedPackedBinary operation;std::vector<std::shared_ptr<const PackedWorkResult>> predecessors;
        std::vector<PackedWorkSource> sources;
    };
    Input materialize(const runtime::WorkInputs& supplied,const std::optional<runtime::WorkAvailability>& cutoff={}) const {
        composite::AspectPlanes planes(schema_,carrier_.slots(),limits_.retained_bytes/8);
        std::vector<std::shared_ptr<const PackedWorkResult>> parents;std::vector<PackedWorkSource> sources;std::set<runtime::WorkKey> seen;
        for(std::size_t aspect=0;aspect<inputs_.size();++aspect) {
            const auto& binding=inputs_[aspect];const auto parent=supplied.at(binding.producer).as<PackedWorkResult>();
            if(parent->key!=binding.producer || !carrier_.same_carrier(parent->carrier))throw std::invalid_argument("packed result carrier/generation mismatch");
            binary_work_detail::availability(parent->sources,cutoff);
            if(seen.insert(binding.producer).second){
                parents.push_back(parent);
                for(const auto& source:parent->sources)if(std::none_of(sources.begin(),sources.end(),[&](const auto& retained){return retained.publication.same_owner(source.publication);}))sources.push_back(source);
            }
            planes.plane(static_cast<composite::AspectId>(aspect))=parent->outputs.at(binding.output);
        }
        return {PreparedPackedBinary(prototype_.program(),planes,prototype_.outputs(),limits_),std::move(parents),std::move(sources)};
    }
    PackedWorkResult result(runtime::WorkKey key,Input input,std::vector<machine::PackedBits> output,
                            std::optional<PackedWorkDeviceReport> device={}) const {
        return {key,carrier_,std::move(input.operation),std::move(output),inputs_,std::move(input.predecessors),std::move(input.sources),std::move(device)};
    }
private:
    PackedWorkCarrier carrier_;composite::AspectSchema schema_;std::vector<PackedWorkBinding> inputs_;
    PackedBinaryLimits limits_;PreparedPackedBinary prototype_;
};
inline runtime::WorkJob make_cpu_binary_work(runtime::WorkKey key,const PackedWorkRecipe& recipe,
    std::size_t workers=1,std::size_t tile_words=64,std::size_t result_limit=std::size_t{1}<<30,
    std::optional<runtime::WorkAvailability> cutoff={}) {
    binary_work_detail::validate(recipe.prototype(),recipe.carrier(),workers,tile_words,result_limit);
    return runtime::WorkJob::make<PackedWorkResult>(key,binary_work_detail::demand(recipe.prototype(),workers,true),recipe.dependencies(),
        [key,recipe,workers,tile_words,result_limit,cutoff](const runtime::WorkInputs& inputs,runtime::CpuResourceCoordinator& assigned,std::stop_token stop) {
            auto input=recipe.materialize(inputs,cutoff);
            auto result=binary_work_detail::cpu(input.operation,workers,tile_words,result_limit,assigned,stop);
            return recipe.result(key,std::move(input),std::move(result));
        });
}
} // namespace rfc::backend
