#include "../../examples/algebraic_metrics/model.hpp"
#include <iostream>

using namespace metric_example;
static std::size_t checks{};
static void require(bool value,const char* message) {if (!value) throw std::runtime_error(message);++checks;}
template<class Error,class F> static void rejects(F action,const char* message) {
    bool caught=false;try {action();} catch (const Error&) {caught=true;}require(caught,message);
}
template<class T> concept TemporaryFactors=requires(T value) {std::move(value).lower_factor();};
template<class T> concept TemporaryDomain=requires(T value) {std::move(value).domain();};
static_assert(!std::default_initializable<Domain> && !std::default_initializable<Metric>);
static_assert(!std::default_initializable<rfc::local::AlgebraicCovector>);
static_assert(!TemporaryFactors<Metric> && !TemporaryDomain<Form>);

static void metrics(const Case& sample) {
    const auto& domain=sample.domain;const auto& f=domain.field();const auto model=cellular_example::make(f);
    const auto u=model.bundle.stalk({G{0},0}),v=model.bundle.stalk({G{0},1}),w=model.bundle.stalk({G{0},2});
    const auto one=f.scalar(Q{1}),zero=f.scalar(Q{0}),alpha=f.generator();
    const auto mu=metric(domain,u),mv=metric(domain,v,Q{4}),mw=metric(domain,w,Q{5});
    const auto t=one+alpha;const auto conjugate=domain.involution().apply(t);
    const auto l=Map::declare(u,u,mu.lower_factor());std::vector<E> pivots;
    for (const auto& p:mu.pivot_certificates()) {
        require(p.sign==1 && p.interval.lower>0 && p.interval.upper>=p.interval.lower,"every exact pivot is certified positive");
        require(domain.involution().apply(p.value)==p.value,"positive pivots are fixed scalars");pivots.push_back(p.value);
    }
    const auto diag=Map::declare(u,u,{pivots[0],zero,zero,pivots[1]});
    const auto lstar=Map::declare(u,u,{one,t,zero,one});
    const auto reconstruction=lstar.then(diag).then(l);
    require(reconstruction.coefficients()==mu.measurement().coefficient_form().coefficients(),"complete native LDL reconstruction");
    require(mu.lower_factor()==std::vector<E>{one,zero,conjugate,one},"factor coefficients retain exact K values");
    for (std::size_t i=0;i<7;++i) {
        const auto x=u.vector({alpha+f.scalar(Q(rfc::numeric::Integer(i))),alpha*alpha+one});
        const auto solved=mu.solve(x),applied=mu.apply(solved);
        require(applied.coordinates()==x.coordinates(),"native reusable solve has complete exact residual");
        const auto covector=mu.measurement().as_covector(x);const auto recovered=mu.from_covector(covector);
        require(covector.evaluate(x)==mu.quadrance(x),"declared metric dual evaluation");
        require(recovered.coordinates()==x.coordinates(),"metric identification inverse retains native coefficients");
        require(domain.certify_sign(mu.quadrance(x)).sign==1,"nonzero native vector quadrance is positive");
        require(x.rational_coordinates().size()==u.rank()*f.degree(),"full rational carrier retained rather than projected to complex coordinates");
    }
    const auto a=Map::declare(u,v,{one,alpha,one+alpha,zero}),b=Map::declare(v,w,{one,t,zero,one});
    const auto adj=rfc::field::algebraic_metric_adjoint(a,mu,mv),back=rfc::field::algebraic_metric_adjoint(adj,mv,mu);
    require(back.coefficients()==a.coefficients(),"double metric adjoint recovers the complete map");
    const auto composite=a.then(b),direct=rfc::field::algebraic_metric_adjoint(composite,mu,mw);
    const auto reverse=rfc::field::algebraic_metric_adjoint(b,mv,mw).then(adj);
    require(reverse.coefficients()==direct.coefficients(),"adjoint reverses chronological noncommuting composition");
    const auto x=u.vector({t,alpha*alpha}),y=v.vector({alpha,f.scalar(Q{3})});
    const auto ax=a.apply(x),ady=adj.apply(y);
    require(mv.inner(ax,y)==mu.inner(x,ady),"exact selected-metric adjoint identity");
    const auto applied_adjoint=rfc::field::algebraic_adjoint_apply(a,mu,mv,y);
    require(ady.coordinates()==applied_adjoint.coordinates(),"adjoint action and materialized map agree");
    const auto raw=rfc::local::AlgebraicCovector::declare(v,{alpha,one});const auto dual=rfc::local::AlgebraicDualMap(a);
    require(dual.apply(raw).evaluate(x)==raw.evaluate(ax),"raw algebraic Stokes evaluation without metric or conjugation");
    require(dual.rational_coordinate_transpose()==a.rational_action().transpose(),"coordinate transpose retained separately from metric adjoint");
    require(adj.rational_action()!=dual.rational_coordinate_transpose(),"nonidentity native metric adjoint is not a coordinate transpose");
    const auto pulled=mv.measurement().pullback(a);const auto pulled_metric=Metric::certify(pulled);
    require(pulled.inner(x,x)==mv.inner(ax,ax),"native Gram pullback preserves pairing");
    require(pulled_metric.carrier().same_owner(u),"pullback metric binds the actual source carrier");
    const auto zero_map=Map::zero(u,v);const auto singular=mv.measurement().pullback(zero_map);
    require(singular.quadrance(x).is_zero(),"supported zero pullback remains a measurement form");
    rejects<std::invalid_argument>([&]{(void)Metric::certify(singular);},"singular pullback cannot become a positive metric");
    const auto select0=Map::declare(u,v,{one,zero,zero,zero}),select1=Map::declare(u,w,{zero,zero,zero,one});
    const auto identity_v=Metric::identity(domain,v),identity_w=Metric::identity(domain,w);
    const auto partial=identity_v.measurement().pullback(select0);
    rejects<std::invalid_argument>([&]{(void)Metric::certify(partial);},"an unmeasured direction prevents metric certification");
    const auto joined=partial.add(identity_w.measurement().pullback(select1));
    const auto joined_metric=Metric::certify(joined);const auto identity_u=Map::identity(u);
    require(joined_metric.measurement().coefficient_form().coefficients()==identity_u.coefficients(),
        "jointly separating local restrictions assemble a complete metric");
    const auto indefinite=Form::declare(domain,Map::declare(u,u,{one,zero,zero,-one}));
    rejects<std::invalid_argument>([&]{(void)Metric::certify(indefinite);},"nonsingular indefinite forms fail positive metric hypotheses");
    rejects<std::invalid_argument>([&]{(void)Form::declare(domain,Map::declare(u,u,{one,one,zero,one}));},"non-Hermitian form rejected");
    rejects<std::invalid_argument>([&]{(void)Form::declare(domain,a);},"equal rank does not authorize a form between distinct carriers");
    rejects<std::invalid_argument>([&]{(void)mu.solve(y);},"solve rejects equal-sized foreign carrier");
    rejects<std::invalid_argument>([&]{(void)raw.evaluate(x);},"raw dual rejects equal-sized foreign carrier");
    rejects<std::invalid_argument>([&]{(void)rfc::field::algebraic_metric_adjoint(a,mv,mu);},"adjoint requires the actual carriers");
    const auto foreign_model=cellular_example::make(f);const auto foreign=foreign_model.bundle.stalk(u.cell());
    rejects<std::invalid_argument>([&]{(void)mu.quadrance(foreign.vector({zero,zero}));},"foreign supported zero still checks carrier owner");
    auto budget=domain.limits();budget.max_factor_steps=0;
    const auto bounded=std::visit([&](const auto& embedding) {
        if constexpr (std::same_as<std::decay_t<decltype(embedding)>,rfc::geometry::SelectedRealAlgebraicEmbedding>)
            return Domain::real(embedding,domain.involution(),budget);
        else return Domain::hermitian(embedding,domain.involution(),budget);
    },domain.embedding());
    rejects<std::length_error>([&]{(void)Metric::identity(bounded,u);},"factor work exhaustion is not false positivity");
    const auto equal_domain=std::visit([&](const auto& embedding) {
        if constexpr (std::same_as<std::decay_t<decltype(embedding)>,rfc::geometry::SelectedRealAlgebraicEmbedding>)
            return Domain::real(embedding,domain.involution());
        else return Domain::hermitian(embedding,domain.involution());
    },domain.embedding());
    require(!equal_domain.same_owner(domain),"equal metric declarations do not infer domain identity");
    const auto foreign_metric=Metric::identity(equal_domain,v);
    rejects<std::invalid_argument>([&]{(void)rfc::field::algebraic_metric_adjoint(a,mu,foreign_metric);},"metric adjoint domain owners are explicit");
    const auto retained=[&]{const auto temporary=metric(domain,u);return temporary;}();
    require(retained.solve(x).rational_coordinates()==mu.solve(x).rational_coordinates(),"metric owner pins factors after declaration wrappers die");
    if (domain.is_real()) {
        rejects<std::invalid_argument>([&]{(void)mu.certify_rational_trace_metric();},"real cubic selected positivity does not imply trace-form positivity");
        const auto witness=u.vector({alpha-alpha*alpha,zero});
        const auto identity=Metric::identity(domain,u);const auto trace_form=identity.measurement().rational_trace_form();
        require(domain.certify_sign(identity.quadrance(witness)).sign==1,"real cubic witness has positive selected quadrance");
        require(rfc::numeric::dot(witness.rational_coordinates(),trace_form.apply(witness.rational_coordinates()))==Q{-4},
            "real cubic full trace form has a retained exact negative witness");
    } else {
        const auto trace=mu.certify_rational_trace_metric();const auto xr=x.rational_coordinates();
        const Q trace_quad=mu.quadrance(x).trace()/Q(rfc::numeric::Integer(f.degree()));
        require(trace.size()==u.rank()*f.degree() && trace.inner(xr,xr)==trace_quad,"explicit full rational trace metric independently certified");
        const auto fixed=alpha+domain.involution().apply(alpha);
        require(domain.certify_sign(fixed).sign>=0,"conjugation compatibility proves fixed scalar image is real");
        rejects<std::invalid_argument>([&]{(void)domain.certify_sign(alpha);},"complex nonfixed scalar has no real sign");
        if (f.degree()>2) {
            const auto h=Map::identity(u).scale(fixed);const auto selected=Metric::certify(Form::declare(domain,h));
            rejects<std::invalid_argument>([&]{(void)selected.certify_rational_trace_metric();},"selected-positive algebraic weight can be indefinite at other embeddings");
            if (f.degree()==4) {
                const auto witness=u.vector({one-alpha,zero});
                require(domain.certify_sign(selected.quadrance(witness)).sign==1,"quartic weighted witness has positive selected quadrance");
                const Q trace_witness=selected.quadrance(witness).trace()/Q{4};
                require(trace_witness==Q{-2},"quartic weighted trace form has an exact negative witness");
            }
        }
    }
    require(domain.certify_sign(zero).sign==0 && domain.certify_sign(-one).sign==-1,"exact zero and negative fixed signs");
}

static void fields(const Case& sample,bool top_deformation=false,bool partial=false,Q magnitude=Q{1},bool reverse=false) {
    const auto& domain=sample.domain;const auto model=cellular_example::make(domain.field(),!partial,false,partial,magnitude,top_deformation,2,reverse);
    const auto prepared=model.prepare(top_deformation?rfc::local::AlgebraicChainPolicy::DeclaredDeformation:rfc::local::AlgebraicChainPolicy::RequireFlat);
    const auto& system=prepared.realization();const auto field=metric_field(domain,system);const auto& source=model.bundle.source();
    require(field.system().same_owner(system),"metrics pin the original full-chain fiber system");
    require(system.flat()!=top_deformation,"metric changes preserve full realized chain-family verdict");
    for (std::size_t k=0;k<source.grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)};const auto input=values(field,grade);
        const auto h=field.hodge(grade,input);const auto q=field.adjacent_quadrance(grade,input);
        require(field.inner(grade,input,h)==q,"Hodge quadrance equals the complete adjacent field measure");
        require(domain.certify_sign(q).sign>=0,"full Hodge measure is positive semidefinite without claiming flatness");
        auto scalar=input;const auto a=domain.field().generator();
        for (auto& vector:scalar) {auto c=vector.coordinates();for (auto& x:c) x=a*x;vector=vector.carrier().vector(std::move(c));}
        const auto scaled=field.hodge(grade,scalar);
        for (std::size_t i=0;i<h.size();++i) for (std::size_t j=0;j<h[i].coordinates().size();++j)
            require(scaled[i].coordinates()[j]==a*h[i].coordinates()[j],"Hodge action remains K-linear on the full native carrier");
        if (!k) continue;
        const auto lower=G{static_cast<std::uint16_t>(k-1)};const auto y=values(field,lower);
        const auto boundary=field.boundary(grade,input),adjoint=field.coboundary(grade,y);
        require(field.inner(lower,boundary,y)==field.inner(grade,input,adjoint),"whole-grade boundary adjunction through retained native occurrences");
        const auto original=system.apply(grade,input);
        for (std::size_t i=0;i<boundary.size();++i)
            require(original[i].coordinates()==boundary[i].coordinates(),"metrics do not replace the prepared native boundary");
        const auto& arena=source.grade(grade);
        for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
            const auto& map=system.occurrence_map(grade,slot);const auto& reverse_map=field.occurrence_adjoint(grade,slot);
            require(reverse_map.source().same_owner(map.target()) && reverse_map.target().same_owner(map.source()),"every retained reverse action has actual carriers");
            if (!arena.aspects.get(source.signature().existence_aspect(),slot))
                require(!reverse_map.rational_action().entries().empty(),"inactive nonzero reverse maps remain retained");
        }
        if (k>=2) {
            const auto two=field.coboundary(grade,field.coboundary(lower,values(field,G{static_cast<std::uint16_t>(k-2)})));
            bool nonzero=false;for (const auto& vector:two) for (const auto& x:vector.coordinates()) nonzero=nonzero || !x.is_zero();
            require(nonzero==(top_deformation && k==4),"reverse chain family is flat exactly where the retained source family is flat");
        }
    }
    if (top_deformation) {
        rejects<std::logic_error>([&]{(void)field.system().homology_dimensions_over_field();},"metrics cannot authorize homology of a deformed nonflat tower");
        require(!system.rational_chain_defect(G{4}).entries().empty(),"grade-four-only curvature stays exposed");
    }
    std::vector<Metric> declared;
    for (std::size_t k=0;k<source.grade_count();++k) for (std::size_t i=0;i<source.grade(G{static_cast<std::uint16_t>(k)}).cell_count();++i)
        declared.push_back(field.metric({G{static_cast<std::uint16_t>(k)},static_cast<std::uint32_t>(i)}));
    auto duplicate=declared;duplicate.back()=duplicate.front();
    rejects<std::invalid_argument>([&]{(void)rfc::field::AlgebraicFiberMetricField::declare(system,domain,duplicate);},"duplicate metric cell cannot conceal missing cell");
    auto missing=declared;missing.pop_back();
    rejects<std::invalid_argument>([&]{(void)rfc::field::AlgebraicFiberMetricField::declare(system,domain,missing);},"complete retained metric family required");
    rejects<std::length_error>([&]{(void)rfc::field::AlgebraicFiberMetricField::declare(system,domain,declared,{0});},"retained metric and adjoint coordinate budget enforced");
    auto wrong=values(field,G{0});std::swap(wrong[0],wrong[1]);
    rejects<std::invalid_argument>([&]{(void)field.coboundary(G{1},wrong);},"complete grade vectors retain physical cell order");
    rejects<std::out_of_range>([&]{(void)field.coboundary(G{0},wrong);},"boundary grade zero has no coboundary input");
}

static void compatibility(const std::vector<Case>& profiles) {
    const auto& complex=profiles[1].domain;const auto& f=complex.field();
    const auto& root=std::get<rfc::geometry::SelectedComplexAlgebraicEmbedding>(complex.embedding());
    rejects<std::invalid_argument>([&]{(void)Domain::hermitian(root,Involution::identity(f));},"a declared identity involution is not complex conjugation");
    const auto foreign=Field::declare(f.minimal_polynomial(),f.certificate());
    rejects<std::invalid_argument>([&]{(void)Domain::hermitian(root,Involution::declare(foreign,-foreign.generator()));},"equal field polynomials do not bind a metric domain");
    rejects<std::invalid_argument>([&]{(void)complex.certify_sign(foreign.scalar(Q{0}));},"foreign native zero cannot bypass sign field identity");
    const auto quadratic=Field::declare({-2,0,1},rfc::numeric::EisensteinShift{2,0});
    const auto selected=rfc::geometry::SelectedRealAlgebraicEmbedding::isolate(quadratic,1,2);
    rejects<std::invalid_argument>([&]{(void)Domain::real(selected,Involution::declare(quadratic,-quadratic.generator()));},"nontrivial real field automorphism is not physical conjugation");
    auto limits=rfc::field::AlgebraicMetricLimits{};limits.max_sign_refinements=0;
    const auto bounded=Domain::real(selected,Involution::identity(quadratic),limits);
    rejects<std::length_error>([&]{(void)bounded.certify_sign(quadratic.generator()-quadratic.scalar(Q{7,5}));},"inconclusive exact sign remains budget exhaustion");
    const auto unbounded=Domain::real(selected,Involution::identity(quadratic));
    const auto positive=unbounded.certify_sign(quadratic.generator()-quadratic.scalar(Q{7,5}));
    require(positive.sign==1 && positive.refinements>0 && positive.interval.lower>0,"near-zero real algebraic sign uses exact refinement");
    const auto& quartic=profiles[2].domain;const auto& qf=quartic.field();const auto alpha=qf.generator();
    const auto fixed=alpha+quartic.involution().apply(alpha)-qf.scalar(Q{7,5});
    const auto certificate=quartic.certify_sign(fixed);
    require(certificate.sign==1 && certificate.refinements>0 && certificate.interval.lower>0,"fixed complex scalar sign refines to a certified rational positive bound");
}
static void higher_rank(const Case& sample) {
    const auto& domain=sample.domain;const auto& f=domain.field();const auto source=cellular_example::source(false,Q{1});
    for (const auto rank:{std::size_t{1},std::size_t{3},std::size_t{5}}) {
        std::vector<std::vector<std::size_t>> ranks;
        for (std::size_t k=0;k<source.grade_count();++k)
            ranks.emplace_back(source.grade(G{static_cast<std::uint16_t>(k)}).cell_count(),rank);
        const auto bundle=rfc::local::AlgebraicStalkBundle::declare(source,f,std::move(ranks));
        const auto carrier=bundle.stalk({G{0},0});
        const auto one=f.scalar(Q{1}),zero=f.scalar(Q{0}),alpha=f.generator();
        std::vector<E> g(rank*rank,zero),d(rank*rank,zero),x;
        for (std::size_t i=0;i<rank;++i) {
            g[i*rank+i]=one;d[i*rank+i]=f.scalar(Q(rfc::numeric::Integer(i+2)));
            x.push_back(alpha.power(i+1)+one);
            for (std::size_t j=i+1;j<rank;++j) g[i*rank+j]=alpha.power(j-i)+one;
        }
        const auto weighted=Metric::certify(Form::declare(domain,Map::declare(carrier,carrier,std::move(d))));
        const auto basis=Map::declare(carrier,carrier,std::move(g));
        const auto declared=Metric::certify(weighted.measurement().pullback(basis));const auto input=carrier.vector(x);
        const auto output=declared.apply(declared.solve(input));
        require(output.coordinates()==x,"higher-rank native solve preserves the complete carrier");
        require(domain.certify_sign(declared.quadrance(input)).sign==1,"higher-rank Gram form certifies positivity");
        require(declared.pivot_certificates().size()==rank,"complete higher-rank LDL pivot family");
        const auto identity=Metric::identity(domain,carrier);
        const auto adj=rfc::field::algebraic_metric_adjoint(basis,identity,identity);
        const auto complete=basis.then(adj);
        const auto gram=identity.measurement().pullback(basis);
        require(complete.coefficients()==gram.coefficient_form().coefficients(),
            "higher-rank adjoint and local assembly agree without dimension reduction");
    }
}
static void supported_zero(const Case& sample) {
    const auto model=cellular_example::make(sample.domain.field());
    const auto prepared=model.prepare(rfc::local::AlgebraicChainPolicy::RequireFlat);const auto& original=prepared.realization();
    std::vector<rfc::local::AlgebraicOccurrenceMap> maps;
    for (std::size_t k=1;k<model.bundle.source().grade_count();++k) {
        const auto grade=G{static_cast<std::uint16_t>(k)};const auto& arena=model.bundle.source().grade(grade);
        for (std::size_t slot=0;slot<arena.incidence_count();++slot) {
            const auto& map=original.occurrence_map(grade,slot);
            const bool active=arena.aspects.get(model.bundle.source().signature().existence_aspect(),slot);
            maps.push_back({grade,slot,k==1 && active?Map::zero(map.source(),map.target()):map});
        }
    }
    const auto system=rfc::local::AlgebraicRelationalFiberSystem::declare(model.bundle,maps,rfc::local::AlgebraicChainPolicy::RequireFlat);
    const auto field=metric_field(sample.domain,system);
    const auto output=field.coboundary(G{1},values(field,G{0}));
    for (const auto& vector:output) for (const auto& coordinate:vector.coordinates())
        require(coordinate.is_zero(),"active supported zero maps give exact zero coboundary values");
    const auto& arena=model.bundle.source().grade(G{1});
    require(arena.incidence_count()==7 && field.system().flat(),"supported zero retains the complete original source and chain family");
    for (std::size_t slot=0;slot<arena.incidence_count();++slot)
        require(field.occurrence_adjoint(G{1},slot).rational_action().entries().empty()==
            arena.aspects.get(model.bundle.source().signature().existence_aspect(),slot),
            "supported zero and inactive nonzero reverse maps remain distinct");
}
int main() {
    const auto profiles=cases();compatibility(profiles);
    for (const auto& sample:profiles) {
        metrics(sample);higher_rank(sample);fields(sample);fields(sample,true);fields(sample,false,true);
        fields(sample,false,false,Q{3,2},true);supported_zero(sample);
    }
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"profiles\":"<<profiles.size()<<"}\n";
}
