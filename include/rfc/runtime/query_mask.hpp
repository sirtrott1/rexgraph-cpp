#pragma once
#include "rfc/machine/packed_bits.hpp"
#include "rfc/composite/execution.hpp"
#include <stdexcept>
#include <vector>

namespace rfc::runtime {

class PreparedMaskIntersection {
public:
    explicit PreparedMaskIntersection(std::size_t count):count_(count),plan_(program(count),{0}){}
    machine::PackedBits apply(const std::vector<const machine::PackedBits*>& masks,
                            composite::BinaryExecutionWorkspace& workspace) const {
        if(masks.size()!=count_)throw std::invalid_argument("query mask count mismatch");
        for(const auto* mask:masks)if(!mask)throw std::invalid_argument("null query mask");
        const auto slots=masks.front()->size();
        for(const auto* mask:masks)if(mask->size()!=slots)throw std::invalid_argument("query mask size mismatch");
        auto out=plan_.materialize(slots,[&](auto factor,auto word){return masks.at(factor)->word(word);},workspace);
        return std::move(out.front());
    }
private:
    static composite::BinaryProgram program(std::size_t count) {
        if(!count || count>std::numeric_limits<composite::AspectId>::max())
            throw std::invalid_argument("query mask count outside packed factor domain");
        composite::BinaryProgram p(2);
        p.append({composite::BinaryOpcode::LoadAspect,0,0,0,0,0});
        for(std::size_t i=1;i<count;++i){
            p.append({composite::BinaryOpcode::LoadAspect,1,0,0,0,static_cast<composite::AspectId>(i)});
            p.append({composite::BinaryOpcode::And,0,0,1,0,0});
        }
        return p;
    }
    std::size_t count_;
    composite::BinaryExecutionPlan plan_;
};
inline machine::PackedBits intersect_masks(const std::vector<const machine::PackedBits*>& masks) {
    composite::BinaryExecutionWorkspace workspace;
    return PreparedMaskIntersection(masks.size()).apply(masks,workspace);
}

} // namespace rfc::runtime
