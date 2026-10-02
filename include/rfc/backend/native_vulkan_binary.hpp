#pragma once

#include "rfc/backend/vulkan_binary.hpp"
#include "rfc/storage/prepared_source_program.hpp"

namespace rfc::backend {
// Source validation and carrier identity come from the authenticated accession.
// The physical factor copy is prospective; the complete publication stays pinned.
class PreparedNativeVulkanBinary {
    struct State {
        storage::PreparedNativeSourceProgram source;
        PreparedPackedBinary packed;
        State(storage::PreparedNativeSourceProgram s,PreparedPackedBinary p):source(std::move(s)),packed(std::move(p)){}
    };
public:
    PreparedNativeVulkanBinary(const storage::PinnedNativeAccession& source,std::uint64_t key,
                              PackedBinaryLimits limits={}) {
        storage::PreparedNativeSourceProgram trusted(source,key,{limits.instructions,limits.outputs,
            limits.retained_bytes/sizeof(std::uint64_t)});
        const auto endpoint=source.endpoint();const auto& declarations=*endpoint->declarations;
        const auto programs=storage::temporal_declaration_detail::keyed(declarations.programs);
        const auto layouts=storage::temporal_declaration_detail::keyed(declarations.source_layouts);
        const auto& program=*programs.at(key);const auto& layout=*layouts.at(program.layout);
        core::SourceAspectLayoutBinding binding(endpoint->source.signature(),
            composite::CompiledLayout(declarations.registry,layout.type,declarations.profile),layout.factors);
        const auto descriptor=source.descriptor();
        const auto grade=descriptor.request().occurrence->upper_grade;
        const auto& planes=endpoint->source.grade(grade).aspects;
        composite::AspectSchema schema;
        for(std::size_t i=0;i<binding.source_aspects().size();++i)schema.add("factor_"+std::to_string(i));
        const auto words=machine::PackedBits::words_for_size(planes.slots());
        composite::AspectPlanes factors(schema,planes.slots(),limits.retained_bytes/sizeof(std::uint64_t));
        for(std::size_t a=0;a<binding.source_aspects().size();++a)for(std::size_t w=0;w<words;++w)
            factors.plane(static_cast<composite::AspectId>(a)).assign_word(w,planes.plane(binding.source_aspects()[a]).word(w));
        state_=std::make_shared<const State>(std::move(trusted),PreparedPackedBinary(program.program,factors,{program.output},limits));
    }
    PreparedNativeVulkanBinary(const PreparedNativeVulkanBinary&)=default;
    PreparedNativeVulkanBinary& operator=(const PreparedNativeVulkanBinary&)=default;
    const PreparedPackedBinary& packed() const & noexcept {return state_->packed;}
    const PreparedPackedBinary& packed() const &&=delete;
    storage::PinnedNativeAccession publication() const {return state_->source.publication();}
    std::uint64_t program_key() const noexcept {return state_->source.program_key();}
    struct Result {
        storage::PinnedNativeAccession publication;
        std::uint64_t program_key;
        MixedBinaryResult execution;
    };
    Result execute(VulkanBinaryWorkspace& workspace,runtime::CpuResourceCoordinator& resources,const MixedBinaryPolicy& policy) const {
        const auto pin=state_;
        return {pin->source.publication(),pin->source.program_key(),workspace.execute(pin->packed,resources,policy)};
    }
private:std::shared_ptr<const State> state_;
};
} // namespace rfc::backend
