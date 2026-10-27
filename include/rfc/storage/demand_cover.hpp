#pragma once
#include "rfc/storage/payload_manifest.hpp"
#include <map>
#include <memory>

namespace rfc::storage {
// Explicit quotient maps, in the ORIGINAL manifest cell/occurrence order.
// No equal local key, coefficient or dimension implicitly identifies sources.
struct DemandPatch {
    PayloadKey payload;
    std::vector<PayloadCell> cells;
    std::vector<std::uint64_t> occurrences;
    std::uint64_t available_at{};
    bool operator==(const DemandPatch&) const = default;
};
struct DemandCoverDeclaration {
    std::uint64_t name_space{},key{},revision{},evidence_namespace{},cutoff{},parameter_revision{},arrangement_revision{};
    std::vector<DemandPatch> patches;
    bool operator==(const DemandCoverDeclaration&) const = default;
};
struct DemandCoverLimits {std::size_t max_encoded_bytes{16u<<20},max_index_bytes{128u<<20},max_records{1000000};};
// A deliberately finite first demand-gluing profile: complete grade-zero/one
// sources, explicit whole-cell/occurrence identifications, scalar identity maps
// (or their explicitly declared coordinate-wise direct sums) and unit metrics.
class DemandCover {
    struct State {
        PayloadManifest manifest;DemandCoverDeclaration declaration;std::vector<std::byte> bytes;
        std::map<PayloadCell,std::vector<std::pair<std::size_t,std::size_t>>> cells;
        std::map<std::uint64_t,std::vector<std::uint64_t>> adjacency;
        std::size_t index_bytes{};
        explicit State(PayloadManifest m):manifest(std::move(m)){}
    };
    std::shared_ptr<const State> state_;
    explicit DemandCover(std::shared_ptr<const State> s):state_(std::move(s)){}
public:
    static DemandCover create(const PayloadManifest& manifest,const DemandCoverDeclaration& declaration,
        DemandCoverLimits limits={},std::function<void()> check={}) {
        using namespace payload_manifest_detail;
        // Inspect caller-owned sizes without callbacks, precharge, then freeze.
        std::size_t records=declaration.patches.size();
        std::size_t encoded=80;
        for(const auto& p:declaration.patches){records=checked_size_add(records,checked_size_add(p.cells.size(),p.occurrences.size()));
            encoded=checked_size_add(encoded,checked_size_add(64,checked_size_add(checked_size_mul(p.cells.size(),10),checked_size_mul(p.occurrences.size(),8))));}
        bound(encoded,limits.max_encoded_bytes,"demand cover wire preadmission");
        bound(records,limits.max_records,"demand cover record admission");
        const auto charge=checked_size_add(sizeof(State),checked_size_mul(records,1024));
        bound(charge,limits.max_index_bytes,"demand cover index admission");
        auto s=std::make_shared<State>(manifest);s->declaration=declaration;s->index_bytes=charge;
        const auto& d=s->declaration;
        require(d.name_space && d.key && d.revision && d.evidence_namespace && d.parameter_revision && d.arrangement_revision,
            "demand cover needs distinct declared producer/evidence/parameter/arrangement identities");
        std::set<PayloadKey> patches;std::map<std::pair<std::uint16_t,std::uint64_t>,PayloadCell> occurrences;
        using Fiber=std::vector<std::tuple<std::uint64_t,PayloadCell,std::uint64_t,bool>>;
        std::map<PayloadCell,Fiber> fibers;
        rfcx_detail::Writer w;w.u64(0x3156474458434652ULL);w.u64(1);
        for(auto x:{d.name_space,d.key,d.revision,d.evidence_namespace,d.cutoff,d.parameter_revision,d.arrangement_revision})w.u64(x);
        w.u64(d.patches.size());
        for(std::size_t pi=0;pi<d.patches.size();++pi) {
            if(check){check();}const auto& p=d.patches[pi];const auto at=s->manifest.find(p.payload);
            require(at.has_value() && patches.insert(p.payload).second,"unknown/repeated demand patch selector");
            const auto& original=s->manifest.payloads()[*at];
            require(original.format=="RFCX-1" && original.scope==PayloadScope::BoundaryClosed,"demand cover needs canonical whole RFCX sources");
            require(p.cells.size()==original.cells.size() && p.occurrences.size()==original.occurrences.size(),"demand map omits original cells/occurrences");
            std::map<PayloadCell,PayloadCell> mapped;std::set<PayloadCell> injective;
            key(w,p.payload);w.u64(p.available_at);w.u64(p.cells.size());
            for(std::size_t ci=0;ci<p.cells.size();++ci) {
                const auto a=original.cells[ci],b=p.cells[ci];
                require(a.grade==b.grade && b.grade.value<=1 && b.key && injective.insert(b).second,"demand cell map changes grade, identifies a local cell or exceeds the graph profile");
                mapped.emplace(a,b);s->cells[b].emplace_back(pi,ci);cell(w,b);
            }
            std::map<PayloadCell,Fiber> local;std::set<std::uint64_t> unique;w.u64(p.occurrences.size());
            for(std::size_t oi=0;oi<p.occurrences.size();++oi) {
                const auto& o=original.occurrences[oi];const auto upper=mapped.at(o.upper),lower=mapped.at(o.lower);const auto target=p.occurrences[oi];
                require(target && unique.insert(target).second,"demand occurrence map identifies distinct original occurrences");
                const auto [pos,added]=occurrences.emplace(std::pair{upper.grade.value,target},upper);
                require(added || pos->second==upper,"global occurrence assigned to distinct upper cells");
                local[upper].emplace_back(target,lower,o.position,o.exists);w.u64(target);
            }
            for(const auto& [a,b]:mapped)if(b.grade.value) {
                const auto [f,added]=fibers.emplace(b,local[b]);
                require(added || f->second==local[b],"shared complete occurrence fiber/attaching order differs");
                (void)a;
            }
        }
        for(const auto& p:d.patches)for(const auto& dependency:s->manifest.payloads()[*s->manifest.find(p.payload)].dependencies)
            require(patches.contains(dependency),"demand cover omits a declared payload dependency");
        // Numerical cancellation never erases represented occurrences here.
        for(const auto& [upper,fiber]:fibers)for(const auto& [occ,lower,pos,exists]:fiber) {
            (void)occ;(void)pos;
            if(exists)s->adjacency[lower.key].push_back(upper.key);
        }
        for(auto& [v,edges]:s->adjacency){(void)v;std::sort(edges.begin(),edges.end());edges.erase(std::unique(edges.begin(),edges.end()),edges.end());}
        auto wire=std::move(w).take_bytes();bound(wire.size(),limits.max_encoded_bytes,"demand cover wire admission");s->bytes=std::move(wire);
        return DemandCover(std::move(s));
    }
    static DemandCover decode(std::span<const std::byte> input,const PayloadManifest& original,DemandCoverLimits limits={}) {
        using namespace payload_manifest_detail;bound(input.size(),limits.max_encoded_bytes,"demand cover receiving admission");
        const std::vector<std::byte> frozen(input.begin(),input.end());rfcx_detail::Reader r(frozen);
        require(r.u64()==0x3156474458434652ULL && r.u64()==1,"unknown demand cover profile");
        DemandCoverDeclaration d;d.name_space=r.u64();d.key=r.u64();d.revision=r.u64();d.evidence_namespace=r.u64();d.cutoff=r.u64();d.parameter_revision=r.u64();d.arrangement_revision=r.u64();
        const auto n=r.size_value();bound(n,limits.max_records,"demand patch receiving admission");
        std::size_t records=n;
        for(std::size_t i=0;i<n;++i){DemandPatch p;p.payload=key(r);p.available_at=r.u64();auto count=r.size_value();records=checked_size_add(records,count);bound(records,limits.max_records,"demand cell receiving admission");
            bound(checked_size_mul(records,1024),limits.max_index_bytes,"demand map receiving bytes");
            for(std::size_t j=0;j<count;++j){p.cells.push_back(cell(r));}
            count=r.size_value();records=checked_size_add(records,count);bound(records,limits.max_records,"demand occurrence receiving admission");
            bound(checked_size_mul(records,1024),limits.max_index_bytes,"demand map receiving bytes");
            for(std::size_t j=0;j<count;++j){p.occurrences.push_back(r.u64());}
            d.patches.push_back(std::move(p));}
        auto out=create(original,d,limits);require(out.bytes().size()==frozen.size() && std::ranges::equal(out.bytes(),frozen),"noncanonical demand cover image");return out;
    }
    const PayloadManifest& manifest() const & {return state_->manifest;}
    const PayloadManifest& manifest() const &&=delete;
    const DemandCoverDeclaration& declaration() const & {return state_->declaration;}
    const DemandCoverDeclaration& declaration() const &&=delete;
    std::span<const std::byte> bytes() const & {return state_->bytes;}
    std::span<const std::byte> bytes() const &&=delete;
    const auto& cells() const & {return state_->cells;}
    const auto& cells() const &&=delete;
    const auto& adjacency() const & {return state_->adjacency;}
    const auto& adjacency() const &&=delete;
    std::size_t index_bytes() const {return state_->index_bytes;}
    bool same_owner(const DemandCover& other) const {return state_==other.state_;}
};
} // namespace rfc::storage
