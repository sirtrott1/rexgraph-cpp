#include "rfc/composite/execution.hpp"
#include "rfc/storage/rfcx.hpp"
#include <filesystem>
#include <iostream>
#include <random>
#include <functional>
#include <limits>

namespace {
using namespace rfc;
using namespace composite;
using Word=BinaryProgram::word_type;
std::size_t checks=0;
void check(bool result,const char* message){++checks;if(!result)throw std::runtime_error(message);}
template<class Fn> void rejects(Fn&& fn){bool threw=false;try{fn();}catch(const std::exception&){threw=true;}check(threw,"expected rejection");}
Word scalar(const BinaryProgram& p,std::size_t slot,const AspectPlanes& planes,std::uint16_t output){
    std::vector<bool> r(p.register_count(),false);
    for(const auto& i:p.instructions())switch(i.opcode){
        case BinaryOpcode::LoadAspect:r[i.dst]=planes.get(i.aspect,slot);break;
        case BinaryOpcode::LoadZero:r[i.dst]=false;break;
        case BinaryOpcode::LoadOne:r[i.dst]=true;break;
        case BinaryOpcode::Not:r[i.dst]=!r[i.a];break;
        case BinaryOpcode::And:r[i.dst]=r[i.a] && r[i.b];break;
        case BinaryOpcode::Or:r[i.dst]=r[i.a] || r[i.b];break;
        case BinaryOpcode::Xor:r[i.dst]=r[i.a]!=r[i.b];break;
        case BinaryOpcode::AndNot:r[i.dst]=r[i.a] && !r[i.b];break;
        case BinaryOpcode::Select:r[i.dst]=r[i.a]?r[i.b]:r[i.c];break;
        default:throw std::logic_error("oracle opcode");
    }
    return r[output]?1:0;
}
void compare(const BinaryProgram& p,const std::vector<std::uint16_t>& outputs,const AspectPlanes& planes,
             BinaryExecutionWorkspace& workspace,bool bits){
    BinaryExecutionPlan plan(p,outputs);
    const auto actual=plan.materialize(planes,workspace);
    check(plan.instruction_count()==p.instruction_count(),"removed instructions");
    for(std::size_t i=0;i<outputs.size();++i){
        const auto expected=p.execute(planes,outputs[i]);
        check(actual[i].size()==expected.size() && actual[i].words()==expected.words(),"packed reference mismatch");
        if(bits)for(std::size_t s=0;s<planes.slots();++s)check(actual[i].get(s)==(scalar(p,s,planes,outputs[i])!=0),"Boolean oracle mismatch");
    }
    const auto growth=workspace.growth_events();
    const auto second=plan.materialize(planes,workspace);
    check(growth==workspace.growth_events(),"warm scratch allocated");
    for(std::size_t i=0;i<actual.size();++i)check(actual[i].words()==second[i].words(),"scratch retained prior word state");
}
void randomized(){
    AspectSchema schema;for(std::size_t i=0;i<7;++i)schema.add("a"+std::to_string(i));
    std::mt19937_64 rng(0xF603u);BinaryExecutionWorkspace workspace;
    for(std::size_t trial=0;trial<600;++trial){
        const std::size_t sizes[]={0,1,2,63,64,65,127,128,129,131,257,513};
        AspectPlanes planes(schema,sizes[trial%12]);
        for(std::size_t a=0;a<7;++a)for(std::size_t w=0;w<planes.plane(0).word_count();++w)planes.plane(static_cast<AspectId>(a)).assign_word(w,rng());
        const auto registers=static_cast<std::uint16_t>(1+rng()%31);
        BinaryProgram p(registers);
        for(std::size_t n=0;n<trial%91;++n){
            const auto reg=[&]{return static_cast<std::uint16_t>(rng()%registers);};
            p.append({static_cast<BinaryOpcode>(rng()%9),reg(),reg(),reg(),reg(),static_cast<AspectId>(rng()%7)});
        }
        const auto r=static_cast<std::uint16_t>(rng()%registers);
        compare(p,{r,0,static_cast<std::uint16_t>(registers-1),r},planes,workspace,trial<120);
    }
}
void exhaustive_gates(){
    AspectSchema schema;for(std::size_t i=0;i<3;++i)schema.add("a"+std::to_string(i));
    AspectPlanes planes(schema,129);
    for(std::size_t s=0;s<129;++s)for(std::size_t a=0;a<3;++a)planes.set(static_cast<AspectId>(a),s,((s>>a)&1)!=0);
    BinaryExecutionWorkspace workspace;
    for(unsigned op=0;op<9;++op)for(std::uint16_t dst=0;dst<5;++dst){
        BinaryProgram p(5);for(std::uint16_t a=0;a<3;++a)p.append({BinaryOpcode::LoadAspect,a,0,0,0,a});
        p.append({static_cast<BinaryOpcode>(op),dst,0,1,2,1});compare(p,{0,1,2,3,4},planes,workspace,true);
    }
}
void boundaries(){
    BinaryExecutionWorkspace workspace;
    BinaryProgram sparse(65535);sparse.append({BinaryOpcode::LoadOne,60000});sparse.append({BinaryOpcode::Xor,65534,60000,5});
    BinaryExecutionPlan compact(sparse,{65534,12});
    check(compact.register_words()==4 && compact.zeroed_register_words()==2,"sparse register lowering");
    auto out=compact.materialize(129,[](auto,auto){return Word{0};},workspace);
    check(out[0].count()==129 && out[1].none(),"initial zero register or tail");
    check(workspace.growth_events()==2,"workspace growth events");
    const auto capacity=workspace.capacity_words();
    for(int i=0;i<40;++i){out=compact.materialize(129,[](auto,auto){return Word{0};},workspace);check(out[0].count()==129,"repeat lowering");}
    check(capacity==workspace.capacity_words() && workspace.growth_events()==2,"warm scratch capacity");
    const auto copy=compact;sparse.append({BinaryOpcode::LoadZero,65534});
    check(copy.materialize(65,[](auto,auto){return Word{0};},workspace)[0].count()==65,"plan aliases mutable program");
    rejects([&]{BinaryExecutionPlan invalid(sparse,{});});
    rejects([&]{BinaryExecutionPlan invalid(sparse,{65535});});
    rejects([&]{BinaryExecutionLimits l;l.max_instructions=1;BinaryExecutionPlan invalid(sparse,{0},l);});
    rejects([&]{BinaryExecutionLimits l;l.max_outputs=1;BinaryExecutionPlan invalid(sparse,{0,1},l);});
    BinaryExecutionLimits limits;limits.max_output_words=3;const BinaryExecutionPlan budget(sparse,{0,1},limits);
    std::size_t reads=0;rejects([&]{(void)budget.materialize(129,[&](auto,auto){++reads;return Word{0};},workspace);});check(reads==0,"allocated/executed before output preflight");
    rejects([&]{(void)budget.materialize(std::numeric_limits<std::size_t>::max(),[](auto,auto){return Word{0};},workspace);});
    AspectSchema schema;schema.add("a");AspectPlanes empty(schema,0);
    BinaryProgram bad(1);bad.append({BinaryOpcode::LoadAspect,0,0,0,0,1});
    rejects([&]{(void)BinaryExecutionPlan(bad,{0}).materialize(empty,workspace);});
    workspace.release();check(workspace.capacity_words()==0,"workspace release");
    BinaryProgram wide(65535);for(std::size_t r=0;r<65535;++r)wide.append({BinaryOpcode::LoadOne,static_cast<std::uint16_t>(r)});
    BinaryExecutionPlan all(wide,{65534});check(all.register_words()==65535,"full uint16 register range");
    check(all.materialize(1,[](auto,auto){return Word{0};},workspace)[0].count()==1,"full register execution");
}
void callbacks(){
    BinaryProgram p(4);p.append({BinaryOpcode::LoadAspect,0,0,0,0,2});p.append({BinaryOpcode::LoadAspect,1,0,0,0,0});p.append({BinaryOpcode::Xor,2,0,1});p.append({BinaryOpcode::LoadAspect,3,0,0,0,1});
    BinaryExecutionPlan plan(p,{2,2});BinaryExecutionWorkspace workspace;
    std::vector<std::pair<AspectId,std::size_t>> trace;
    const auto provider=[&](AspectId a,std::size_t w){trace.emplace_back(a,w);return Word(a)+w;};
    const auto out=plan.materialize(65,provider,workspace);
    check(trace==std::vector<std::pair<AspectId,std::size_t>>{{2,0},{0,0},{1,0},{2,1},{0,1},{1,1}},"loads reordered/repeated/removed");
    check(out[0].words()==out[1].words(),"duplicate outputs");
    rejects([&]{(void)plan.materialize(65,[](AspectId a,auto){if(a==1)throw std::runtime_error("unused direction");return Word{0};},workspace);});
    check(plan.materialize(65,[](auto,auto){return Word{0};},workspace)[0].none(),"workspace after provider exception");
    rejects([&]{plan.for_each_word(65,provider,[](auto,auto){throw std::runtime_error("sink");},workspace);});
    std::size_t reentry=0;
    plan.for_each_word(65,[&](auto,auto){
        rejects([&]{workspace.release();});
        rejects([&]{(void)plan.materialize(0,[](auto,auto){return Word{0};},workspace);});
        ++reentry;return Word{1};},[](auto,auto){},workspace);
    check(reentry==6,"reentry guard");
    BinaryProgram zero(1);zero.append({BinaryOpcode::LoadZero,0});
    BinaryProgram one(1);one.append({BinaryOpcode::LoadOne,0});
    BinaryExecutionPlan replace(one,{0});
    auto pinned=replace.materialize(65,[&](auto,auto){return Word{0};},workspace);
    check(pinned[0].count()==65,"initial pin");
    BinaryProgram load(1);load.append({BinaryOpcode::LoadAspect,0});replace=BinaryExecutionPlan(load,{0});
    pinned=replace.materialize(65,[&](auto,auto){replace=BinaryExecutionPlan(zero,{0});return ~Word{0};},workspace);
    check(pinned[0].count()==65 && replace.materialize(65,[](auto,auto){return Word{0};},workspace)[0].none(),"plan callback snapshot");
    auto moved=std::move(replace);rejects([&]{(void)replace.output_count();});check(moved.output_count()==1,"plan move");
}
void mapped(const std::filesystem::path& dir){
    core::RelationSignature signature;
    const auto type=signature.add_type("datum",core::BoundaryPolicy::DeclaredExact,1,1);
    core::RelationalComplexBuilder builder(signature);
    builder.declare_grade(core::Grade{2});
    for(std::uint64_t i=1;i<=131;++i)builder.add_grade_one(core::RelationKey{i},type,{{core::IdentityKey{1},true,i%3==0,numeric::Rational{1},{}}});
    const auto source=builder.freeze();storage::write_rfcx(source,dir/"execution.rfcx");storage::MappedRfcx mapped(dir/"execution.rfcx");
    const auto program=compile_active_orientation_query(source.signature().existence_aspect(),source.signature().orientation_aspect(),true);
    check(mapped.binary_count(core::Grade{1},program,2)==43,"mapped count");
    check(mapped.binary_count(core::Grade{2},program,2)==0,"empty mapped count");
    BinaryExecutionPlan prepared(program,{2});BinaryExecutionWorkspace workspace;
    check(mapped.binary_count(core::Grade{1},prepared,workspace)==43,"prepared mapped count");
    const auto grows=workspace.growth_events();
    for(std::size_t i=0;i<1000;++i)check(mapped.binary_count(core::Grade{1},prepared,workspace)==43,"warm mapped count");
    check(workspace.growth_events()==grows,"mapped count scratch grew");
    BinaryExecutionPlan many(program,{0,1,2,2});
    const auto counts=mapped.binary_counts(core::Grade{1},many,workspace);
    check(counts==std::vector<std::uint64_t>{131,43,43,43},"mapped one-pass outputs");
    rejects([&]{(void)mapped.binary_count(core::Grade{1},many,workspace);});
    BinaryProgram bad(1);bad.append({BinaryOpcode::LoadAspect,0,0,0,0,255});
    rejects([&]{(void)mapped.binary_count(core::Grade{2},bad,0);});
}
} // namespace
int main(int argc,char** argv){try{
    const auto dir=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"rfc-execution-tests";
    std::filesystem::create_directories(dir);
    randomized();exhaustive_gates();boundaries();callbacks();mapped(dir);
    std::cout<<"binary execution checks="<<checks<<" passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"binary execution failure: "<<e.what()<<'\n';return 1;}}
