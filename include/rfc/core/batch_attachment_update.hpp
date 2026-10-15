#pragma once
#include "rfc/core/attachment_update.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <memory>

namespace rfc::core {
// All lower indices name existing cells in the selected lower grade. The
// declaration never creates a new lower identity or an implicit occurrence.
struct BatchAttachmentCell {
    RelationKey key;
    RelationTypeId type;
    std::vector<HigherIncidenceInput> incidences;
};
class PreparedBatchAttachment {
    using Guard=numeric::ExactActionGuard;
    using M=numeric::ExactMatrix;
    using Q=numeric::Rational;
    struct State {
        std::shared_ptr<const RelationalComplex> before,after;
        Grade upper;
        std::vector<BatchAttachmentCell> declarations;
        std::vector<CellId> appended;
        M columns;
        BianchiReport bianchi;
        std::size_t retained,steps;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedBatchAttachment(std::shared_ptr<const State> state):state_(std::move(state)){}
    static std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
    static std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
    static Q coefficient(const RelationalComplex& source,Grade g,std::size_t p,Guard& guard) {
        const auto& arena=source.grade(g);const auto& lower=source.grade(Grade{static_cast<std::uint16_t>(g.value-1)});
        return guard.div(guard.mul(lower.cell_scales[arena.lower_index[p]],Q{arena.lattice_coefficient[p]}),arena.cell_scales[arena.upper_index[p]]);
    }
    static std::vector<AspectValue> aspects(const RelationalComplex& source,const GradeArena& arena,std::size_t p,Guard& guard) {
        std::vector<AspectValue> out;const auto& schema=source.signature().aspects();
        for(std::size_t a=0;a<schema.size();++a) {
            guard.step();const auto id=static_cast<composite::AspectId>(a);const auto role=schema.at(id).role;
            if(role!=composite::AspectRole::Existence && role!=composite::AspectRole::Orientation)out.push_back({id,arena.aspects.get(id,p)});
        }
        return out;
    }
    static std::size_t source_words(const RelationalComplex& source,Guard& guard) {
        std::size_t words=32;
        for(std::size_t k=0;k<source.grade_count();++k) {
            guard.step();const auto& a=source.grade(Grade{static_cast<std::uint16_t>(k)});guard.dimension(a.cell_count());
            words=add(words,add(mul(a.cell_count(),16),mul(a.incidence_count(),add(16,source.signature().aspects().size()))));
            guard.entries(words);for(const auto& q:a.cell_scales){guard.step();guard.value(q);}
            for(const auto& z:a.lattice_coefficient){guard.step();guard.value(Q{z});}
        }
        return words;
    }
    static void same_prefix(const RelationalComplex& old,const RelationalComplex& next,Guard& guard) {
        if(old.grade(Grade{0}).identities!=next.grade(Grade{0}).identities)throw std::logic_error("batch changed lower carrier identities");
        for(std::size_t k=0;k<old.grade_count();++k) {
            const Grade g{static_cast<std::uint16_t>(k)};const auto& a=old.grade(g);const auto& b=next.grade(g);
            if(a.cell_count()>b.cell_count() || a.incidence_count()>b.incidence_count())throw std::logic_error("batch removed retained source data");
            for(std::size_t i=0;i<a.cell_count();++i) {
                guard.step();if(a.cell_scales[i]!=b.cell_scales[i] || a.relation_types[i]!=b.relation_types[i] ||
                   (k && (a.relation_identities[i]!=b.relation_identities[i] || a.incidence_range(i)!=b.incidence_range(i))))
                    throw std::logic_error("batch changed an existing relation or coordinated lattice scale");
            }
            for(std::size_t p=0;p<a.incidence_count();++p) {
                guard.step();if(a.lower_index[p]!=b.lower_index[p] || a.upper_index[p]!=b.upper_index[p] ||
                   a.occurrence_identities[p]!=b.occurrence_identities[p] || a.lattice_coefficient[p]!=b.lattice_coefficient[p])
                    throw std::logic_error("batch changed an existing retained occurrence");
                for(std::size_t j=0;j<old.signature().aspects().size();++j) {
                    guard.step();const auto aspect=static_cast<composite::AspectId>(j);
                    if(a.aspects.get(aspect,p)!=b.aspects.get(aspect,p))throw std::logic_error("batch changed an existing occurrence aspect");
                }
            }
        }
    }
public:
    static PreparedBatchAttachment prepare(std::shared_ptr<const RelationalComplex> source,Grade upper,
            std::vector<BatchAttachmentCell> cells,Guard& guard) {
        const auto start=guard.steps();guard.step();
        if(!source || !upper.value || upper.value>source->max_grade().value+1u || cells.empty())
            throw std::invalid_argument("batch needs an owned source, an adjacent positive grade and actual appended cells");
        const auto old_count=upper.value<source->grade_count()?source->grade(upper).cell_count():0;
        const auto lower=Grade{static_cast<std::uint16_t>(upper.value-1)};
        const auto n=source->grade(lower).cell_count();guard.dimension(n);guard.dimension(add(old_count,cells.size()));
        std::size_t retained=source_words(*source,guard);
        for(auto& cell:cells) {
            guard.step();(void)source->signature().type(cell.type);retained=add(retained,4);
            for(auto& incidence:cell.incidences) {
                guard.step();if(incidence.lower_index>=n)throw std::invalid_argument("batch occurrence does not name an existing lower cell");
                guard.value(incidence.magnitude);incidence.magnitude=numeric::canonical_rational(std::move(incidence.magnitude));
                if(incidence.magnitude<0)throw std::invalid_argument("batch occurrence orientation owns its sign");
                retained=add(retained,add(8,mul(incidence.aspects.size(),2)));
            }
        }
        guard.entries(mul(retained,4));
        RelationalComplexBuilder builder(source->signature(),source->semantic_scale());
        for(std::size_t k=0;k<source->grade_count();++k)builder.declare_grade(Grade{static_cast<std::uint16_t>(k)});
        for(const auto identity:source->grade(Grade{0}).identities){guard.step();builder.declare_witness_identity(identity);}
        const auto exists=source->signature().existence_aspect(),orientation=source->signature().orientation_aspect();
        for(std::size_t k=1;k<source->grade_count();++k) {
            const Grade g{static_cast<std::uint16_t>(k)};const auto& arena=source->grade(g);
            for(std::size_t cell=0;cell<arena.cell_count();++cell) {
                guard.step();const auto [begin,end]=arena.incidence_range(cell);
                std::vector<BoundaryOccurrenceInput> first;std::vector<HigherIncidenceInput> higher;
                for(std::size_t p=begin;p<end;++p) {
                    const auto q=coefficient(*source,g,p,guard);const auto magnitude=q<0?guard.sub(0,q):q;
                    if(k==1)first.emplace_back(source->grade(Grade{0}).identities[arena.lower_index[p]],arena.aspects.get(exists,p),
                        arena.aspects.get(orientation,p),magnitude,aspects(*source,arena,p,guard),arena.occurrence_identities[p]);
                    else higher.emplace_back(arena.lower_index[p],arena.aspects.get(exists,p),arena.aspects.get(orientation,p),
                        magnitude,aspects(*source,arena,p,guard),arena.occurrence_identities[p]);
                }
                if(k==1)builder.add_grade_one(arena.relation_identities[cell],arena.relation_types[cell],first);
                else builder.add_higher(g,arena.relation_identities[cell],arena.relation_types[cell],higher);
            }
        }
        std::vector<CellId> appended;
        for(const auto& cell:cells) {
            guard.step();
            if(upper.value==1) {
                std::vector<BoundaryOccurrenceInput> input;
                for(const auto& p:cell.incidences) {
                    guard.step();input.emplace_back(source->grade(Grade{0}).identities[p.lower_index],p.exists,p.negative,p.magnitude,p.aspects,p.occurrence_key);
                }
                appended.push_back(builder.add_grade_one(cell.key,cell.type,input));
            } else appended.push_back(builder.add_higher(upper,cell.key,cell.type,cell.incidences));
        }
        auto after=std::make_shared<const RelationalComplex>(builder.freeze());guard.step();same_prefix(*source,*after,guard);
        retained=add(retained,source_words(*after,guard));M columns(n,cells.size());const auto& arena=after->grade(upper);
        for(std::size_t j=0;j<appended.size();++j) {
            const auto [begin,end]=arena.incidence_range(appended[j].index);
            for(std::size_t p=begin;p<end;++p) {
                guard.step();if(!arena.aspects.get(exists,p))continue;
                columns.set(arena.lower_index[p],j,guard.add(columns.get(arena.lower_index[p],j),coefficient(*after,upper,p,guard)));
            }
        }
        retained=add(retained,add(columns.nonzeros(),mul(appended.size(),2)));guard.entries(mul(retained,2));
        auto bianchi=canonical_bianchi_certificate(*after);
        return PreparedBatchAttachment(std::make_shared<const State>(State{std::move(source),std::move(after),upper,
            std::move(cells),std::move(appended),std::move(columns),std::move(bianchi),retained,guard.steps()-start}));
    }
    std::shared_ptr<const RelationalComplex> before() const {return state_->before;}
    std::shared_ptr<const RelationalComplex> after() const {return state_->after;}
    Grade upper_grade() const noexcept {return state_->upper;}
    const std::vector<BatchAttachmentCell>& declarations() const & {return state_->declarations;}
    const std::vector<CellId>& appended_cells() const & {return state_->appended;}
    const M& columns() const & {return state_->columns;}
    const BianchiReport& bianchi() const & {return state_->bianchi;}
    const std::vector<BatchAttachmentCell>& declarations() const &&=delete;
    const std::vector<CellId>& appended_cells() const &&=delete;
    const M& columns() const &&=delete;
    const BianchiReport& bianchi() const &&=delete;
    std::size_t retained_entries() const noexcept {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return state_->steps;}
    bool same_owner(const PreparedBatchAttachment& other) const noexcept {return state_==other.state_;}
};
} // namespace rfc::core
