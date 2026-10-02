#include "rfc_preview/execution.hpp"
#include <iostream>
#include <charconv>
using namespace rfc_preview;
namespace {
std::uint64_t number(const char* s){std::uint64_t n{};auto end=s+std::strlen(s);auto [p,ec]=std::from_chars(s,end,n);require(ec==std::errc{}&&p==end,"invalid unsigned number");return n;}
void inspect(const Database& db){const auto tail=db.series().tail();const auto& e=tail.endpoint();std::cout<<"{\"namespace\":\""<<db.manifest().name_space
    <<"\",\"first\":"<<db.manifest().first<<",\"last\":"<<db.manifest().last<<",\"components\":"<<db.manifest().entries.size()
    <<",\"bytes\":"<<db.bytes().size()<<",\"retained_relations\":"<<(e.source.grade_count()>1?e.source.grade(rfc::core::Grade{1}).cell_count():0)
    <<",\"retained_occurrences\":"<<(e.source.grade_count()>1?e.source.grade(rfc::core::Grade{1}).incidence_count():0)<<",\"aspects\":"<<e.source.signature().aspects().size()
    <<",\"sha256\":\""<<hex(db.publication_digest())<<"\"}\n";}
}
int main(int argc,char** argv){try{
    require(argc>=3,"usage: rfcdb sample NEW_DIR [STEPS] | pack INPUT NEW_FILE | unpack INPUT NEW_DIR | inspect INPUT | verify INPUT | export-series INPUT NEW_RFCXS | import-series INPUT NEW_DIR | append INPUT RFCXT NEW_DIR | field INPUT SEQUENCE TENSOR CELL | branch INPUT SEQUENCE RELATION | sample-catalogue INPUT | placement INPUT");
    const std::string command=argv[1];
    if(command=="sample"){require(argc==3||argc==4,"sample argument count");auto db=Database::from_series(sample::history(argc==4?number(argv[3]):9));db.save_directory(argv[2]);inspect(db);return 0;}
    if(command=="sample-action"){require(argc==3||argc==4,"sample-action argument count");auto db=Database::from_series(action_sample_history(argc==4?number(argv[3]):1));db.save_directory(argv[2]);inspect(db);return 0;}
    if(command=="import-series"){require(argc==4,"import-series argument count");auto db=Database::from_series(rfc::storage::read_native_series(argv[2]));db.save_directory(argv[3]);inspect(db);return 0;}
    auto db=Database::open(argv[2]);
    if(command=="execute"){require(argc==6||argc==7,"execute INPUT SEQUENCE GRADE COORDINATES [cpu|required-gpu|allow-cpu-fallback]");
        const auto grade=number(argv[4]);require(grade<=std::numeric_limits<std::uint16_t>::max(),"execution grade overflow");
        rfc::runtime::ExecutionHostPolicy policy;
        if(argc==7){const std::string backend=argv[6];require(backend=="cpu"||backend=="required-gpu"||backend=="allow-cpu-fallback","unknown execution backend option");
            if(backend!="cpu")policy.backend=rfc::runtime::ExecutionBackend::ExactGpu;
            if(backend=="allow-cpu-fallback")policy.fallback=rfc::runtime::BackendFallback::AllowExactCpuFallback;}
        execute_field_diagnostic(db,number(argv[3]),rfc::core::Grade{static_cast<std::uint16_t>(grade)},number(argv[5]),policy);return 0;}
    else if(command=="pack"){require(argc==4,"pack argument count");db.save_file(argv[3]);}
    else if(command=="unpack"){require(argc==4,"unpack argument count");db.save_directory(argv[3]);}
    else if(command=="export-series"){require(argc==4,"export-series argument count");auto b=rfc::storage::encode_native_series(db.series());write_new_file(argv[3],b);}
    else if(command=="append"){require(argc==5,"append argument count");auto history=db.series();auto b=read_file(argv[3]);history.append_stream(b);db=Database::from_series(history);db.save_directory(argv[4]);}
    else if(command=="field"){require(argc==6,"field argument count");auto ep=db.series().at(number(argv[3]));rfc::storage::NativeAccessionDirectory views(ep.endpoint(),ep.source_namespace());
        rfc::storage::NativeAccessionRequest request;request.kind=rfc::storage::NativeAccessionKind::Field;request.subject=number(argv[4]);
        auto v=views.pin(request);auto carrier=v.carrier();
        require(carrier->kind==rfc::storage::TemporalCarrierKind::SourceGrade,"field command requires a source-grade carrier");
        auto cell=rfc::storage::resolve_stable_cell(ep.endpoint().source,{carrier->grade,number(argv[5])});
        require(cell.has_value(),"field cell absent from source");auto value=v.tensor()->state(cell->index);require(value.has_value(),"field cell outside structural support");
        std::cout<<"{\"sequence\":"<<ep.sequence()<<",\"value\":\""<<*value<<"\",\"replayed_segments\":"<<ep.stats().replayed_segments<<"}\n";return 0;}
    else if(command=="branch"){require(argc==5,"branch argument count");auto ep=db.series().at(number(argv[3]));
        rfc::storage::NativeAccessionDirectory views(ep.endpoint(),ep.source_namespace());auto cell=views.cell({rfc::core::Grade{1},number(argv[4])});
        const auto schema=cell.source_schema();require(cell.occurrence_count()<=4096&&schema->aspects().size()<=4096&&mul(cell.occurrence_count(),schema->aspects().size())<=1000000,"branch diagnostic output budget");
        std::ostringstream output;output<<"occurrence,participant,active,coefficient,aspect_bits_in_schema_order\n";
        for(std::size_t i=0;i<cell.occurrence_count();++i){auto leg=cell.occurrence(i);output<<leg.key().key.value<<','<<leg.participant().key<<','<<leg.aspect("exists")<<','<<leg.coefficient()<<',';
            for(std::size_t j=0;j<schema->aspects().size();++j) { output<<leg.aspect(static_cast<rfc::composite::AspectId>(j)); }
            output<<'\n';}
        std::cout<<output.str();return 0;}
    else if(command=="sample-catalogue"){require(argc==3,"catalogue argument count");require(db.manifest().name_space==sample::namespace_key,"not the sample namespace");auto ep=db.series().tail();const auto legs=sample::directory_legs(ep.endpoint());
        for(const auto& leg:legs) { std::cout<<leg.relation<<','<<leg.occurrence<<','<<leg.target<<','<<leg.active<<','<<leg.parent<<','<<leg.child<<'\n'; }
        return 0;}
    else if(command=="placement"){require(argc==3,"placement argument count");auto t=db.placement_tensor();for(std::size_t i=0;i<db.manifest().entries.size();++i) {
        std::cout<<i<<','<<t.state(3*i).value()<<','<<t.state(3*i+1).value()<<','<<t.state(3*i+2).value()<<'\n'; } return 0;}
    else {require((command=="verify"||command=="inspect")&&argc==3,"unknown command or argument count");}
    inspect(db);return 0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
