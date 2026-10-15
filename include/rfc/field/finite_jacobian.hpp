#pragma once
#include "rfc/field/period_lattice.hpp"
#include "rfc/geometry/real_algebraic_embedding.hpp"
#include "rfc/numeric/sparse_algebraic_matrix.hpp"

namespace rfc::field {
struct ExactComplexPeriod {
    numeric::AlgebraicElement real,imaginary;
};
struct DeclaredPeriodPlane {
    numeric::Rational squared_frequency;
    numeric::AlgebraicElement positive_root;
    // Explicit source cochain coordinates. The consumer certifies and
    // saturates this supplied rational plane; it never discovers eigenvectors.
    numeric::ExactMatrix rational_basis;
};
struct FiniteEllipticFactor {
    numeric::Rational squared_frequency;
    numeric::ExactMatrix declared_rational_basis;
    numeric::IntegerMatrix saturated_lattice;
    numeric::Integer intersection;
    numeric::ExactMatrix gram;
    ExactComplexPeriod modulus;
};
namespace jacobian_detail {
using A=numeric::SparseAlgebraicMatrix;
using E=numeric::AlgebraicElement;
using F=numeric::CertifiedAlgebraicField;
using Guard=numeric::ExactActionGuard;
using Q=numeric::Rational;
inline void value(const E& scalar,Guard& guard){for(const auto& q:scalar.coordinates()){guard.step();guard.value(q);}}
inline std::vector<E> positive(const A& form,const geometry::SelectedRealAlgebraicEmbedding& embedding,Guard& guard){
    if(!form.field().same_owner(embedding.field())||form.rows()!=form.cols())throw std::invalid_argument("period positivity coefficient/carrier mismatch");
    form.validate(guard);if(!(form==form.transpose(guard)))throw std::invalid_argument("normalized period form is not symmetric");
    auto residual=form;std::vector<E> pivots;const auto zero=form.field().scalar(Q{0});
    for(std::size_t k=0;k<form.rows();++k){
        guard.step();const auto pivot=residual.get(k,k);if(embedding.sign(pivot)<=0)throw std::invalid_argument("normalized period form is not positive definite at the selected root");
        pivots.push_back(pivot);std::vector<std::pair<std::size_t,E>> neighbors;
        for(const auto& [ij,coefficient]:residual.entries())if(ij.first==k&&ij.second>k)neighbors.emplace_back(ij.second,coefficient);
        for(std::size_t i=0;i<neighbors.size();++i)for(std::size_t j=i;j<neighbors.size();++j){
            guard.step();const auto correction=neighbors[i].second*neighbors[j].second/pivot;value(correction,guard);
            const auto next=residual.get(neighbors[i].first,neighbors[j].first)-correction;value(next,guard);
            residual.set(neighbors[i].first,neighbors[j].first,next);residual.set(neighbors[j].first,neighbors[i].first,next);
        }
        for(const auto& [index,coefficient]:neighbors){(void)coefficient;residual.set(k,index,zero);residual.set(index,k,zero);}
        residual.set(k,k,zero);residual.validate(guard);
    }
    return pivots;
}
inline numeric::IntegerMatrix integer_form(const numeric::ExactMatrix& matrix,Guard& guard){
    guard.matrix(matrix);numeric::IntegerMatrix out(matrix.rows(),matrix.cols());
    for(const auto& [ij,q]:matrix.entries()){
        if(numeric::denominator(q)!=1)throw std::invalid_argument("period form is not integral");
        out.set(ij.first,ij.second,numeric::numerator(q));
    }
    return out;
}
inline numeric::ExactMatrix canonical_cup(std::size_t h){
    numeric::ExactMatrix out(h,h);const auto g=h/2;
    for(std::size_t i=0;i<g;++i){out.set(i,g+i,Q{1});out.set(g+i,i,Q{-1});}return out;
}
} // namespace jacobian_detail

class FiniteJacobianPoint;
class FiniteIsogenyCertificate;

// The paper's exact finite normalized period torus.  A supplied normalization
// is checked algebraically and at a separately selected real embedding. No
// eigenvalues, approximate roots, smooth conformal limit, curve equation, or
// identification with a smooth surface Jacobian is inferred by this consumer.
class PreparedFiniteJacobian {
    using A=jacobian_detail::A;
    using E=jacobian_detail::E;
    struct State {
        PreparedIntegralPeriodLattice lattice;
        geometry::SelectedRealAlgebraicEmbedding embedding;
        numeric::IntegerMatrix symplectic_basis;
        numeric::ExactMatrix star;
        A normalization,complex_structure,normalized_form,real_periods,imaginary_periods;
        std::vector<E> positive_pivots;
    };
public:
    static PreparedFiniteJacobian normalize(const PreparedIntegralPeriodLattice& lattice,
        numeric::IntegerMatrix symplectic_basis,A normalization,const geometry::SelectedRealAlgebraicEmbedding& embedding,
        numeric::ExactActionGuard& guard){
        using namespace jacobian_detail;
        const auto h=lattice.period_rank(),g=lattice.genus();const auto field=embedding.field();
        if(!normalization.field().same_owner(field)||normalization.rows()!=h||normalization.cols()!=h||
            symplectic_basis.rows()!=h||symplectic_basis.cols()!=h)throw std::invalid_argument("finite complex-structure declaration carrier differs");
        const auto basis_smith=numeric::smith_normal_form_actions(symplectic_basis,guard);
        if(basis_smith.rank!=h)throw std::invalid_argument("symplectic coordinate declaration is singular");
        for(const auto& d:basis_smith.invariant_factors)if(d!=1)throw std::invalid_argument("symplectic coordinates change the integral period lattice");
        const auto tq=period_detail::rational(symplectic_basis,guard);
        const auto transported_cup=period_detail::product(tq.transpose(),period_detail::product(lattice.cup_form(),tq,guard),guard);
        if(!(transported_cup==canonical_cup(h)))throw std::invalid_argument("declared integral basis is not symplectic for the source cup pairing");
        const auto omega_smith=numeric::smith_normal_form_actions(integer_form(lattice.cup_form(),guard),guard);
        const auto star=omega_smith.right_inverse.apply_inverse_rational(
            omega_smith.left.apply_rational(lattice.gram(),guard),guard);
        const auto s=A::from_rational(star,field,guard),omega=A::from_rational(lattice.cup_form(),field,guard);
        const auto identity=A::identity(h,field,guard);
        const auto minus_one=field.scalar(Q{-1});
        if(!(normalization.multiply(s,guard)==s.multiply(normalization,guard))||
            !(normalization.multiply(normalization,guard).multiply(s.multiply(s,guard),guard)==identity.scale(minus_one,guard)))
            throw std::invalid_argument("declared normalization does not commute with the source star or normalize its square");
        const auto complex_structure=s.multiply(normalization,guard);
        if(!(complex_structure.multiply(complex_structure,guard)==identity.scale(minus_one,guard)))
            throw std::logic_error("normalized complex-structure square certificate failed");
        const auto original_form=omega.multiply(complex_structure,guard);
        (void)positive(original_form,embedding,guard);
        const auto t=A::from_integer(symplectic_basis,field,guard);
        const auto normalized_form=t.transpose(guard).multiply(original_form.multiply(t,guard),guard);
        auto pivots=positive(normalized_form,embedding,guard);
        A ab(g,g,field),bb(g,g,field);
        for(const auto& [ij,coefficient]:normalized_form.entries())if(ij.second>=g){
            if(ij.first<g)ab.set(ij.first,ij.second-g,coefficient);
            else bb.set(ij.first-g,ij.second-g,coefficient);
        }
        const auto imaginary=bb.solve(A::identity(g,field,guard),guard);
        const auto real=ab.multiply(imaginary,guard).scale(minus_one,guard);
        if(!(real==real.transpose(guard))||!(imaginary==imaginary.transpose(guard)))
            throw std::logic_error("finite period matrix failed the symmetric Riemann relation");
        (void)positive(imaginary,embedding,guard);
        const auto j_basis=t.solve(complex_structure.multiply(t,guard),guard);
        A real_forms(h,g,field),imaginary_forms(h,g,field);const auto one=field.scalar(Q{1});
        for(std::size_t i=0;i<g;++i)real_forms.set(i,i,one);
        for(const auto& [ij,coefficient]:real.entries())real_forms.set(g+ij.first,ij.second,coefficient);
        for(const auto& [ij,coefficient]:imaginary.entries())imaginary_forms.set(g+ij.first,ij.second,coefficient);
        if(!(j_basis.multiply(real_forms,guard)==imaginary_forms)||
            !(j_basis.multiply(imaginary_forms,guard)==real_forms.scale(minus_one,guard)))
            throw std::logic_error("holomorphic period forms do not satisfy the exact complex-structure equation");
        return PreparedFiniteJacobian(std::make_shared<const State>(State{lattice,embedding,std::move(symplectic_basis),star,
            std::move(normalization),complex_structure,normalized_form,real,imaginary,std::move(pivots)}));
    }
    static PreparedFiniteJacobian torus(const PreparedIntegralPeriodLattice& lattice,const numeric::AlgebraicElement& positive_sqrt_determinant,
        const geometry::SelectedRealAlgebraicEmbedding& embedding,numeric::ExactActionGuard& guard){
        if(lattice.period_rank()!=2)throw std::invalid_argument("torus normalization requires a complete rank-two period lattice");
        const auto field=embedding.field();if(!field.owns(positive_sqrt_determinant))throw std::invalid_argument("torus square-root field owner differs");
        const auto& g=lattice.gram();const auto determinant=guard.sub(guard.mul(g.get(0,0),g.get(1,1)),guard.mul(g.get(0,1),g.get(1,0)));
        if(positive_sqrt_determinant*positive_sqrt_determinant!=field.scalar(determinant)||embedding.sign(positive_sqrt_determinant)<=0)
            throw std::invalid_argument("torus normalization needs the declared positive exact square root");
        auto basis=numeric::IntegerMatrix::identity(2);if(lattice.cup_form().get(0,1)==-1)basis.set(1,1,numeric::Integer{-1});
        const auto normalization=A::identity(2,field,guard).scale(positive_sqrt_determinant.inverse(),guard);
        return normalize(lattice,std::move(basis),normalization,embedding,guard);
    }
    const PreparedIntegralPeriodLattice& lattice() const & noexcept{return state_->lattice;}
    const PreparedIntegralPeriodLattice& lattice() const &&=delete;
    const geometry::SelectedRealAlgebraicEmbedding& embedding() const & noexcept{return state_->embedding;}
    const geometry::SelectedRealAlgebraicEmbedding& embedding() const &&=delete;
    const numeric::IntegerMatrix& symplectic_basis() const & noexcept{return state_->symplectic_basis;}
    const numeric::IntegerMatrix& symplectic_basis() const &&=delete;
    const numeric::ExactMatrix& star() const & noexcept{return state_->star;}
    const numeric::ExactMatrix& star() const &&=delete;
    const A& complex_structure() const & noexcept{return state_->complex_structure;}
    const A& complex_structure() const &&=delete;
    const A& real_periods() const & noexcept{return state_->real_periods;}
    const A& real_periods() const &&=delete;
    const A& imaginary_periods() const & noexcept{return state_->imaginary_periods;}
    const A& imaginary_periods() const &&=delete;
    const std::vector<E>& positivity_pivots() const & noexcept{return state_->positive_pivots;}
    const std::vector<E>& positivity_pivots() const &&=delete;
    bool same_owner(const PreparedFiniteJacobian& other) const noexcept{return state_==other.state_;}
    FiniteJacobianPoint point(const IntegralPeriodPoint& point,numeric::ExactActionGuard& guard) const;
    bool supplied_vertices_are_distinct(const std::vector<IntegralPeriodPoint>& points,numeric::ExactActionGuard& guard) const;
    FiniteIsogenyCertificate declared_elliptic_factors(const std::vector<DeclaredPeriodPlane>& planes,numeric::ExactActionGuard& guard) const;
private:
    explicit PreparedFiniteJacobian(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};

class FiniteJacobianPoint {
public:
    const PreparedFiniteJacobian& owner() const & noexcept{return owner_;}
    const PreparedFiniteJacobian& owner() const &&=delete;
    const IntegralPeriodPoint& source_point() const & noexcept{return source_;}
    const IntegralPeriodPoint& source_point() const &&=delete;
    const std::vector<ExactComplexPeriod>& coordinates() const & noexcept{return coordinates_;}
    const std::vector<ExactComplexPeriod>& coordinates() const &&=delete;
    const numeric::RationalVector& real_lattice_coordinates() const & noexcept{return lattice_coordinates_;}
    const numeric::RationalVector& real_lattice_coordinates() const &&=delete;
    bool same_point(const FiniteJacobianPoint& other) const{
        if(!owner_.same_owner(other.owner_)||source_.base_vertex()!=other.source_.base_vertex())
            throw std::invalid_argument("Jacobian point comparison requires the same owned declaration and base vertex");
        return lattice_coordinates_==other.lattice_coordinates_;
    }
private:
    friend class PreparedFiniteJacobian;
    FiniteJacobianPoint(PreparedFiniteJacobian owner,IntegralPeriodPoint source,numeric::RationalVector lattice_coordinates,std::vector<ExactComplexPeriod> coordinates)
        :owner_(std::move(owner)),source_(std::move(source)),lattice_coordinates_(std::move(lattice_coordinates)),coordinates_(std::move(coordinates)){}
    PreparedFiniteJacobian owner_;IntegralPeriodPoint source_;numeric::RationalVector lattice_coordinates_;std::vector<ExactComplexPeriod> coordinates_;
};
inline FiniteJacobianPoint PreparedFiniteJacobian::point(const IntegralPeriodPoint& source_point,numeric::ExactActionGuard& guard) const{
    if(!lattice().same_owner(source_point.connecting_chain().owner()))throw std::invalid_argument("Abel-Jacobi period source owner differs");
    const auto h=lattice().period_rank(),g=lattice().genus();const auto field=embedding().field();
    numeric::RationalVector coordinates(h,numeric::Rational{0});
    for(const auto& [ij,value]:symplectic_basis().entries())coordinates[ij.second]=guard.add(coordinates[ij.second],guard.mul(numeric::Rational{value},source_point.coordinates()[ij.first]));
    for(auto& q:coordinates)q=period_detail::fractional(std::move(q));
    std::vector<ExactComplexPeriod> out;out.reserve(g);
    for(std::size_t i=0;i<g;++i)out.push_back({field.scalar(coordinates[i]),field.scalar(numeric::Rational{0})});
    for(const auto& [ij,value]:real_periods().entries()){guard.step();out[ij.first].real=out[ij.first].real+value*field.scalar(coordinates[g+ij.second]);jacobian_detail::value(out[ij.first].real,guard);}
    for(const auto& [ij,value]:imaginary_periods().entries()){guard.step();out[ij.first].imaginary=out[ij.first].imaginary+value*field.scalar(coordinates[g+ij.second]);jacobian_detail::value(out[ij.first].imaginary,guard);}
    return FiniteJacobianPoint(*this,source_point,std::move(coordinates),std::move(out));
}
inline bool PreparedFiniteJacobian::supplied_vertices_are_distinct(const std::vector<IntegralPeriodPoint>& input,numeric::ExactActionGuard& guard) const{
    std::vector<FiniteJacobianPoint> points;points.reserve(input.size());for(const auto& item:input)points.push_back(point(item,guard));
    for(std::size_t i=0;i<points.size();++i)for(std::size_t j=i+1;j<points.size();++j){guard.step();if(points[i].same_point(points[j]))return false;}
    return true;
}

class FiniteIsogenyCertificate {
public:
    const PreparedFiniteJacobian& owner() const & noexcept{return owner_;}
    const PreparedFiniteJacobian& owner() const &&=delete;
    const std::vector<FiniteEllipticFactor>& factors() const & noexcept{return factors_;}
    const std::vector<FiniteEllipticFactor>& factors() const &&=delete;
    const numeric::Integer& index() const & noexcept{return index_;}
    const numeric::Integer& index() const &&=delete;
    const numeric::IntegerMatrix& lattice_inclusion() const & noexcept{return inclusion_;}
    const numeric::IntegerMatrix& lattice_inclusion() const &&=delete;
private:
    friend class PreparedFiniteJacobian;
    FiniteIsogenyCertificate(PreparedFiniteJacobian owner,std::vector<FiniteEllipticFactor> factors,numeric::Integer index,numeric::IntegerMatrix inclusion)
        :owner_(std::move(owner)),factors_(std::move(factors)),index_(std::move(index)),inclusion_(std::move(inclusion)){}
    PreparedFiniteJacobian owner_;std::vector<FiniteEllipticFactor> factors_;numeric::Integer index_;numeric::IntegerMatrix inclusion_;
};
inline FiniteIsogenyCertificate PreparedFiniteJacobian::declared_elliptic_factors(const std::vector<DeclaredPeriodPlane>& declarations,numeric::ExactActionGuard& guard) const{
    using namespace jacobian_detail;using I=numeric::Integer;using Z=numeric::IntegerMatrix;
    const auto h=lattice().period_rank();if(machine::checked_size_mul(declarations.size(),2)!=h)throw std::invalid_argument("elliptic declarations do not cover the complete period carrier");
    const auto field=embedding().field();const auto squared=period_detail::product(star(),star(),guard);
    std::vector<FiniteEllipticFactor> factors;Z inclusion(h,h);
    for(std::size_t part=0;part<declarations.size();++part){
        const auto& declared=declarations[part];auto mu=declared.squared_frequency;mu.canonicalize();guard.value(mu);
        if(mu<=0||!field.owns(declared.positive_root)||
            declared.positive_root*declared.positive_root!=field.scalar(mu)||embedding().sign(declared.positive_root)<=0)
            throw std::invalid_argument("elliptic plane needs a positive rational square and a checked positive root");
        const auto& supplied=declared.rational_basis;guard.matrix(supplied);
        if(supplied.rows()!=h||supplied.cols()!=2)throw std::invalid_argument("elliptic factor requires an explicit rational two-column source plane");
        auto equation=squared;for(std::size_t i=0;i<h;++i)equation.set(i,i,guard.add(equation.get(i,i),mu));
        if(period_detail::product(equation,supplied,guard).nonzeros())throw std::invalid_argument("supplied period plane does not satisfy its declared source normalization identity");
        I denominator{1};for(const auto& [ij,q]:supplied.entries()){
            (void)ij;guard.step();const auto d=numeric::denominator(q);mpz_lcm(denominator.get_mpz_t(),denominator.get_mpz_t(),d.get_mpz_t());guard.value(Q{denominator});
        }
        Z integer_generators(h,2);
        for(const auto& [ij,q]:supplied.entries()){guard.step();I coefficient=numeric::numerator(q)*(denominator/numeric::denominator(q));guard.value(Q{coefficient});integer_generators.set(ij.first,ij.second,std::move(coefficient));}
        const auto smith=numeric::smith_normal_form_actions(integer_generators,guard);
        if(smith.rank!=2)throw std::invalid_argument("supplied rational period plane is not two-dimensional");
        // U*B*V=D proves span_Q(B)=U^{-1}span_Q(e0,e1). Thus these two
        // columns of U^{-1} generate its complete saturated integral lattice.
        Z selector(h,2);selector.set(0,0,I{1});selector.set(1,1,I{1});
        auto basis=smith.left.apply_inverse(selector,guard);auto bq=period_detail::rational(basis,guard);
        auto pairing=period_detail::product(bq.transpose(),period_detail::product(lattice().cup_form(),bq,guard),guard);
        auto w=pairing.get(0,1);if(w==0||numeric::denominator(w)!=1)throw std::invalid_argument("declared rational plane has no integral polarized lattice");
        if(w<0){basis.col_scale(1,I{-1},guard);w=-w;bq=period_detail::rational(basis,guard);}
        const auto gram=period_detail::product(bq.transpose(),period_detail::product(lattice().gram(),bq,guard),guard);
        const auto det=guard.sub(guard.mul(gram.get(0,0),gram.get(1,1)),guard.mul(gram.get(0,1),gram.get(1,0)));
        if(det!=guard.mul(mu,guard.mul(w,w))||gram.get(1,1)<=0)throw std::invalid_argument("declared elliptic plane metric/intersection identity failed");
        const auto algebraic_basis=A::from_integer(basis,field,guard);
        const auto restricted=A::from_rational(period_detail::product(star(),bq,guard),field,guard).scale(declared.positive_root.inverse(),guard);
        const auto j_image=complex_structure().multiply(algebraic_basis,guard);
        if(!(j_image==restricted))throw std::invalid_argument("declared complex structure differs from the proposed factor normalization");
        const auto pivots=algebraic_basis.column_basis(guard).pivot_rows;
        A minor(2,2,field),selected_image(2,2,field);
        for(std::size_t i=0;i<2;++i)for(std::size_t j=0;j<2;++j){
            minor.set(i,j,algebraic_basis.get(pivots[i],j));selected_image.set(i,j,j_image.get(pivots[i],j));
        }
        if(!(algebraic_basis.multiply(minor.solve(selected_image,guard),guard)==j_image))
            throw std::invalid_argument("supplied rational plane is not invariant under the declared complex structure");
        for(const auto& [ij,value]:basis.entries())inclusion.set(ij.first,2*part+ij.second,value);
        const auto real=field.scalar(guard.div(-gram.get(0,1),gram.get(1,1)));
        const auto imaginary=field.scalar(guard.div(w,gram.get(1,1)))*declared.positive_root;
        factors.push_back({mu,supplied,std::move(basis),numeric::numerator(w),gram,{real,imaginary}});
    }
    const auto index_smith=numeric::smith_normal_form_actions(inclusion,guard);
    if(index_smith.rank!=h)throw std::invalid_argument("declared elliptic lattices do not span the full rational period carrier");
    I index{1};for(const auto& d:index_smith.invariant_factors){index*=d;guard.value(Q{index});}
    return FiniteIsogenyCertificate(*this,std::move(factors),std::move(index),std::move(inclusion));
}
} // namespace rfc::field
