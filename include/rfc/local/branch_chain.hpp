#pragma once
#include "rfc/core/builder.hpp"
#include "rfc/numeric/sparse_algebraic_matrix.hpp"
#include <concepts>
#include <set>

namespace rfc::local {

struct BranchChainWorkReceipt {
    std::size_t vertices{}, relations{}, occurrences{}, nullity{};
    std::size_t retained_entries_bound{}, preparation_steps{};
};

// The positive-share recipe of companion Proposition21.3. These are fields in
// ker(B1^T), hence grade-zero harmonic fields in identity metrics. No metric
// adjoint, selected spatial embedding or extra primary cells are implicit.
class PreparedBranchChain {
    using Q = numeric::Rational;
    using V = numeric::RationalVector;
    using M = numeric::ExactMatrix;
    using Guard = numeric::ExactActionGuard;
    struct State {
        core::RelationalComplex source;
        V shares, polynomial;
        std::vector<core::IdentityKey> vertices;
        std::vector<core::RelationKey> relations;
        M boundary, basis;
        BranchChainWorkReceipt receipt;
    };
    std::shared_ptr<const State> state_;
    explicit PreparedBranchChain(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    static std::size_t add(std::size_t a, std::size_t b) { return machine::checked_size_add(a,b); }
    static std::size_t mul(std::size_t a, std::size_t b) { return machine::checked_size_mul(a,b); }
    void operation(Guard& guard, std::size_t scratch) const {
        guard.step();
        guard.dimension(state_->vertices.size());
        guard.dimension(state_->relations.size());
        guard.entries(add(state_->receipt.retained_entries_bound,scratch));
        guard.matrix(state_->boundary);
        guard.matrix(state_->basis);
    }
public:
    class Field {
        friend class PreparedBranchChain;
        std::shared_ptr<const State> owner_;
        V coordinates_;
        Field(std::shared_ptr<const State> owner, V coordinates)
            : owner_(std::move(owner)), coordinates_(std::move(coordinates)) {}
    public:
        const V& coordinates() const & noexcept { return coordinates_; }
        const V& coordinates() const && = delete;
        bool same_owner(const Field& other) const noexcept { return owner_==other.owner_; }
    };

    class ModeFamily {
        friend class PreparedBranchChain;
        std::shared_ptr<const State> owner_;
        numeric::CertifiedAlgebraicField field_;
        std::vector<numeric::AlgebraicElement> roots_;
        numeric::SparseAlgebraicMatrix values_;
        bool complete_;
        std::size_t steps_;
        ModeFamily(std::shared_ptr<const State> owner, numeric::CertifiedAlgebraicField field,
            std::vector<numeric::AlgebraicElement> roots, numeric::SparseAlgebraicMatrix values,
            bool complete, std::size_t steps)
            : owner_(std::move(owner)), field_(std::move(field)), roots_(std::move(roots)),
              values_(std::move(values)), complete_(complete), steps_(steps) {}
    public:
        const numeric::CertifiedAlgebraicField& field() const & noexcept { return field_; }
        const numeric::CertifiedAlgebraicField& field() const && = delete;
        const std::vector<numeric::AlgebraicElement>& roots() const & noexcept { return roots_; }
        const std::vector<numeric::AlgebraicElement>& roots() const && = delete;
        const numeric::SparseAlgebraicMatrix& values() const & noexcept { return values_; }
        const numeric::SparseAlgebraicMatrix& values() const && = delete;
        bool complete() const noexcept { return complete_; }
        std::size_t preparation_steps() const noexcept { return steps_; }
        void require_complete() const {
            if(!complete_) throw std::domain_error("declared geometric modes do not span the full recurrence family");
        }
        void require_source(const PreparedBranchChain& actual) const {
            if(owner_!=actual.state_) throw std::invalid_argument("branch modes belong to another actual source");
        }
    };

    static PreparedBranchChain prepare(const V& shares,
        const std::vector<core::IdentityKey>& vertices, const std::vector<core::RelationKey>& relations,
        Guard& guard, core::SemanticScale scale=core::SemanticScale{0}) {
        const auto start=guard.steps();
        guard.step();
        const auto m=shares.size(), n=relations.size(), count=add(n,m);
        if(!m || !n || vertices.size()!=count)
            throw std::invalid_argument("branch chain needs positive arity/order and every declared vertex/relation key");
        if(count>std::numeric_limits<std::uint32_t>::max()) throw std::length_error("branch source index limit");
        guard.dimension(count);guard.dimension(n);guard.dimension(m);
        const auto occurrences=mul(n,add(m,1));
        // Complete primary source, ordered IDs, construction copies, boundary,
        // full recurrence basis and polynomial. Counts are container entries,
        // not a GMP/RSS claim. Recurrence arithmetic uses the exact guard.
        auto retained=add(128,add(mul(count,128),mul(occurrences,256)));
        retained=add(retained,add(mul(count,m),add(occurrences,add(mul(m,2),1))));
        guard.entries(add(retained,add(mul(n,m),mul(count,4))));
        auto admitted=guard.canonical_input(shares);
        Q sum{0};
        for(const auto& s:admitted) {
            if(s<=0) throw std::invalid_argument("branch tail shares must be strictly positive");
            sum=guard.add(sum,s);
        }
        if(sum!=1) throw std::invalid_argument("branch tail shares must sum exactly to one");
        // Admit the coordinated integral column before the builder forms it.
        // Its coefficients have magnitude at most this common denominator.
        numeric::Integer denominator{1};
        for(const auto& s:admitted) {
            guard.step();numeric::Integer next;
            mpz_lcm(next.get_mpz_t(),denominator.get_mpz_t(),mpq_denref(s.get_mpq_t()));
            guard.value(Q{next});denominator=std::move(next);
        }
        std::set<core::IdentityKey> vertex_keys;
        for(const auto key:vertices) {guard.step();if(!vertex_keys.insert(key).second)throw std::invalid_argument("branch vertex identities must be distinct");}
        std::set<core::RelationKey> relation_keys;
        for(const auto key:relations) {guard.step();if(!key.value || !relation_keys.insert(key).second)throw std::invalid_argument("branch relation keys must be distinct and nonzero");}
        core::RelationSignature signature;
        const auto type=signature.add_branch_type("finite positive-share branch",add(m,1),add(m,1));
        const auto role=*signature.type(type).distinguished_role_aspect();
        core::RelationalComplexBuilder builder(signature,scale);
        for(const auto key:vertices) {guard.step();builder.declare_witness_identity(key);}
        for(std::size_t i=0;i<n;++i) {
            std::vector<core::BoundaryOccurrenceInput> inputs;
            inputs.reserve(add(m,1));
            for(std::size_t j=0;j<=m;++j) {
                guard.step();
                inputs.emplace_back(vertices[i+j],true,j==0,j==0?Q{1}:admitted[j-1],std::vector<core::AspectValue>{{role,j==0}});
            }
            builder.add_grade_one(relations[i],type,inputs);
        }
        auto source=builder.freeze();
        M boundary(count,n),basis(count,m);
        const auto& arena=source.grade(core::Grade{1});
        for(const auto& cell_scale:arena.cell_scales){guard.step();guard.value(cell_scale);}
        for(const auto& coefficient:arena.lattice_coefficient){guard.step();guard.value(Q{coefficient});}
        for(std::size_t slot=0;slot<arena.incidence_count();++slot) {
            guard.step();const auto q=source.incidence_coefficient(core::Grade{1},slot);guard.value(q);
            boundary.set(arena.lower_index[slot],arena.upper_index[slot],q);
        }
        for(std::size_t j=0;j<m;++j) {guard.step();basis.set(j,j,1);}
        // Initial m values determine the entire sequence because s_m>0.
        // Identity initial rows prove independence, including repeated roots.
        for(std::size_t i=0;i<n;++i) for(std::size_t c=0;c<m;++c) {
            auto value=basis.get(i,c);
            for(std::size_t j=1;j<m;++j)value=guard.sub(value,guard.mul(admitted[j-1],basis.get(i+j,c)));
            basis.set(i+m,c,guard.div(value,admitted.back()));
        }
        // The retained head rows form a triangular N-column minor with -1
        // diagonal. Together with B^T basis=0 this certifies full kernel span.
        for(std::size_t i=0;i<n;++i) {
            guard.step();if(boundary.get(i,i)!=-1)throw std::logic_error("branch leading head coefficient changed");
        }
        for(const auto& [ij,q]:boundary.entries()) {
            (void)q;guard.step();if(ij.first<ij.second)throw std::logic_error("branch head echelon certificate failed");
        }
        if(numeric::checked_multiply(boundary.transpose(),basis,guard).nonzeros())
            throw std::logic_error("branch recurrence basis has a source boundary residual");
        V polynomial{-1};polynomial.insert(polynomial.end(),admitted.begin(),admitted.end());
        guard.step(); // No complete object before the final checkpoint.
        BranchChainWorkReceipt receipt{count,n,occurrences,m,retained,guard.steps()-start};
        return PreparedBranchChain(std::make_shared<const State>(State{std::move(source),std::move(admitted),
            std::move(polynomial),vertices,relations,std::move(boundary),std::move(basis),receipt}));
    }
    template<std::floating_point T> static PreparedBranchChain prepare(std::initializer_list<T>,
        const std::vector<core::IdentityKey>&,const std::vector<core::RelationKey>&,Guard&,
        core::SemanticScale=core::SemanticScale{0})=delete;

    const core::RelationalComplex& source() const & noexcept { return state_->source; }
    const core::RelationalComplex& source() const && = delete;
    const V& shares() const & noexcept { return state_->shares; }
    const V& shares() const && = delete;
    const V& characteristic_polynomial() const & noexcept { return state_->polynomial; }
    const V& characteristic_polynomial() const && = delete;
    const M& boundary() const & noexcept { return state_->boundary; }
    const M& boundary() const && = delete;
    const M& basis() const & noexcept { return state_->basis; }
    const M& basis() const && = delete;
    const BranchChainWorkReceipt& work_receipt() const & noexcept { return state_->receipt; }
    const BranchChainWorkReceipt& work_receipt() const && = delete;
    std::size_t source_rank() const noexcept { return state_->relations.size(); }
    std::size_t nullity() const noexcept { return state_->shares.size(); }
    bool same_owner(const PreparedBranchChain& other) const noexcept { return state_==other.state_; }

    Field field(const V& coordinates,Guard& guard) const {
        operation(guard,mul(state_->vertices.size(),2));
        if(coordinates.size()!=state_->vertices.size())throw std::invalid_argument("branch field carrier differs");
        return Field(state_,guard.canonical_input(coordinates));
    }
    template<std::floating_point T> Field field(std::initializer_list<T>,Guard&) const=delete;
    V annihilator_residual(const Field& input,Guard& guard) const {
        if(input.owner_!=state_)throw std::invalid_argument("branch field belongs to another actual source");
        operation(guard,mul(state_->vertices.size(),4));
        return numeric::checked_transpose_apply(state_->boundary,input.coordinates_,guard);
    }
    Field from_initial_values(const V& initial,Guard& guard) const {
        operation(guard,mul(state_->vertices.size(),4));
        if(initial.size()!=nullity())throw std::invalid_argument("branch initial values omit recurrence coordinates");
        auto out=Field(state_,numeric::checked_apply(state_->basis,initial,guard));
        if(!numeric::is_zero(annihilator_residual(out,guard)))throw std::logic_error("extended branch field has a residual");
        return out;
    }
    template<std::floating_point T> Field from_initial_values(std::initializer_list<T>,Guard&) const=delete;
    V initial_values(const Field& input,Guard& guard) const {
        if(!numeric::is_zero(annihilator_residual(input,guard)))throw std::domain_error("branch field is outside the recurrence kernel");
        return {input.coordinates_.begin(),input.coordinates_.begin()+static_cast<std::ptrdiff_t>(nullity())};
    }

    ModeFamily declared_modes(const numeric::CertifiedAlgebraicField& field,
        const std::vector<numeric::AlgebraicElement>& roots,Guard& guard) const {
        const auto start=guard.steps(),m=nullity(),count=state_->vertices.size(),r=roots.size(),d=field.degree();
        guard.step();
        if(!r || r>m)throw std::invalid_argument("declare a nonempty independent mode subset of size at most the nullity");
        guard.dimension(d);
        if(field.limits().max_integer_bits>guard.limits().integer_bits)
            throw std::length_error("declared branch field scalar bit limit exceeds receiver");
        // Full output fields, initial-coordinate rank check, actual B^T
        // coefficients, temporary elimination maps and scalar polynomial work.
        const auto mode_entries=mul(add(mul(count,r),add(mul(m,r),state_->boundary.nonzeros())),d);
        const auto reserve=add(state_->receipt.retained_entries_bound,add(mul(mode_entries,8),mul(add(d,1),16)));
        operation(guard, reserve-state_->receipt.retained_entries_bound);
        auto limits=guard.limits();limits.entries-=reserve;limits.steps-=guard.steps();
        Guard work(limits,[&guard]{guard.step();});
        for(const auto& q:field.minimal_polynomial()){work.step();work.value(q);}
        const auto admit=[&](const numeric::AlgebraicElement& x) {
            for(const auto& q:x.coordinates()){work.step();work.value(q);}
        };
        for(std::size_t j=0;j<r;++j) {
            work.step();if(!field.owns(roots[j]))throw std::invalid_argument("branch root has a foreign coefficient owner");
            admit(roots[j]);
            for(std::size_t i=0;i<j;++i) {work.step();if(roots[i]==roots[j])throw std::invalid_argument("duplicate geometric root does not add a mode");}
            const auto value=field.evaluate(state_->polynomial,roots[j]);admit(value);
            if(!value.is_zero())throw std::invalid_argument("declared branch ratio is not a recurrence root");
        }
        numeric::SparseAlgebraicMatrix values(count,r,field),initial(m,r,field);
        for(std::size_t j=0;j<r;++j) {
            auto power=field.scalar(1);
            for(std::size_t i=0;i<count;++i) {
                work.step();admit(power);values.set(i,j,power);if(i<m)initial.set(i,j,power);
                if(i+1<count)power=power*roots[j];
            }
        }
        const auto transposed=numeric::SparseAlgebraicMatrix::from_rational(state_->boundary.transpose(),field,work);
        if(transposed.multiply(values,work).nonzeros())throw std::logic_error("declared mode has an original source residual");
        if(initial.rank(work)!=r)throw std::logic_error("declared geometric modes are dependent");
        // r distinct valid roots with r=m exhaust a degree-m polynomial.
        // Repeated-root polynomials need generalized modes, outside this profile.
        work.step();
        return ModeFamily(state_,field,roots,std::move(values),r==m,guard.steps()-start);
    }
};
} // namespace rfc::local
