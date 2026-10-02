#pragma once

#include "rfc/models/native_context.hpp"
#include <map>

namespace rfc::models {
struct NativeContextCutoff {
    std::uint64_t source{},source_namespace{},max_sequence{};
    bool operator==(const NativeContextCutoff&) const=default;
};
// Cutoffs restrict availability by publication sequence. Event timestamps and
// label availability need explicit host declarations; neither is inferred here.
class NativeTemporalContext {
public:
    static NativeTemporalContext build(const storage::NativeAtlas& atlas,
        const std::vector<NativeContextSelection>& selections,std::vector<NativeContextCutoff> cutoffs,
        const NativeContextAuthorizer& authorize,const NativeContextLimits& limits={}) {
        if(!authorize)throw std::invalid_argument("temporal context denies without host authorization");
        if(cutoffs.size()>limits.max_items)throw std::length_error("temporal cutoff count budget");
        std::map<std::uint64_t,NativeContextCutoff> by_source;
        for(const auto& cutoff:cutoffs) {
            if(!cutoff.source || !cutoff.source_namespace || !by_source.emplace(cutoff.source,cutoff).second)
                throw std::invalid_argument("temporal cutoff requires unique source and namespace");
        }
        const auto policy=authorize;
        auto context=NativeConsumerContext::build(atlas,selections,
            [by_source,policy](const NativeContextAuthorization& origin) {
                const auto it=by_source.find(origin.source);
                return it!=by_source.end() && it->second.source_namespace==origin.source_namespace &&
                    origin.sequence<=it->second.max_sequence && policy(origin);
            },limits);
        return NativeTemporalContext(std::move(context),std::move(cutoffs));
    }
    const std::vector<NativeContextItem>& items() const & noexcept {return context_.items();}
    const std::vector<NativeContextItem>& items() const &&=delete;
    const std::vector<NativeContextCutoff>& cutoffs() const & noexcept {return cutoffs_;}
    const std::vector<NativeContextCutoff>& cutoffs() const &&=delete;
    NativeModelProposal propose(ProspectiveEncoding encoding,std::string model,std::string interpretation,
                               std::vector<std::byte> bytes) const {
        return context_.propose(encoding,std::move(model),std::move(interpretation),std::move(bytes));
    }
private:
    NativeTemporalContext(NativeConsumerContext context,std::vector<NativeContextCutoff> cutoffs)
        :context_(std::move(context)),cutoffs_(std::move(cutoffs)){}
    NativeConsumerContext context_;
    std::vector<NativeContextCutoff> cutoffs_;
};
} // namespace rfc::models
