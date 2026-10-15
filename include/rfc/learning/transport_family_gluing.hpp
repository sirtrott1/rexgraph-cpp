#pragma once
#include "rfc/learning/graded_transport_fit.hpp"
#include "rfc/learning/context_transport_views.hpp"

namespace rfc::learning {
// The common source and both complete retained maps are owned declarations.
// Views are indexed by the COMMON cells, at every grade. They retain each
// original stalk through reconstruction after accession. Only actually shared
// represented inputs are compared; no unshared parameter direction is removed.
struct TransportFamilyInterface {
    std::uint64_t key{},revision{};
    std::size_t left{},right{};
    core::RelationalComplex common;
    algebra::RetainedSourceMap common_left,common_right;
    std::vector<std::vector<TransportContextView>> left_views,right_views;
};
struct TransportFamilyEquation {
    std::size_t interface{},grade{},occurrence{},row{},column{};
};
struct TransportFamilyWitness {
    std::size_t interface{},grade{},occurrence{};
    TransportContextViewWitness input;
    std::size_t output_dimension{};
};
struct TransportFamilyParameterTerm {
    std::size_t member{},parameter{};
    numeric::Rational coefficient;
};
struct TransportFamilyClaim {
    TransportParameterRelationStatus status;
    numeric::Rational residual;
    numeric::RationalVector directions;
};

// Exact fiber product of complete affine transport families. Members may learn
// different original grades. Every retained interface occurrence is compared
// with its own sign, coefficient, type and original stalk maps. The result is
// a compatible family of owned members, not an averaging operation or an
// undeclared identification of their primary cells.
class PreparedTransportFamilyGluing {
    using Q=numeric::Rational;
    using V=numeric::RationalVector;
    using M=numeric::ExactMatrix;
    using Guard=numeric::ExactActionGuard;
    struct EntryFamily {V origin;M directions;};
    struct Equation {Q constant;std::map<std::size_t,Q> terms;};
    using Equations=std::map<M::Key,Equation>;
    std::vector<PreparedGradedTransportFit> members_;
    std::vector<TransportFamilyInterface> interfaces_;
    std::vector<TransportFamilyWitness> witnesses_;
    std::vector<TransportFamilyEquation> coordinates_;
    std::vector<TransportParameterRelationStatus> statuses_;
    std::vector<std::size_t> parameter_offsets_{0},member_free_offsets_{0},free_columns_;
    V residual_,coordinate_origin_,particular_,contradiction_;
    M equations_,coordinate_kernel_,kernel_;
    numeric::ExactActionLimits limits_;
    std::size_t retained_{},steps_{},rank_{};
    bool consistent_{};
    static std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
    static std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
    static core::Grade grade(std::size_t k){return algebra::owned_gluing_detail::grade(k);}
    void admit(Guard& guard,std::size_t scratch=0)const{guard.entries(add(retained_,scratch));}
    void retain(std::size_t n,Guard& guard){retained_=add(retained_,n);admit(guard);}
    void operation(Guard& guard)const {
        const auto& l=guard.limits();
        if(l.dimension!=limits_.dimension || l.entries!=limits_.entries || l.steps!=limits_.steps || l.integer_bits!=limits_.integer_bits)
            throw std::invalid_argument("transport family operation requires its original action limits");
        guard.step();admit(guard);
    }
    M product(const M& a,const M& b,Guard& guard,std::size_t scratch=0)const {
        return context_view_detail::product(a,b,guard,add(retained_,scratch));
    }
    static V shift(const V& origin,const M& directions,const V& free,Guard& guard) {
        auto out=numeric::checked_apply(directions,guard.canonical_input(free),guard);
        if(out.size()!=origin.size())throw std::logic_error("transport family affine shape");
        for(std::size_t i=0;i<out.size();++i)out[i]=guard.add(out[i],origin[i]);
        return out;
    }
    void view(const TransportContextView& v,std::size_t dimension,Guard& guard) {
        guard.matrix(v.accession);guard.matrix(v.reconstruction);
        if(!v.frame || v.accession.cols()!=dimension || v.reconstruction.rows()!=dimension ||
                v.accession.rows()!=v.reconstruction.cols())
            throw std::invalid_argument("complete original interface stalk view required");
        retain(add(3,add(v.accession.nonzeros(),v.reconstruction.nonzeros())),guard);
        if(product(v.reconstruction,v.accession,guard)!=M::identity(dimension))
            throw std::invalid_argument("interface reconstruction must retain every original stalk direction");
    }
    void validate_interfaces(Guard& guard) {
        std::set<std::uint64_t> keys;std::map<std::uint64_t,std::size_t> frames;
        for(const auto& interface:interfaces_) {
            guard.step();
            if(!interface.key || !keys.insert(interface.key).second || interface.left>=members_.size() || interface.right>=members_.size())
                throw std::invalid_argument("distinct interface keys and actual member indices required");
            const auto& left=members_[interface.left].model().owner().fibers();
            const auto& right=members_[interface.right].model().owner().fibers();
            const auto& common=interface.common;
            retain(add(5,add(algebra::owned_gluing_detail::words(common),add(
                algebra::owned_gluing_detail::words(interface.common_left),algebra::owned_gluing_detail::words(interface.common_right)))),guard);
            algebra::owned_gluing_detail::source(common,guard);
            algebra::owned_gluing_detail::map(common,left.complex(),interface.common_left,true,guard);
            algebra::owned_gluing_detail::map(common,right.complex(),interface.common_right,true,guard);
            if(interface.left_views.size()!=common.grade_count() || interface.right_views.size()!=common.grade_count())
                throw std::invalid_argument("interface views must cover every retained grade");
            for(std::size_t k=0;k<common.grade_count();++k) {
                const auto n=common.grade(grade(k)).cell_count();
                if(interface.left_views[k].size()!=n || interface.right_views[k].size()!=n)
                    throw std::invalid_argument("interface views must cover every retained common cell");
                for(std::size_t i=0;i<n;++i) {
                    const auto& a=interface.left_views[k][i];const auto& b=interface.right_views[k][i];
                    view(a,left.stalks().dimension({grade(k),interface.common_left.cells[k][i]}),guard);
                    view(b,right.stalks().dimension({grade(k),interface.common_right.cells[k][i]}),guard);
                    if(a.frame!=b.frame || a.accession.rows()!=b.accession.rows())
                        throw std::invalid_argument("both interface views require the same declared common carrier");
                    const auto [it,inserted]=frames.emplace(a.frame,a.accession.rows());
                    if(!inserted && it->second!=a.accession.rows())
                        throw std::invalid_argument("interface common frame dimensions disagree");
                }
            }
        }
    }
    void accumulate(Equations& rows,std::size_t member,std::size_t k,std::size_t occurrence,
                    const M& accession,const M& input,int sign,const EntryFamily& family,Guard& guard,
                    std::size_t& temporary)const {
        const auto& fit=members_[member];const auto& model=fit.model();const auto& fibers=model.owner().fibers();
        const auto& map=fibers.transport_override(grade(k),occurrence);
        if(!map)throw std::invalid_argument("explicit realized interface transport required");
        const auto fixed=product(product(accession,*map,guard,temporary),input,guard,temporary);
        std::size_t index_size{};
        const auto row_at=[&](const M::Key& key)->Equation& {
            const auto [found,inserted]=rows.try_emplace(key);
            if(inserted){temporary=add(temporary,3);admit(guard,add(temporary,index_size));}
            return found->second;
        };
        for(const auto& [key,q]:fixed.entries()) {
            auto& row=row_at(key);row.constant=guard.add(row.constant,guard.mul(Q{sign},q));
        }
        if(model.grade()!=grade(k))return;
        const auto coefficient=algebra::owned_gluing_detail::coefficient(fibers.complex(),grade(k),occurrence,guard);
        using Entries=std::map<std::size_t,std::vector<std::pair<std::size_t,Q>>>;
        Entries columns,inputs,directions;
        const auto index=[&](Entries& into,std::size_t key,std::size_t axis,const Q& q) {
            guard.step();const auto [found,inserted]=into.try_emplace(key);
            index_size=add(index_size,inserted?3:2);admit(guard,add(temporary,index_size));
            found->second.emplace_back(axis,q);
        };
        for(const auto& [key,q]:accession.entries())index(columns,key.second,key.first,q);
        for(const auto& [key,q]:input.entries())index(inputs,key.first,key.second,q);
        for(const auto& [key,q]:family.directions.entries())index(directions,key.first,key.second,q);
        for(std::size_t e=0;e<model.variable_coordinates().size();++e) {
            guard.step();const auto& entry=model.variable_coordinates()[e];if(entry.occurrence!=occurrence)continue;
            const auto c=columns.find(entry.row),in=inputs.find(entry.column),d=directions.find(e);
            if(c==columns.end() || in==inputs.end() || (family.origin[e]==0 && d==directions.end()))continue;
            for(const auto& [i,a]:c->second)for(const auto& [j,b]:in->second) {
                const auto q=guard.mul(Q{sign},guard.mul(coefficient,guard.mul(a,b)));
                if(q==0)continue;
                auto& row=row_at({i,j});
                row.constant=guard.add(row.constant,guard.mul(q,family.origin[e]));
                if(d==directions.end())continue;
                for(const auto& [axis,value]:d->second) {
                    const auto column=add(member_free_offsets_[member],axis);
                    const auto old=row.terms.find(column);
                    auto updated=guard.add(old==row.terms.end()?Q{0}:old->second,guard.mul(q,value));
                    if(updated==0){if(old!=row.terms.end()){row.terms.erase(old);temporary-=2;}}
                    else {
                        if(old==row.terms.end()){temporary=add(temporary,2);admit(guard,add(temporary,index_size));}
                        row.terms[column]=std::move(updated);
                    }
                }
            }
        }
    }
    void build_equations(const std::vector<EntryFamily>& entries,Guard& guard) {
        std::vector<Equation> all;std::size_t temporary{};
        for(std::size_t f=0;f<interfaces_.size();++f) {
            const auto& interface=interfaces_[f];
            for(std::size_t k=1;k<interface.common.grade_count();++k) {
                const auto& arena=interface.common.grade(grade(k));
                for(std::size_t p=0;p<arena.incidence_count();++p) {
                    guard.step();const auto lower=arena.lower_index[p],upper=arena.upper_index[p];
                    auto witness=context_view_detail::intersection(interface.left_views[k][upper],interface.right_views[k][upper],guard,
                        add(retained_,temporary));
                    const auto output=interface.left_views[k-1][lower].accession.rows();
                    Equations rows;
                    accumulate(rows,interface.left,k,interface.common_left.occurrences[k][p],interface.left_views[k-1][lower].accession,
                        witness.origin_input,1,entries[interface.left],guard,temporary);
                    accumulate(rows,interface.right,k,interface.common_right.occurrences[k][p],interface.right_views[k-1][lower].accession,
                        witness.target_input,-1,entries[interface.right],guard,temporary);
                    for(auto& [key,row]:rows) {
                        guard.step();if(row.constant==0 && row.terms.empty()){temporary-=3;continue;}
                        guard.dimension(add(all.size(),1));
                        coordinates_.push_back({f,k,p,key.first,key.second});all.push_back(std::move(row));
                    }
                    retain(add(5,add(witness.common_input.nonzeros(),add(witness.origin_input.nonzeros(),witness.target_input.nonzeros()))),guard);
                    witnesses_.push_back({f,k,p,std::move(witness),output});
                }
            }
        }
        equations_=M(all.size(),member_free_offsets_.back());residual_=V(all.size());statuses_.reserve(all.size());
        retain(mul(all.size(),7),guard);
        for(std::size_t i=0;i<all.size();++i) {
            guard.step();residual_[i]=all[i].constant;bool moving=false;
            for(const auto& [j,q]:all[i].terms)if(q!=0){equations_.set(i,j,q);moving=true;retain(1,guard);}
            statuses_.push_back(moving?TransportParameterRelationStatus::Open:
                (residual_[i]==0?TransportParameterRelationStatus::Forced:TransportParameterRelationStatus::Excluded));
        }
        admit(guard,temporary);
    }
public:
    PreparedTransportFamilyGluing(std::vector<PreparedGradedTransportFit> members,
        std::vector<TransportFamilyInterface> interfaces,Guard& guard)
        :members_(std::move(members)),interfaces_(std::move(interfaces)),limits_(guard.limits()) {
        const auto start=guard.steps();guard.step();
        if(members_.empty())throw std::invalid_argument("at least one owned transport family required");
        guard.dimension(members_.size());guard.dimension(interfaces_.size());std::vector<EntryFamily> entries;
        for(const auto& fit:members_) {
            retain(fit.retained_entries(),guard);
            parameter_offsets_.push_back(add(parameter_offsets_.back(),fit.model().parameter_count()));
            member_free_offsets_.push_back(add(member_free_offsets_.back(),fit.kernel().cols()));
            guard.dimension(parameter_offsets_.back());guard.dimension(add(member_free_offsets_.back(),1));
            const auto& basis=fit.model().parameter_basis();
            entries.push_back({fit.entries(fit.particular(),guard),basis?product(*basis,fit.kernel(),guard):fit.kernel()});
            retain(add(2,add(entries.back().origin.size(),entries.back().directions.nonzeros())),guard);
        }
        validate_interfaces(guard);build_equations(entries,guard);
        const auto d=member_free_offsets_.back(),n=parameter_offsets_.back();M augmented(equations_.rows(),add(d,1));
        for(const auto& [key,q]:equations_.entries()){guard.step();augmented.set(key.first,key.second,q);}
        for(std::size_t i=0;i<residual_.size();++i)augmented.set(i,d,guard.sub(0,residual_[i]));
        admit(guard,augmented.nonzeros());
        const numeric::PreparedExactFrames frames(equations_,guard),joint(augmented,guard);
        rank_=frames.rank();consistent_=rank_==joint.rank();free_columns_=frames.free_columns();
        coordinate_kernel_=M(d,0);kernel_=M(n,0);
        if(consistent_) {
            coordinate_origin_=V(d);coordinate_kernel_=frames.kernel();
            for(std::size_t i=0;i<joint.rank();++i){guard.step();coordinate_origin_[joint.pivots()[i]]=joint.reduced().get(i,d);}
            V original(n);M block(n,d);
            for(std::size_t m=0;m<members_.size();++m) {
                const auto& fit=members_[m];
                for(std::size_t i=0;i<fit.particular().size();++i){guard.step();original[parameter_offsets_[m]+i]=fit.particular()[i];}
                for(const auto& [key,q]:fit.kernel().entries()) {
                    guard.step();block.set(parameter_offsets_[m]+key.first,member_free_offsets_[m]+key.second,q);
                }
            }
            admit(guard,add(original.size(),block.nonzeros()));
            particular_=shift(original,block,coordinate_origin_,guard);kernel_=product(block,coordinate_kernel_,guard);
            const auto certificate=shift(residual_,equations_,coordinate_origin_,guard);
            if(!numeric::is_zero(certificate) || product(equations_,coordinate_kernel_,guard).nonzeros())
                throw std::logic_error("complete glued transport family residual certificate failed");
        } else {
            // A complete original-equation contradiction witness, independent
            // of a selected parameter origin or a numerical tolerance.
            const numeric::PreparedExactFrames dual(equations_.transpose(),guard);const auto& left=dual.kernel();
            for(std::size_t j=0;j<left.cols() && contradiction_.empty();++j) {
                V w(left.rows());Q pairing;
                for(const auto& [key,q]:left.entries())if(key.second==j){guard.step();w[key.first]=q;pairing=guard.add(pairing,guard.mul(q,residual_[key.first]));}
                if(pairing!=0)contradiction_=std::move(w);
            }
            if(contradiction_.empty() || !numeric::is_zero(numeric::checked_transpose_apply(equations_,contradiction_,guard)))
                throw std::logic_error("glued transport contradiction certificate failed");
        }
        retain(add(coordinate_origin_.size(),add(particular_.size(),add(contradiction_.size(),add(coordinate_kernel_.nonzeros(),kernel_.nonzeros())))),guard);
        admit(guard,add(augmented.nonzeros(),add(frames.stored_entries(),joint.stored_entries())));steps_=guard.steps()-start;
    }
    bool consistent()const noexcept{return consistent_;}
    std::size_t rank()const noexcept{return rank_;}
    std::size_t retained_entries()const noexcept{return retained_;}
    std::size_t preparation_steps()const noexcept{return steps_;}
    const std::vector<PreparedGradedTransportFit>& members()const &{return members_;}
    const std::vector<TransportFamilyInterface>& interfaces()const &{return interfaces_;}
    const std::vector<TransportFamilyWitness>& witnesses()const &{return witnesses_;}
    const std::vector<TransportFamilyEquation>& equation_coordinates()const &{return coordinates_;}
    const std::vector<TransportParameterRelationStatus>& statuses()const &{return statuses_;}
    const std::vector<std::size_t>& parameter_offsets()const &{return parameter_offsets_;}
    const std::vector<std::size_t>& member_free_offsets()const &{return member_free_offsets_;}
    const std::vector<std::size_t>& free_columns()const &{return free_columns_;}
    const V& residual_origin()const &{return residual_;}
    const V& coordinate_origin()const &{return coordinate_origin_;}
    const M& coordinate_kernel()const &{return coordinate_kernel_;}
    const V& particular()const &{return particular_;}
    const M& kernel()const &{return kernel_;}
    const M& equations()const &{return equations_;}
    const V& contradiction()const &{return contradiction_;}
    const std::vector<PreparedGradedTransportFit>& members()const &&=delete;
    const std::vector<TransportFamilyInterface>& interfaces()const &&=delete;
    const std::vector<TransportFamilyWitness>& witnesses()const &&=delete;
    const std::vector<TransportFamilyEquation>& equation_coordinates()const &&=delete;
    const std::vector<TransportParameterRelationStatus>& statuses()const &&=delete;
    const std::vector<std::size_t>& parameter_offsets()const &&=delete;
    const std::vector<std::size_t>& member_free_offsets()const &&=delete;
    const std::vector<std::size_t>& free_columns()const &&=delete;
    const V& residual_origin()const &&=delete;
    const V& coordinate_origin()const &&=delete;
    const M& coordinate_kernel()const &&=delete;
    const V& particular()const &&=delete;
    const M& kernel()const &&=delete;
    const M& equations()const &&=delete;
    const V& contradiction()const &&=delete;
    V residuals(const V& member_free,Guard& guard)const {
        operation(guard);if(member_free.size()!=member_free_offsets_.back())throw std::invalid_argument("complete member free coordinates required");
        return shift(residual_,equations_,member_free,guard);
    }
    V parameters(const V& free,Guard& guard)const {
        operation(guard);if(!consistent_)throw std::domain_error("inconsistent interface has no parameter family");
        if(free.size()!=kernel_.cols())throw std::invalid_argument("complete glued free coordinates required");
        return shift(particular_,kernel_,free,guard);
    }
    TransportFamilyClaim classify(const std::vector<TransportFamilyParameterTerm>& terms,Q expected,Guard& guard)const {
        operation(guard);if(!consistent_)throw std::domain_error("inconsistent interface has no claim family");
        TransportFamilyClaim out{TransportParameterRelationStatus::Forced,guard.sub(0,numeric::canonical_rational(expected)),V(kernel_.cols())};
        for(const auto& term:terms) {
            if(term.member>=members_.size() || term.parameter>=members_[term.member].model().parameter_count())
                throw std::invalid_argument("claim must name original member parameter coordinates");
            const auto index=add(parameter_offsets_[term.member],term.parameter);const auto q=numeric::canonical_rational(term.coefficient);
            out.residual=guard.add(out.residual,guard.mul(q,particular_[index]));
            for(const auto& [key,value]:kernel_.entries()){guard.step();if(key.first==index)out.directions[key.second]=guard.add(out.directions[key.second],guard.mul(q,value));}
        }
        if(!numeric::is_zero(out.directions))out.status=TransportParameterRelationStatus::Open;
        else if(out.residual!=0)out.status=TransportParameterRelationStatus::Excluded;
        return out;
    }
    std::vector<local::OwnedLinearFiberSystem> materialize(const V& free,Guard& guard)const {
        operation(guard);if(!consistent_)throw std::domain_error("inconsistent interface cannot materialize");
        if(free.size()!=coordinate_kernel_.cols())throw std::invalid_argument("complete glued free coordinates required");
        const auto coordinates=shift(coordinate_origin_,coordinate_kernel_,free,guard);
        if(!numeric::is_zero(shift(residual_,equations_,coordinates,guard)))throw std::logic_error("glued member residual is nonzero");
        std::vector<local::OwnedLinearFiberSystem> result;result.reserve(members_.size());
        for(std::size_t m=0;m<members_.size();++m) {
            V local(coordinates.begin()+static_cast<std::ptrdiff_t>(member_free_offsets_[m]),
                    coordinates.begin()+static_cast<std::ptrdiff_t>(member_free_offsets_[m+1]));
            result.push_back(members_[m].materialize(local,guard));
        }
        for(const auto& witness:witnesses_) {
            const auto& interface=interfaces_[witness.interface];const auto k=witness.grade,p=witness.occurrence;
            const auto lower=interface.common.grade(grade(k)).lower_index[p];
            const auto& left=*result[interface.left].fibers().transport_override(grade(k),interface.common_left.occurrences[k][p]);
            const auto& right=*result[interface.right].fibers().transport_override(grade(k),interface.common_right.occurrences[k][p]);
            const auto a=product(product(interface.left_views[k-1][lower].accession,left,guard),witness.input.origin_input,guard);
            const auto b=product(product(interface.right_views[k-1][lower].accession,right,guard),witness.input.target_input,guard);
            if(a!=b)throw std::logic_error("materialized original interface responses disagree");
        }
        return result;
    }
};
} // namespace rfc::learning
