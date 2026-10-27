#pragma once
#include "rfc/storage/certified_stalk_colimit.hpp"

namespace rfc::storage {
// The coordinate record is shared with the finite colimit layer. The arrows here
// go FROM a local observation stalk TO an interface, not the other way round.
using ObservationCoordinateSpace = ColimitCoordinateSpace;
struct ObservationLinearLink {
    std::string name;
    std::size_t left{}, right{};
    ObservationCoordinateSpace common;
    std::array<numeric::ExactMatrix,2> restrictions;
    bool operator==(const ObservationLinearLink&) const = default;
};
// W=sum(local observations), K=sum(interface observations), C:W->K is the
// actual difference of the two restriction maps on each interface. A:X->W,
// E:W->X and H:K->W certify EA=I, CA=0, AE+HC=I. Thus im A=ker C exactly.
// E is used ONLY after compatibility is checked. It is not a denoiser, inferred
// average, metric adjoint, or upward incidence map. No complement is discovered.
struct StalkObservationWitness {
    ObservationCoordinateSpace target;
    numeric::ExactMatrix accession, reconstruction, compatibility_lift;
    bool operator==(const StalkObservationWitness&) const = default;
};
class ObservationCompatibilityError : public std::invalid_argument {
    numeric::SparseRationalVector residual_;
public:
    explicit ObservationCompatibilityError(numeric::SparseRationalVector residual)
        : std::invalid_argument("local observations disagree on declared interfaces"), residual_(std::move(residual)) {}
    const numeric::SparseRationalVector& residual() const & noexcept { return residual_; }
    const numeric::SparseRationalVector& residual() const && = delete;
};

class CertifiedStalkObservations {
    using M=numeric::ExactMatrix; using S=numeric::SparseRationalVector;
    using G=numeric::ExactActionGuard; using Q=numeric::Rational;
    struct State {
        std::vector<ObservationCoordinateSpace> spaces;
        std::vector<ObservationLinearLink> links;
        StalkObservationWitness witness;
        std::vector<std::size_t> offsets, interface_offsets;
        M compatibility;
        std::vector<M> readouts, contributions;
        std::size_t dimension{}, records{};
    };
    std::shared_ptr<const State> state_;
    static S tuple(const std::shared_ptr<const State>& pin,const std::vector<S>& values,G& g) {
        using namespace certified_colimit_detail;
        require(values.size()==pin->spaces.size(),"one total observation per local space required");
        std::size_t held=pin->records; std::map<std::size_t,Q> entries;
        for(std::size_t p=0;p<values.size();++p){
            require(values[p].size()==pin->offsets[p+1]-pin->offsets[p],"local observation carrier differs");
            g.sparse_vector(values[p]); held=add(held,values[p].nonzeros()); g.entries(held);
        }
        for(std::size_t p=0;p<values.size();++p)for(const auto& e:values[p].entries())
            accumulate(entries,add(pin->offsets[p],e.index),e.value,g,held);
        return numeric::checked_sparse_detail::vector(pin->offsets.back(),std::move(entries),g);
    }
public:
    CertifiedStalkObservations(std::vector<ObservationCoordinateSpace> spaces,
        std::vector<ObservationLinearLink> links,StalkObservationWitness witness,G& g){
        using namespace certified_colimit_detail;
        g.step(); require(!spaces.empty(),"observation atlas needs local spaces");
        auto s=std::make_shared<State>(State{std::move(spaces),std::move(links),std::move(witness),{0},{0},M{}, {},{},0,0});
        std::set<std::string> names; std::vector<composite::Declaration> definitions;
        s->records=add(s->spaces.size(),s->links.size()); g.entries(s->records);
        for(const auto& c:s->spaces){
            g.step();require(names.insert(c.name).second,"duplicate observation space identity");
            s->offsets.push_back(add(s->offsets.back(),extent(c,g)));g.dimension(s->offsets.back());definitions.push_back(c.declaration);
        }
        auto& w=s->witness; s->dimension=extent(w.target,g); definitions.push_back(w.target.declaration);
        const auto n=s->offsets.back(),m=s->dimension; names.clear();
        for(const auto& e:s->links){
            g.step();require(!e.name.empty() && names.insert(e.name).second,"duplicate observation interface identity");
            require(e.left<s->spaces.size() && e.right<s->spaces.size() && e.left!=e.right,"observation interface endpoints");
            s->interface_offsets.push_back(add(s->interface_offsets.back(),extent(e.common,g)));
            g.dimension(s->interface_offsets.back());definitions.push_back(e.common.declaration);
        }
        (void)composite::DeclarationRegistry(std::move(definitions));
        const auto kd=s->interface_offsets.back();
        const auto retain=[&](const M& a){g.matrix(a);s->records=add(s->records,a.nonzeros());g.entries(s->records);};
        retain(w.accession);retain(w.reconstruction);retain(w.compatibility_lift);
        require(w.accession.rows()==n && w.accession.cols()==m && w.reconstruction.rows()==m && w.reconstruction.cols()==n &&
                w.compatibility_lift.rows()==n && w.compatibility_lift.cols()==kd,"observation certificate shapes");
        s->compatibility=M(kd,n);
        for(std::size_t ei=0;ei<s->links.size();++ei){const auto& e=s->links[ei];
            const auto d=e.common.declaration.axes[0].extent; const std::array<std::size_t,2> ps{e.left,e.right};
            for(std::size_t side=0;side<2;++side){const auto p=ps[side];const auto& f=e.restrictions[side];retain(f);
                require(f.rows()==d && f.cols()==s->offsets[p+1]-s->offsets[p],"observation restriction shape");
                for(const auto& [ij,q]:f.entries())set(s->compatibility,add(s->interface_offsets[ei],ij.first),add(s->offsets[p],ij.second),side?Q{-q}:q,g,s->records);
            }
        }
        retain(s->compatibility);
        require(identity(product(w.reconstruction,w.accession,g,s->records),g),"observation reconstruction is not a left inverse");
        require(product(s->compatibility,w.accession,g,s->records).nonzeros()==0,"observation accession violates interface compatibility");
        const auto ae=product(w.accession,w.reconstruction,g,s->records);
        const auto hc=product(w.compatibility_lift,s->compatibility,g,add(s->records,ae.nonzeros()));
        require(sum_is_identity(ae,hc,g,s->records),"observation certificate omits compatible directions");
        for(std::size_t p=0;p<s->spaces.size();++p){const auto off=s->offsets[p],d=s->offsets[p+1]-off;
            auto a=block(w.accession,off,d,0,m,g,s->records);auto e=block(w.reconstruction,0,m,off,d,g,s->records);
            retain(a);retain(e);s->readouts.push_back(std::move(a));s->contributions.push_back(std::move(e));
        }
        state_=std::move(s);
    }
    std::size_t dimension()const {return state_->dimension;}
    std::size_t part_count()const {return state_->spaces.size();}
    std::size_t stored_entries()const {return state_->records;}
    const StalkObservationWitness& witness()const & {return state_->witness;}
    const StalkObservationWitness& witness()const &&=delete;
    const M& compatibility()const & {return state_->compatibility;}
    const M& compatibility()const &&=delete;
    const std::vector<std::size_t>& interface_offsets()const & {return state_->interface_offsets;}
    const std::vector<std::size_t>& interface_offsets()const &&=delete;
    S restrict(std::size_t p,const S& x,G& g)const {const auto pin=state_;return certified_colimit_detail::apply(pin->readouts.at(p),x,g,pin->records);}
    S restrict_transpose(std::size_t p,const S& x,G& g)const {const auto pin=state_;return certified_colimit_detail::apply(pin->readouts.at(p),x,g,pin->records,true);}
    // A single E_p output is an additive reconstruction channel, not a section
    // restriction or an independent physical reinsertion of a duplicated value.
    S contribution(std::size_t p,const S& x,G& g)const {const auto pin=state_;return certified_colimit_detail::apply(pin->contributions.at(p),x,g,pin->records);}
    S contribution_transpose(std::size_t p,const S& x,G& g)const {const auto pin=state_;return certified_colimit_detail::apply(pin->contributions.at(p),x,g,pin->records,true);}
    S compatibility_residual(const std::vector<S>& values,G& g)const {
        const auto pin=state_;const auto v=tuple(pin,values,g);return certified_colimit_detail::apply(pin->compatibility,v,g,pin->records);
    }
    S glue(const std::vector<S>& values,G& g)const {
        using namespace certified_colimit_detail;
        const auto pin=state_;const auto v=tuple(pin,values,g);const auto residual=apply(pin->compatibility,v,g,pin->records);
        if(residual.nonzeros())throw ObservationCompatibilityError(residual);
        const auto result=apply(pin->witness.reconstruction,v,g,add(pin->records,residual.nonzeros()));
        // Redundant with the whole certificate; retained as an input round-trip check.
        require(apply(pin->witness.accession,result,g,add(pin->records,v.nonzeros())).entries()==v.entries(),"compatible observation reconstruction mismatch");
        return result;
    }
};
} // namespace rfc::storage
