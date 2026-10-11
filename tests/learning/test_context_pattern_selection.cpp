#include "rfc/core/builder.hpp"
#include "rfc/learning/context_transport_pattern_bank_codec.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using G=numeric::ExactActionGuard;
namespace {
std::size_t checks{};
void check(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F&& f) {
    bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}
    check(rejected,"invalid selection accepted");
}
bool same_image(const M& a,const M& b) {
    if(a.rows()!=b.rows())return false;
    G guard;M both(a.rows(),a.cols()+b.cols());
    for(const auto& [ij,q]:a.entries())both.set(ij.first,ij.second,q);
    for(const auto& [ij,q]:b.entries())both.set(ij.first,a.cols()+ij.second,q);
    const numeric::PreparedExactFrames x(a,guard),y(b,guard),z(both,guard);
    return x.rank()==y.rank() && x.rank()==z.rank();
}
auto bank(std::size_t count=5) {
    core::RelationSignature signature;
    const auto type=signature.add_type("stored",core::BoundaryPolicy::DeclaredExact,0,6);
    core::RelationalComplexBuilder builder(signature);
    for(std::size_t i=0;i<count;++i)builder.add_grade_one(type,{
        {0,true,true,Q{1},{}},{0,true,false,Q{1},{}},{1,true,false,Q{1,2},{}},
        {2,false,false,Q{3},{}},{3+i,true,false,Q{0},{}}});
    storage::TemporalEndpoint endpoint(builder.freeze());
    const auto lower=endpoint.source.grade(core::Grade{0}).cell_count();
    const auto& arena=endpoint.source.grade(core::Grade{1});
    std::vector<std::size_t> dimensions(lower,1);
    dimensions[arena.lower_index[2]]=2;
    const auto carrier=[&](std::uint64_t key,std::uint64_t frame,std::size_t size,
        std::optional<storage::StableCellKey> cell={}) {
        storage::TemporalCarrier c;c.key=key;c.coordinate_system=frame;c.domain=numeric::IndexDomain::linear(size);
        if(cell){c.kind=storage::TemporalCarrierKind::CellStalk;c.cell=cell;c.grade=cell->grade;}
        endpoint.carriers.push_back(std::move(c));
    };
    std::size_t weight_count{};
    for(std::size_t i=0;i<count;++i) {
        carrier(3+i,100+i,2,storage::stable_cell_key(endpoint.source,{core::Grade{1},static_cast<std::uint32_t>(i)}));
        carrier(3+count+i,2,3);
        M domain(3,2);domain.set(0,0,1);domain.set(1,1,1);domain.set(2,0,1);domain.set(2,1,1);
        storage::TemporalTensor image;image.key=5+i;image.role=storage::TemporalTensorRole::Accession;
        image.input=3+i;image.output=3+count+i;image.interpretation="shared-bank-input-image";
        image.tensor=storage::temporal_matrix_tensor(domain);endpoint.tensors.push_back(std::move(image));
        const auto [begin,end]=arena.incidence_range(i);
        for(auto p=begin;p<end;++p)weight_count+=dimensions[arena.lower_index[p]]*2;
    }
    carrier(1,10,weight_count);carrier(2,11,3);
    for(std::size_t i=0;i<lower;++i)carrier(3+2*count+i,dimensions[i]==2?3:1,dimensions[i],
        storage::stable_cell_key(endpoint.source,{core::Grade{0},static_cast<std::uint32_t>(i)}));
    std::vector<numeric::SparseRationalEntry> weights;M directions(weight_count,3);std::size_t row{};
    for(std::size_t i=0;i<count;++i) {
        const auto [begin,end]=arena.incidence_range(i);
        for(auto p=begin;p<end;++p)for(std::size_t r=0;r<dimensions[arena.lower_index[p]];++r)
            for(std::size_t j=0;j<2;++j,++row) {
                const auto value=Q{numeric::Integer(std::to_string(1+r+j)),numeric::Integer{3}};
                weights.push_back({row,value});directions.set(row,i<2?0:1,j?Q{2}:Q{1});
                if(i<2 && j)directions.set(row,2,1);
            }
    }
    storage::TemporalTensor w;w.key=1;w.role=storage::TemporalTensorRole::Field;w.output=1;
    w.interpretation="ordered-bank-occurrence-weights";
    w.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(weight_count),numeric::StructuralSupport::full(weight_count),
        numeric::SparseRationalVector(weight_count,std::move(weights)));endpoint.tensors.push_back(std::move(w));
    storage::TemporalTensor f;f.key=2;f.role=storage::TemporalTensorRole::PhysicalRealization;f.input=2;f.output=1;
    f.interpretation="joint-observable-parameter-directions";f.tensor=storage::temporal_matrix_tensor(directions);endpoint.tensors.push_back(std::move(f));
    std::vector<numeric::SparseRationalEntry> receipt;
    for(std::size_t i=0;i<64;++i)receipt.push_back({i,Q{i<32?7:11}});
    receipt.push_back({64,9});receipt.push_back({65,12});
    storage::TemporalTensor p;p.key=3;p.role=storage::TemporalTensorRole::OpaqueExact;
    p.interpretation="rfc.context-transport-pattern-bank.v1";
    p.tensor=numeric::SparseRationalTensor(numeric::IndexDomain::linear(66),numeric::StructuralSupport::full(66),
        numeric::SparseRationalVector(66,std::move(receipt)));endpoint.tensors.push_back(std::move(p));
    std::sort(endpoint.carriers.begin(),endpoint.carriers.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    std::sort(endpoint.tensors.begin(),endpoint.tensors.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    G guard;return learning::PreparedTransportPatternBank::from_endpoint(std::move(endpoint),guard)
        .with_frame_labels({{1,"scalar",{"value"}},{2,"input",{"first","second","sum"}},
            {3,"paired",{"left","right"}}},guard);
}
learning::TransportPatternSparseInput input(std::size_t pattern,Q a=1,Q b=2) {
    std::vector<numeric::SparseRationalEntry> entries;
    if(a!=0)entries.push_back({0,a});
    if(b!=0)entries.push_back({1,b});
    if(a+b!=0)entries.push_back({2,a+b});
    return {pattern,2,numeric::SparseRationalTensor(numeric::IndexDomain::linear(3),numeric::StructuralSupport::full(3),
        numeric::SparseRationalVector(3,std::move(entries)))};
}
void compare_response(const learning::PreparedTransportPatternBank& parent,
    const learning::PreparedTransportPatternBank& local,const std::vector<std::size_t>& patterns,bool shared,Q a,Q b) {
    std::vector<learning::TransportPatternSparseInput> before,after;
    for(std::size_t i=0;i<patterns.size();++i){before.push_back(input(patterns[i],a,b));after.push_back(input(i,a,b));}
    G guard;const auto whole=parent.response_sparse(before,guard,shared),selected=local.response_sparse(after,guard,shared);
    std::vector<std::size_t> rows;
    if(shared) {
        const auto& cells=parent.endpoint().source.grade(core::Grade{0}).identities;
        for(const auto id:local.endpoint().source.grade(core::Grade{0}).identities) {
            const auto old=static_cast<std::size_t>(std::find(cells.begin(),cells.end(),id)-cells.begin());
            for(auto r=whole.offsets[old];r<whole.offsets[old+1];++r)rows.push_back(r);
        }
    } else for(const auto pattern:patterns) {
        const auto [begin,end]=parent.endpoint().source.grade(core::Grade{1}).incidence_range(pattern);
        for(auto r=whole.offsets[begin];r<whole.offsets[end];++r)rows.push_back(r);
    }
    V origin;M directions(rows.size(),whole.directions.cols());
    for(std::size_t i=0;i<rows.size();++i) {
        origin.push_back(whole.origin[rows[i]]);
        for(std::size_t j=0;j<whole.directions.cols();++j)directions.set(i,j,whole.directions.get(rows[i],j));
    }
    check(origin==selected.origin,"selected boundary origin differs");
    check(same_image(directions,selected.directions),"selected joint family differs");
    check(selected.offsets.back()==rows.size(),"selected field retains unrelated output coordinates");
}
void selections() {
    const auto parent=bank();const auto original=learning::encode_transport_pattern_bank(parent,800);G guard;
    for(const auto& patterns:std::vector<std::vector<std::size_t>>{{0},{1},{2},{0,1},{0,2},{1,3,4},{0,1,2,3,4}}) {
        const auto local=parent.select(patterns,900,901,guard);
        check(local.input_images().size()==patterns.size(),"selected pattern count changed");
        check(local.provenance().source_sha256==parent.provenance().source_sha256 &&
            local.provenance().evidence_sha256==parent.provenance().evidence_sha256 &&
            local.provenance().observations==9 && local.provenance().peers==12,"original provenance replaced by selection counts");
        check(local.frame_labels().size()==3 && local.frame_labels()[1].axes==parent.frame_labels()[1].axes,"full named carriers changed");
        const auto& a=parent.endpoint().source.grade(core::Grade{1});const auto& b=local.endpoint().source.grade(core::Grade{1});
        for(std::size_t i=0;i<patterns.size();++i) {
            check(a.relation_identities[patterns[i]]==b.relation_identities[i],"stored relation identity changed");
            const auto [start,end]=a.incidence_range(patterns[i]);const auto [first,last]=b.incidence_range(i);
            check(end-start==last-first,"repeated or inactive occurrence removed");
            for(auto p=start;p<end;++p) {
                const auto q=first+p-start;
                check(a.occurrence_identities[p]==b.occurrence_identities[q],"occurrence identity or order changed");
                check(parent.endpoint().source.incidence_coefficient(core::Grade{1},p)==
                    local.endpoint().source.incidence_coefficient(core::Grade{1},q),"exact incidence coefficient changed");
                for(std::size_t aspect=0;aspect<a.aspects.aspect_count();++aspect)
                    check(a.aspects.get(static_cast<composite::AspectId>(aspect),p)==
                        b.aspects.get(static_cast<composite::AspectId>(aspect),q),"retained aspect changed");
            }
        }
        for(const bool shared:{false,true})for(const auto& values:std::vector<std::pair<Q,Q>>{{0,0},{1,2},{Q{3,7},Q{-5,11}},{-1,1}})
            compare_response(parent,local,patterns,shared,values.first,values.second);
        const auto bytes=learning::encode_transport_pattern_bank(local,801);
        const auto loaded=learning::decode_transport_pattern_bank(bytes,801,guard);
        check(learning::encode_transport_pattern_bank(loaded,801)==bytes,"cold selection bytes differ");
        compare_response(parent,loaded,patterns,true,Q{3,5},Q{-2,7});
    }
    const auto tied=parent.select({0,1},900,901,guard);
    check(tied.parameter_directions().cols()==2 && parent.parameter_directions().cols()==3,"unrelated parameter direction retained");
    const auto cancelled=tied.response_sparse({input(0),input(1,-1,-2)},guard);
    check(std::all_of(cancelled.origin.begin(),cancelled.origin.end(),[](const auto& q){return q==0;}) &&
        !cancelled.directions.cols(),"shared joint parameters were independently replaced");
    const auto full=parent.select({0,1,2,3,4},900,901,guard);
    const auto nested=full.select({0,1},902,903,guard);
    compare_response(tied,nested,{0,1},true,Q{3,7},Q{-2,11});
    check(learning::encode_transport_pattern_bank(parent,800)==original,"selection mutated parent archive");
    std::vector<std::future<std::vector<std::byte>>> jobs;
    for(int i=0;i<4;++i)jobs.push_back(std::async(std::launch::async,[parent]{G g;return learning::encode_transport_pattern_bank(parent.select({0,1},900,901,g),801);}));
    const auto bytes=learning::encode_transport_pattern_bank(tied,801);
    for(auto& job:jobs)check(job.get()==bytes,"concurrent selection differs");
    for(const auto& patterns:std::vector<std::vector<std::size_t>>{{},{1,0},{0,0},{5},{0,6}})
        rejects([&]{G g;(void)parent.select(patterns,900,901,g);});
    for(const auto& coords:std::vector<std::pair<std::uint64_t,std::uint64_t>>{{0,901},{900,0},{900,900},{10,901},{900,11},{1,901},{900,104}})
        rejects([&]{G g;(void)parent.select({0},coords.first,coords.second,g);});
    rejects([&]{G g({},[]{throw std::runtime_error("cancelled");});(void)parent.select({0},900,901,g);});
    numeric::ExactActionLimits limits;limits.entries=parent.retained_entries();
    rejects([&]{G g(limits);(void)parent.select({0},900,901,g);});
    limits={};limits.steps=4;rejects([&]{G g(limits);(void)parent.select({0},900,901,g);});
    limits={};limits.dimension=1;rejects([&]{G g(limits);(void)parent.select({0,1},900,901,g);});
    auto endpoint=parent.endpoint();endpoint.tensors.erase(std::remove_if(endpoint.tensors.begin(),endpoint.tensors.end(),[](const auto& t){return t.key==4;}),endpoint.tensors.end());
    const auto unlabeled=learning::PreparedTransportPatternBank::from_endpoint(std::move(endpoint),guard);
    const auto unlabelled_selection=unlabeled.select({0},900,901,guard);
    check(unlabelled_selection.frame_labels().empty(),"selection invented frame labels");
}
}
int main() {
    try{selections();std::cout<<"context pattern selection: "<<checks<<" assertions passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
