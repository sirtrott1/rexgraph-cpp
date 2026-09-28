#pragma once
#include "rfc/database/native_interpretation.hpp"

namespace rfc::database {
// A bounded LOGICAL manifest, not an Iceberg/Parquet/cloud-file reader. Its
// fragments have already been interpreted under one explicit declaration.
struct LogicalFragment {
    std::string identity;
    std::vector<ExternalOrigin> subjects;
    std::vector<ExternalAssertion> assertions;
};
struct LogicalSnapshotManifest {
    std::string revision;
    std::vector<LogicalFragment> fragments;
    std::vector<ExternalOrigin> deletions; // exact assertion lineages, never value predicates
};
inline LogicalSnapshot materialize_logical_snapshot(const LogicalSnapshotManifest& manifest,
                                                     const InterpretationLimits& limits={}) {
    using namespace interpretation_detail;
    std::size_t bytes=0,records=0,nodes=0,value_bytes=0;token(manifest.revision,bytes,limits);
    std::set<std::string> fragments;std::set<ExternalOrigin> subjects,deletes;
    std::map<ExternalOrigin,const ExternalAssertion*> assertions;
    add(records,manifest.fragments.size(),limits.max_records,"logical manifest fragment budget");
    for(const auto& fragment:manifest.fragments) {
        token(fragment.identity,bytes,limits);require(fragments.insert(fragment.identity).second,"duplicate logical fragment identity");
        add(records,fragment.subjects.size(),limits.max_records,"logical manifest subject budget");
        std::set<ExternalOrigin> local_subjects;
        for(const auto& s:fragment.subjects){origin(s,bytes,limits);require(local_subjects.insert(s).second,"duplicate subject in one fragment");subjects.insert(s);}
        add(records,fragment.assertions.size(),limits.max_records,"logical manifest assertion budget");
        for(const auto& a:fragment.assertions){
            origin(a.origin,bytes,limits);account_value_copy(a.payload,limits,nodes,value_bytes);
            add(records,a.roles.size(),limits.max_records,"logical manifest role budget");
            for(const auto& r:a.roles){add(records,r.occurrences.size(),limits.max_records,"logical manifest occurrence budget");
                for(const auto& o:r.occurrences){token(o.lineage,bytes,limits);origin(o.participant,bytes,limits);}}
            require(assertions.emplace(a.origin,&a).second,"duplicate assertion lineage across fragments");
        }
    }
    add(records,manifest.deletions.size(),limits.max_records,"logical manifest delete budget");
    for(const auto& d:manifest.deletions){origin(d,bytes,limits);require(deletes.insert(d).second && assertions.erase(d)==1,"duplicate/unmatched logical delete");}
    LogicalSnapshot out;out.revision=manifest.revision;out.subjects.assign(subjects.begin(),subjects.end());
    out.assertions.reserve(assertions.size());for(const auto& [key,value]:assertions){(void)key;out.assertions.push_back(*value);}return out;
}
inline InterpretedSnapshot interpret_logical_manifest(const InterpretationSchema& schema,const LogicalSnapshotManifest& manifest,
                                                      const InterpretationLimits& limits={}) {
    return InterpretedSnapshot::interpret(schema,materialize_logical_snapshot(manifest,limits),limits);
}
} // namespace rfc::database
