#include "rfc/storage/prepared_relational_binary.hpp"
#include "rfc/storage/temporal_codec.hpp"
#include <iostream>
#include <random>
#include <thread>
#include <atomic>
using namespace rfc;
using namespace rfc::storage;
using namespace rfc::composite;
using Word=BinaryProgram::word_type;
namespace {
std::size_t checks=0;
void check(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}
template<class E=std::exception,class F>void rejects(F&& f){bool yes=false;try{f();}catch(const E&){yes=true;}check(yes,"missing expected rejection");}
RelationalCodeState fixture(BinaryProgram p,std::vector<std::uint16_t> regs,bool leaf=false){
    RelationalCodeState s;s.namespace_key=501;
    s.types={{1,RelationalCodeTypeKind::Finite,{0,1},{}},
             {2,RelationalCodeTypeKind::Product,{},{{101,1,1,"exists"},{102,1,2,"gate"},{103,1,3,"orientation"}}},
             {3,RelationalCodeTypeKind::Product,{},{}}};
    for(std::size_t i=0;i<regs.size();++i)s.types[2].factors.push_back({201+i,1,static_cast<std::uint16_t>(i),{}});
    RelationalCodeOperation op;op.key=7;op.input=2;op.output=leaf?1:3;op.opcode=RelationalCodeOpcode::Binary;op.binary=std::move(p);op.registers=std::move(regs);
    s.operations.push_back(op);return s;
}
BinaryProgram program(){
    BinaryProgram p(9);
    p.append({BinaryOpcode::LoadAspect,0,0,0,0,2});
    p.append({BinaryOpcode::LoadAspect,1,0,0,0,0});
    p.append({BinaryOpcode::Xor,2,0,1});
    p.append({BinaryOpcode::Not,4,2});
    p.append({BinaryOpcode::LoadAspect,8,0,0,0,1}); // unused load MUST run
    return p;
}
Word scalar(const BinaryProgram& p,const std::vector<machine::PackedBits>& factors,std::size_t slot,std::uint16_t output){
    std::vector<bool> r(p.register_count(),false);
    for(const auto& i:p.instructions())switch(i.opcode){
    case BinaryOpcode::LoadAspect:r[i.dst]=factors[i.aspect].get(slot);break;
    case BinaryOpcode::LoadZero:r[i.dst]=false;break;
    case BinaryOpcode::LoadOne:r[i.dst]=true;break;
    case BinaryOpcode::Not:r[i.dst]=!r[i.a];break;
    case BinaryOpcode::And:r[i.dst]=r[i.a]&&r[i.b];break;
    case BinaryOpcode::Or:r[i.dst]=r[i.a]||r[i.b];break;
    case BinaryOpcode::Xor:r[i.dst]=r[i.a]!=r[i.b];break;
    case BinaryOpcode::AndNot:r[i.dst]=r[i.a]&&!r[i.b];break;
    case BinaryOpcode::Select:r[i.dst]=r[i.a]?r[i.b]:r[i.c];break;
    }
    return r[output];
}
void equivalence(){
    std::mt19937_64 rng(0xE1EC1);BinaryExecutionWorkspace work;
    for(std::size_t trial=0;trial<108;++trial){
        BinaryProgram p(static_cast<std::uint16_t>(1+rng()%31));
        for(std::size_t i=0;i<trial%45;++i){const auto reg=[&]{return static_cast<std::uint16_t>(rng()%p.register_count());};
            p.append({static_cast<BinaryOpcode>(i%9),reg(),reg(),reg(),reg(),static_cast<AspectId>(rng()%3)});}
        const auto r=static_cast<std::uint16_t>(rng()%p.register_count());const std::vector<std::uint16_t> regs{r,0,r};
        const auto s=fixture(p,regs);const PreparedRelationalBinaryOperation prepared(s,7);const RelationalCodeAlgebra reference(s);
        const std::size_t sizes[]={0,1,63,64,65,127,128,129,513};const auto slots=sizes[trial%9];
        std::vector<machine::PackedBits> factors(3,machine::PackedBits(slots));
        for(auto& f:factors)for(std::size_t w=0;w<f.word_count();++w)f.assign_word(w,rng());
        const auto actual=prepared.materialize(2,slots,factors,work);
        for(std::size_t lane=0;lane<slots;++lane){
            RelationalCodeValue::Product input;for(const auto& f:factors)input.push_back(RelationalCodeValue{Word(f.get(lane))});
            const auto expected=reference.apply(7,RelationalCodeValue{input});
            for(std::size_t j=0;j<regs.size();++j){
                check(actual.outputs[j].get(lane)==bool(std::get<Word>(std::get<2>(expected.data)[j].data)),"reference output");
                check(actual.outputs[j].get(lane)==bool(scalar(p,factors,lane,regs[j])),"scalar bit oracle");
            }
        }
        const auto words=machine::PackedBits::words_for_size(slots);
        check(actual.receipt.words==words && actual.receipt.instructions==words*p.instruction_count(),"word accounting");
        check(actual.receipt.output_words==words*regs.size() && actual.receipt.admitted_work==1+words*(p.instruction_count()+regs.size()),"work accounting");
        for(const auto& out:actual.outputs)if(slots%64)check((out.words().back()>>(slots%64))==0,"tail mask");
        const auto growth=work.growth_events();const auto again=prepared.materialize(2,slots,factors,work);
        check(growth==work.growth_events() && again.outputs[0].words()==actual.outputs[0].words(),"warm workspace");
        temporal_codec_detail::Writer before,after;
        temporal_codec_detail::code_operation(before,s.operations.front());temporal_codec_detail::code_operation(after,prepared.operation());
        check(before.bytes()==after.bytes(),"operation wordcode changed");
    }
    auto s=fixture(program(),{4},true);const PreparedRelationalBinaryOperation p(s,7);
    const RelationalCodeValue value{RelationalCodeValue::Product{{Word{1}},{Word{0}},{Word{1}}}};
    const auto a=p.apply(value,work);check(std::get<Word>(a.value.data)==1,"leaf output");
    check(a.receipt.namespace_key==501 && a.receipt.operation==7 && a.receipt.input_type==2 && a.receipt.output_type==1,"typed receipt");
    auto bad=value;std::get<2>(bad.data)[1].data=Word{2};rejects([&]{(void)p.apply(bad,work);});
}
void policies_and_callbacks(){
    const auto original=fixture(program(),{2,4,2,6});auto codes=original;
    PreparedRelationalBinaryOperation p(codes,7);const auto copy=p;
    check(copy.same_owner(p),"copied owner");
    check(!PreparedRelationalBinaryOperation(codes,7).same_owner(p),"content became owner identity");
    codes.operations[0].binary->append({BinaryOpcode::LoadZero,2});
    check(p.operation().binary->instruction_count()==5 && p.input_type().factors[1].key==102 && p.output_type().factors[3].grade==3,"snapshot metadata");
    BinaryExecutionWorkspace w;std::vector<std::pair<AspectId,std::size_t>> trace;
    const auto provider=[&](AspectId a,std::size_t word){trace.emplace_back(a,word);return ~Word{0};};
    auto r=p.materialize(2,3,65,provider,w);
    check(trace==std::vector<std::pair<AspectId,std::size_t>>{{2,0},{0,0},{1,0},{2,1},{0,1},{1,1}},"provider order/unused load");
    check(r.outputs[0].none() && r.outputs[1].count()==65 && r.outputs[2].none() && r.outputs[3].none(),"duplicate/unwritten outputs");
    check(r.receipt.aspect_reads==6 && r.receipt.instructions==10,"one program per word");
    std::size_t read=0;auto counted=[&](auto,auto){++read;return Word{0};};
    RelationalBinaryRequest request;request.max_work=18;rejects<std::length_error>([&]{(void)p.materialize(2,3,65,counted,w,request);});check(read==0,"work preflight");
    request.max_work=19;request.max_output_words=7;rejects<std::length_error>([&]{(void)p.materialize(2,3,65,counted,w,request);});check(read==0,"output preflight");
    const auto streamed=p.for_each_word(2,3,65,counted,[](auto,auto){},w,request);check(streamed.words==2,"streaming output budget incorrectly applied");
    rejects([&]{(void)p.materialize(999,3,0,counted,w);});rejects([&]{(void)p.materialize(2,2,0,counted,w);});
    std::vector<machine::PackedBits> wrong(3,machine::PackedBits(1));rejects([&]{(void)p.materialize(2,0,wrong,w);});
    rejects([&]{(void)p.materialize(2,3,std::numeric_limits<std::size_t>::max(),counted,w);});
    rejects([&]{(void)p.materialize(2,3,65,[](auto a,auto){if(a==1)throw std::runtime_error("unused load");return Word{0};},w);});
    check(p.materialize(2,3,1,counted,w).outputs[1].count()==1,"provider exception poisoned workspace");
    rejects([&]{p.for_each_word(2,3,65,counted,[](auto,auto){throw std::runtime_error("sink");},w);});
    check(p.materialize(2,3,1,counted,w).outputs[1].count()==1,"sink exception poisoned workspace");
    p.for_each_word(2,3,1,[&](auto,auto){
        rejects<std::logic_error>([&]{w.release();});
        rejects<std::logic_error>([&]{p.for_each_word(2,3,0,counted,[](auto,auto){},w);});
        BinaryExecutionWorkspace other;check(copy.materialize(2,3,1,counted,other).outputs[1].get(0),"separate reentrant scratch");return Word{0};
    },[](auto,auto){},w);
    auto changed=fixture(program(),{4},true);p=copy;
    r=p.materialize(2,3,65,[&](auto,auto){p=PreparedRelationalBinaryOperation(changed,7);return Word{0};},w);
    check(r.outputs.size()==4 && p.output_count()==1 && r.receipt.output_type==3,"callback replaced active snapshot");
    // Real concurrent use shares immutable plan only; each thread owns scratch.
    std::atomic<unsigned> successes{0};auto worker=[&]{try{BinaryExecutionWorkspace local;for(int i=0;i<20;++i){
        const auto a=copy.materialize(2,3,129,[](auto,auto){return Word{0};},local);if(a.outputs[1].count()!=129)return;}++successes;}catch(...){}};
    std::thread t1(worker),t2(worker);t1.join();t2.join();check(successes==2,"concurrent immutable plan");
    for(unsigned kind=0;kind<10;++kind){auto bad=original;
        if(kind==0)bad.namespace_key=0;
        if(kind==1)bad.types.push_back(bad.types.front());
        if(kind==2)bad.operations.push_back(bad.operations.front());
        if(kind==3)bad.operations[0].opcode=RelationalCodeOpcode::Identity;
        if(kind==4)bad.operations[0].registers[0]=65535;
        if(kind==5)bad.operations[0].binary->append({BinaryOpcode::LoadAspect,0,0,0,0,3});
        if(kind==6)bad.types[0].atoms={1,0};
        if(kind==7)bad.types[1].factors[0].type=2;
        if(kind==8)bad.operations[0].registers.clear();
        if(kind==9)bad.types[1].factors[1].key=101;
        rejects([&]{PreparedRelationalBinaryOperation rejected(bad,7);});
    }
    for(unsigned kind=0;kind<7;++kind){RelationalBinaryPreparationLimits l;
        if(kind==0)l.max_types=2;
        if(kind==1)l.max_operations=0;
        if(kind==2)l.max_type_items=1;
        if(kind==3)l.max_aspect_name_bytes=0;
        if(kind==4)l.binary.max_instructions=4;
        if(kind==5)l.binary.max_outputs=3;
        if(kind==6)l.binary.max_output_words=0;
        rejects<std::length_error>([&]{const PreparedRelationalBinaryOperation small(original,7,l);(void)small.materialize(2,3,1,counted,w);});
    }
    BinaryProgram sparse(65535);sparse.append({BinaryOpcode::LoadOne,60000});sparse.append({BinaryOpcode::Xor,65534,60000,5});
    const PreparedRelationalBinaryOperation high(fixture(sparse,{65534,12}),7);
    check(high.register_words()==4 && high.materialize(2,3,65,counted,w).outputs[0].count()==65,"sparse register lowering");
    const PreparedRelationalBinaryOperation empty(fixture(BinaryProgram(1),{0}),7);
    check(empty.materialize(2,3,65,counted,w).outputs[0].none(),"empty program initial values");
}
void cancellation(){
    PreparedRelationalBinaryOperation p(fixture(program(),{2,4}),7);BinaryExecutionWorkspace w;
    std::stop_source stop;RelationalBinaryRequest r;r.stop=stop.get_token();stop.request_stop();std::size_t calls=0;
    rejects<RelationalBinaryCancelled>([&]{(void)p.materialize(2,3,65,[&](auto,auto){++calls;return Word{0};},w,r);});
    check(calls==0,"cancel before callbacks");
    stop=std::stop_source{};r.stop=stop.get_token();
    rejects<RelationalBinaryCancelled>([&]{(void)p.materialize(2,3,65,[&](auto,auto){++calls;stop.request_stop();return Word{0};},w,r);});
    check(calls==1,"cancel at load checkpoint");
    BinaryProgram constants(1);constants.append({BinaryOpcode::LoadOne,0});p=PreparedRelationalBinaryOperation(fixture(constants,{0}),7);
    for(auto slots:{65u,1u}){stop=std::stop_source{};r.stop=stop.get_token();std::size_t emitted=0;
        rejects<RelationalBinaryCancelled>([&]{p.for_each_word(2,3,slots,[](auto,auto){return Word{0};},[&](auto,auto){++emitted;stop.request_stop();},w,r);});
        check(emitted==1,"cancel after sink/final word");}
    check(p.materialize(2,3,65,[](auto,auto){return Word{0};},w).outputs[0].count()==65,"cancel released workspace");
    const auto no=p.materialize(2,3,0,[](auto,auto){return Word{0};},w);check(no.receipt.words==0 && no.receipt.instructions==0 && no.receipt.admitted_work==1,"empty domain receipt");
}
}
int main(){try{equivalence();policies_and_callbacks();cancellation();std::cout<<"prepared_relational_binary checks="<<checks<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
