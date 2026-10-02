#pragma once
#include "rfc/storage/native_accession.hpp"
#include <bit>

namespace rfc::storage {

// Trusted-host execution over one authentic immutable publication. No source
// planes are copied. The retained declaration/program remains the identity;
// the binary plan only assigns physical registers and reuses private scratch.
class PreparedNativeSourceProgram {
public:
    PreparedNativeSourceProgram(const PinnedNativeAccession& source,std::uint64_t key,
                                composite::BinaryExecutionLimits limits = {}) {
        using native_accession_detail::require;
        require(source.kind()==NativeAccessionKind::SourceAspects,"program requires source aspect accession");
        const auto descriptor=source.descriptor();
        const auto request=descriptor.request();
        require(request.occurrence.has_value(),"source program requires a retained occurrence");
        const auto endpoint=source.endpoint();
        require(endpoint->declarations.has_value(),"source program requires declarations");
        const auto& declarations=*endpoint->declarations;
        const auto programs=temporal_declaration_detail::keyed(declarations.programs);
        const auto layouts=temporal_declaration_detail::keyed(declarations.source_layouts);
        require(programs.contains(key),"declared source program absent");
        const auto& program=*programs.at(key);
        require(program.layout==request.subject && layouts.contains(program.layout),"program has different declared layout");
        const auto& layout=*layouts.at(program.layout);
        core::SourceAspectLayoutBinding binding(endpoint->source.signature(),
            composite::CompiledLayout(declarations.registry,layout.type,declarations.profile),layout.factors);
        const auto grade=request.occurrence->upper_grade;
        const auto slots=endpoint->source.grade(grade).aspects.slots();
        const auto work=machine::checked_size_mul(machine::PackedBits::words_for_size(slots),program.program.instruction_count());
        if(work>source.limits().max_action_work)throw std::length_error("source program accession work budget");
        composite::BinaryExecutionPlan plan(program.program,{program.output},limits);
        plan.validate_aspects(binding.source_aspects().size()); // even an empty domain
        state_=std::make_shared<const State>(source,key,grade,binding.source_aspects(),std::move(plan));
    }
    template<class Sink>
    void for_each_word(Sink&& sink,composite::BinaryExecutionWorkspace& workspace) const {
        const auto pin=state_; // the callback may replace the caller's wrapper
        const auto& planes=pin->source.endpoint()->source.grade(pin->grade).aspects;
        pin->plan.for_each_word(planes.slots(),
            [&](auto factor,auto word){return planes.plane(pin->aspects.at(factor)).word(word);},
            std::forward<Sink>(sink),workspace);
    }
    [[nodiscard]] machine::PackedBits materialize(composite::BinaryExecutionWorkspace& workspace) const {
        const auto pin=state_;
        const auto& planes=pin->source.endpoint()->source.grade(pin->grade).aspects;
        auto result=pin->plan.materialize(planes.slots(),
            [&](auto factor,auto word){return planes.plane(pin->aspects.at(factor)).word(word);},workspace);
        return std::move(result.front());
    }
    [[nodiscard]] std::size_t count(composite::BinaryExecutionWorkspace& workspace) const {
        std::size_t result=0;
        for_each_word([&](auto,std::span<const composite::BinaryProgram::word_type> values){
            result+=static_cast<std::size_t>(std::popcount(values.front()));
        },workspace);
        return result;
    }
    [[nodiscard]] std::uint64_t program_key() const noexcept {return state_->key;}
    [[nodiscard]] PinnedNativeAccession publication() const {return state_->source;}
private:
    struct State {
        PinnedNativeAccession source;
        std::uint64_t key;
        core::Grade grade;
        std::vector<composite::AspectId> aspects;
        composite::BinaryExecutionPlan plan;
        State(PinnedNativeAccession pin,std::uint64_t k,core::Grade g,std::vector<composite::AspectId> a,
              composite::BinaryExecutionPlan p):source(std::move(pin)),key(k),grade(g),aspects(std::move(a)),plan(std::move(p)){}
    };
    // Copying also handles rvalues and keeps both handles usable, as accession does.
    std::shared_ptr<const State> state_;
public:
    PreparedNativeSourceProgram(const PreparedNativeSourceProgram&)=default;
    PreparedNativeSourceProgram& operator=(const PreparedNativeSourceProgram&)=default;
};
} // namespace rfc::storage
