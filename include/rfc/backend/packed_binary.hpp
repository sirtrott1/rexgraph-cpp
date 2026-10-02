#pragma once

#include "rfc/composite/execution.hpp"
#include "rfc/runtime/cpu_tiles.hpp"
#include <memory>
#include <span>
#include <stop_token>

namespace rfc::backend {
struct PackedBinaryLimits {
    std::size_t instructions{1000000}, outputs{65535}, retained_bytes{std::size_t{1} << 30};
};
// Frozen complete input. This API accepts no live callbacks or mutable providers.
class PreparedPackedBinary {
    struct State {
        composite::BinaryProgram program;
        composite::AspectPlanes planes;
        std::vector<std::uint16_t> outputs;
        composite::BinaryExecutionPlan plan;
        State(const composite::BinaryProgram& p,const composite::AspectPlanes& a,
              std::vector<std::uint16_t> o):program(p),planes(a),outputs(std::move(o)),
              plan(program,outputs.empty()?std::vector<std::uint16_t>{0}:outputs,
                   {p.instruction_count(),std::max(std::size_t{1},outputs.size()),std::numeric_limits<std::size_t>::max()}){}
    };
public:
    PreparedPackedBinary(const composite::BinaryProgram& program,const composite::AspectPlanes& planes,
                         std::vector<std::uint16_t> outputs,PackedBinaryLimits limits={}) {
        if(program.instruction_count()>limits.instructions || outputs.size()>limits.outputs)
            throw std::length_error("packed binary preparation limits");
        for(auto output:outputs)if(output>=program.register_count())throw std::out_of_range("packed output register");
        for(const auto& i:program.instructions())if(i.opcode==composite::BinaryOpcode::LoadAspect && i.aspect>=planes.aspect_count())
            throw std::invalid_argument("packed source aspect absent");
        const auto words=machine::PackedBits::words_for_size(planes.slots());
        for(std::size_t a=0;a<planes.aspect_count();++a)
            if(planes.plane(static_cast<composite::AspectId>(a)).size()!=planes.slots())
                throw std::invalid_argument("packed source plane domain changed");
        auto bytes=machine::checked_size_mul(machine::checked_size_mul(words,planes.aspect_count()),sizeof(std::uint64_t));
        bytes=machine::checked_size_add(bytes,machine::checked_size_mul(program.instruction_count(),2*sizeof(composite::BinaryInstruction)));
        const auto output_labels=machine::checked_size_add(outputs.size(),std::max(std::size_t{1},outputs.size()));
        bytes=machine::checked_size_add(bytes,machine::checked_size_mul(output_labels,sizeof(std::uint16_t)));
        bytes=machine::checked_size_add(bytes,machine::checked_size_mul(program.register_count(),sizeof(std::uint16_t)));
        if(bytes>limits.retained_bytes)throw std::length_error("packed retained byte budget");
        state_=std::make_shared<const State>(program,planes,std::move(outputs));
    }
    PreparedPackedBinary(const PreparedPackedBinary&)=default;
    PreparedPackedBinary& operator=(const PreparedPackedBinary&)=default;
    const composite::BinaryProgram& program() const & noexcept {return state_->program;}
    const composite::AspectPlanes& planes() const & noexcept {return state_->planes;}
    const std::vector<std::uint16_t>& outputs() const & noexcept {return state_->outputs;}
    const composite::BinaryProgram& program() const &&=delete;
    const composite::AspectPlanes& planes() const &&=delete;
    const std::vector<std::uint16_t>& outputs() const &&=delete;
    std::size_t words() const noexcept {return machine::PackedBits::words_for_size(state_->planes.slots());}
    bool same_owner(const PreparedPackedBinary& other) const noexcept {return state_==other.state_;}
    std::vector<machine::PackedBits> empty_result(std::size_t byte_limit) const {
        const auto bytes=machine::checked_size_mul(machine::checked_size_mul(words(),outputs().size()),sizeof(std::uint64_t));
        if(bytes>byte_limit)throw std::length_error("packed result byte budget");
        std::vector<machine::PackedBits> result;result.reserve(outputs().size());
        for(std::size_t i=0;i<outputs().size();++i)result.emplace_back(planes().slots(),false,words());
        return result;
    }
    void cpu_words(std::size_t begin,std::size_t end,std::vector<machine::PackedBits>& result,
                   std::stop_token stop={}) const {
        if(begin>end || end>words() || result.size()!=outputs().size())throw std::invalid_argument("packed CPU range/output");
        for(const auto& out:result)if(out.size()!=planes().slots())throw std::invalid_argument("packed CPU output domain");
        if(begin==end)return;
        const auto pin=state_;
        const auto final_bits=end==words() && planes().slots()%64?planes().slots()%64:std::size_t{64};
        const auto local_slots=machine::checked_size_add(machine::checked_size_mul(end-begin-1,64),final_bits);
        composite::BinaryExecutionWorkspace scratch;
        pin->plan.for_each_word(local_slots,
            [&](auto aspect,auto word){
                if(stop.stop_requested())throw std::runtime_error("packed execution cancelled");
                return pin->planes.plane(aspect).word(begin+word);
            },
            [&](auto word,std::span<const std::uint64_t> values){
                if(stop.stop_requested())throw std::runtime_error("packed execution cancelled");
                for(std::size_t o=0;o<result.size();++o)result[o].assign_word(begin+word,values[o]);
            },scratch);
    }
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::backend
