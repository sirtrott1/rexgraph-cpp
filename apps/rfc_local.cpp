// Local developer-preview CLI. No server, network calls, implicit assertions,
// or general SQL parser. Every write creates a new file exclusively.
#include "rfc/database/interpretation_codec.hpp"
#include "rfc/database/native_query.hpp"
#ifdef RFC_LOCAL_SQLITE
#include "rfc/io/sqlite_interpretation.hpp"
#endif
#include <charconv>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string_view>
#include <system_error>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
using namespace rfc;
using namespace database;
using V=composite::DeclaredValue;
using D=composite::Declaration;
void require(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
std::uint64_t number(std::string_view s){
    std::uint64_t n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    require(!s.empty() && r.ec==std::errc{} && r.ptr==s.data()+s.size(),"expected an unsigned decimal integer");return n;
}
std::vector<std::uint64_t> factor_path(std::string_view s){
    if(s=="-")return {};
    std::vector<std::uint64_t> out;
    while(true){const auto end=s.find('/');auto n=number(s.substr(0,end));require(n!=0,"factor identities cannot be zero");out.push_back(n);require(out.size()<=64,"factor path too deep");if(end==s.npos)break;s.remove_prefix(end+1);}
    return out;
}
struct Json {
    std::string text;
    void add(std::string_view s){require(s.size()<=16u*1024u*1024u-text.size(),"CLI output exceeds 16 MiB; request fewer results");text.append(s);}
    void str(std::string_view s){add("\"");constexpr char hex[]="0123456789abcdef";
        for(const char x:s){const auto c=static_cast<unsigned char>(x);if(c=='"')add("\\\"");else if(c=='\\')add("\\\\");else if(c<32 || c>=127){char b[]={'\\','u','0','0',hex[c>>4],hex[c&15]};add(std::string_view(b,6));}else {const char b=static_cast<char>(c);add(std::string_view(&b,1));}}add("\"");}
    void id(std::uint64_t n){str(std::to_string(n));}
};
void value(Json& j,const V& v,std::size_t& remaining){
    require(remaining!=0,"CLI value-node budget");--remaining;
    const auto list=[&](const auto& xs){j.add("[");bool first=true;for(const auto& x:xs){if(!first)j.add(",");first=false;value(j,x,remaining);}j.add("]");};
    switch(v.data.index()){
        case 0:j.add(std::get<bool>(v.data)?"{\"boolean\":true}":"{\"boolean\":false}");break;
        case 1:j.add("{\"symbol\":");j.id(std::get<V::Symbol>(v.data).key);j.add("}");break;
        case 2:j.add("{\"integer\":");j.str(std::get<numeric::Integer>(v.data).get_str());j.add("}");break;
        case 3:j.add("{\"rational\":");j.str(std::get<numeric::Rational>(v.data).get_str());j.add("}");break;
        case 4:j.add("{\"product\":");list(std::get<V::Product>(v.data).elements);j.add("}");break;
        case 5:{const auto& a=std::get<V::Alternative>(v.data);j.add("{\"alternative\":");j.id(a.key);j.add(",\"value\":");value(j,a.element.at(0),remaining);j.add("}");break;}
        case 6:{const auto& o=std::get<V::Optional>(v.data);j.add("{\"optional\":");if(o.element.empty())j.add("null");else value(j,o.element.at(0),remaining);j.add("}");break;}
        case 7:j.add("{\"repeated\":");list(std::get<V::Repeated>(v.data).elements);j.add("}");break;
        case 8:{const auto& t=std::get<numeric::SparseRationalTensor>(v.data);j.add("{\"tensor\":{\"shape\":[");bool first=true;
            for(auto extent:t.domain().extents()){if(!first)j.add(",");first=false;j.id(extent);}j.add("],\"support\":[");first=true;
            t.support().for_each_active([&](std::size_t index){require(remaining!=0,"CLI support budget");--remaining;if(!first)j.add(",");first=false;j.id(index);});
            j.add("],\"values\":[");first=true;for(const auto& e:t.values().entries()){require(remaining!=0,"CLI tensor budget");--remaining;if(!first)j.add(",");first=false;j.add("[");j.id(e.index);j.add(",");j.str(e.value.get_str());j.add("]");}j.add("]}}");break;}
        default:throw std::invalid_argument("unsupported CLI value");
    }
}
void write_new(const std::filesystem::path& path,std::span<const std::byte> bytes){
#if defined(__unix__) || defined(__APPLE__)
    const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0)throw std::system_error(errno,std::generic_category(),"cannot exclusively create output");
    std::size_t offset=0;int error=0;
    while(offset<bytes.size()){
        const auto size=std::min<std::size_t>(bytes.size()-offset,1u<<20);
        const auto written=::write(fd,bytes.data()+offset,size);
        if(written<0){if(errno==EINTR)continue;error=errno;break;}
        if(!written){error=EIO;break;}offset+=static_cast<std::size_t>(written);
    }
    if(::close(fd)!=0 && !error)error=errno;
    // An incomplete newly created output is left for inspection/removal. Never
    // unlink through a path that another process could have replaced.
    if(error)throw std::system_error(error,std::generic_category(),"incomplete output; input unchanged");
#else
    (void)path;(void)bytes;throw std::runtime_error("this preview needs POSIX exclusive-file creation (Linux/WSL/macOS)");
#endif
}
InterpretedSnapshot sample(bool changed){
    InterpretationSchema schema;schema.source_namespace=1001;schema.binding_namespace=1002;
    const composite::DeclarationId quantity{1003,1,1},optional{1003,2,1},payload{1003,3,1};
    schema.values=composite::DeclarationRegistry({D::rational(quantity),D::optional(optional,quantity),D::product(payload,{{1,optional,0}})});
    schema.subjects={{1,"participants"}};schema.assertions={{10,"transfer",payload,{{11,1,1,4,false,false},{12,1,1,4,false,false}},{{1,"quantity"}}}};
    LogicalSnapshot data;data.revision="sample/0";data.subjects={{1,"alice"},{1,"bob"}};
    const auto row=[](std::string id,V v){return ExternalAssertion{{10,std::move(id)},V::product({std::move(v)}),{{11,ReferenceState::Present,{{"from",{1,"alice"},true}}},{12,ReferenceState::Present,{{"to",{1,"bob"},true}}}}};};
    data.assertions={row("event/1",V::present(V::rational(numeric::Rational{3,2}))),row("event/2",V::present(V::rational(numeric::Rational{3,2})))};
    const auto base=InterpretedSnapshot::interpret(schema,data);
    if(!changed)return base;
    data.revision="sample/1";data.assertions.front().payload=V::product({V::present(V::rational(0))});data.assertions.push_back(row("event/3",V::absent()));return base.replace(data);
}
void inspect(Json& j,const InterpretedSnapshot& source){
    const auto endpoint=source.endpoint();j.add("{\"namespace\":");j.id(source.schema().source_namespace);j.add(",\"revision\":");j.str(source.revision());j.add(",\"sequence\":");j.id(source.sequence());
    j.add(",\"subjects\":");j.id(endpoint->source.grade(core::Grade{0}).cell_count());j.add(",\"retained_occurrences\":");j.id(endpoint->source.grade(core::Grade{1}).incidence_count());
    j.add(",\"assertion_kinds\":[");bool first=true;
    for(const auto& a:source.schema().assertions){if(!first)j.add(",");first=false;j.add("{\"kind\":");j.id(a.key);j.add(",\"label\":");j.str(a.label);j.add(",\"count\":");j.id(source.assertions(a.key).size());
        j.add(",\"fields\":[");bool f=true;for(const auto& field:a.fields){if(!f)j.add(",");f=false;j.add("{\"factor\":");j.id(field.factor);j.add(",\"label\":");j.str(field.label);j.add("}");}j.add("],\"roles\":[");f=true;
        for(const auto& role:a.roles){if(!f)j.add(",");f=false;j.id(role.key);}j.add("]}");}j.add("]}");
}
V literal(const native_query_detail::Scalar& s,std::string_view input){
    require(!input.empty() && input.size()<=4096,"literal must contain 1..4096 characters");
    if(s.kind==composite::DeclarationKind::Boolean){require(input=="true" || input=="false","Boolean literal must be true or false");return V::boolean(input=="true");}
    if(s.kind==composite::DeclarationKind::Finite)return V::symbol(number(input));
    const auto integer=[](std::string_view text){require(!text.empty(),"empty integer literal");auto digits=text;if(digits.front()=='-')digits.remove_prefix(1);require(!digits.empty(),"missing integer digits");for(char c:digits)require(c>='0' && c<='9',"literal requires decimal digits");return numeric::Integer(std::string(text),10);};
    if(s.kind==composite::DeclarationKind::Integer)return V::integer(integer(input));
    const auto slash=input.find('/');auto numerator=integer(input.substr(0,slash));auto denominator=slash==input.npos?numeric::Integer(1):integer(input.substr(slash+1));
    require(denominator!=0,"zero denominator");numeric::Rational q(numerator,denominator);q.canonicalize();return V::rational(std::move(q));
}
constexpr const char* help=R"(rfc_local — local exact-data developer preview (Phase 03F6)
  doctor
  sample OUT.rfcxi [base|changed]
  inspect FILE.rfcxi
  show FILE.rfcxi KIND [LIMIT]              # defaults to 10, maximum 1000
  sum FILE.rfcxi KIND FACTOR/PATH           # '-' selects root
  select FILE.rfcxi KIND PATH OP [LITERAL]  # eq ne lt le gt ge null not-null
  diff BASE.rfcxi TARGET.rfcxi OUT.rfcxiu
  replay BASE.rfcxi UPDATE.rfcxiu OUT.rfcxi
  import-sqlite DB OUT.rfcxi --namespace N --revision REV
      --subject KIND TABLE ID_COLUMN
      --assertion KIND TABLE LINEAGE_COLUMN
      [--reference ASSERTION_KIND ROLE SUBJECT_KIND COLUMN ...]
  refresh-sqlite BASE.rfcxi DB OUT.rfcxi --namespace N --revision REV [same bindings]
SQLITE import/refresh needs RFC_ENABLE_SQLITE_INTERPRETATION=ON at build time.
Payload declarations are discovered; only identities and roles are supplied.
Namespaces N,N+1,N+2 are reserved by this CLI profile. No SQL query parser.
All output files must be new. No server, provider, training or automatic writes.
Each invocation validates/reconstructs its archive; this is not a hot-query daemon.
)";
#ifdef RFC_LOCAL_SQLITE
io::SQLiteInterpretationPlan sql_plan(int argc,char** argv,int begin,std::string& revision){
    io::SQLiteInterpretationPlan p;bool ns=false,rev=false;
    struct Ref {std::uint64_t kind;io::SQLiteReferenceBinding binding;};std::vector<Ref> refs;
    for(int i=begin;i<argc;){const std::string flag=argv[i++];const auto take=[&](){require(i<argc,"missing binding argument");return std::string(argv[i++]);};
        if(flag=="--namespace"){require(!ns,"duplicate namespace");ns=true;const auto n=number(take());require(n && n<=UINT64_MAX-2,"namespace out of range");p.source_namespace=n;p.binding_namespace=n+1;p.value_namespace=n+2;}
        else if(flag=="--revision"){require(!rev,"duplicate revision");rev=true;revision=take();}
        else if(flag=="--subject"){const auto k=number(take());auto t=take();auto c=take();p.subjects.push_back({k,std::move(t),std::move(c)});}
        else if(flag=="--assertion"){const auto k=number(take());auto t=take();auto c=take();p.assertions.push_back({k,std::move(t),std::move(c),{}});}
        else if(flag=="--reference"){const auto k=number(take()),r=number(take()),s=number(take());auto c=take();refs.push_back({k,{r,s,std::move(c)}});}
        else throw std::invalid_argument("unknown SQLite binding option");
    }
    require(ns && rev,"explicit namespace and revision required");
    for(auto& r:refs){const auto it=std::find_if(p.assertions.begin(),p.assertions.end(),[&](const auto& a){return a.kind==r.kind;});require(it!=p.assertions.end(),"reference assertion kind absent");it->references.push_back(std::move(r.binding));}return p;
}
#endif
} // namespace
int main(int argc,char** argv){try{
    if(argc<2 || std::string_view(argv[1])=="--help" || std::string_view(argv[1])=="help"){std::cout<<help;return 0;}
    const std::string command=argv[1];Json j;
    if(command=="doctor"){
        require(argc==2,"doctor takes no arguments");j.add("{\"release\":\"03F6-local-preview\",\"cpp\":20,\"sqlite\":");
#ifdef RFC_LOCAL_SQLITE
        j.add("true,\"sqlite_version\":");j.str(sqlite3_libversion());
#else
        j.add("false");
#endif
        j.add(",\"gmp_version\":");j.str(gmp_version);j.add(",\"network\":false,\"transactions\":false}");
    }else if(command=="sample"){
        require(argc==3 || argc==4,"sample OUT [base|changed]");const auto mode=argc==4?std::string_view(argv[3]):"base";require(mode=="base" || mode=="changed","unknown sample mode");
        const auto snapshot=sample(mode=="changed");inspect(j,snapshot);write_new(argv[2],encode_interpreted_snapshot(snapshot));
    }else if(command=="inspect"){
        require(argc==3,"inspect FILE");inspect(j,read_interpreted_snapshot(argv[2]));
    }else if(command=="show"){
        require(argc==4 || argc==5,"show FILE KIND [LIMIT]");const auto kind=number(argv[3]),limit=argc==5?number(argv[4]):10;require(limit<=1000,"show limit exceeds 1000");const auto snapshot=read_interpreted_snapshot(argv[2]);
        const auto& keys=snapshot.assertions(kind);const auto definition=std::find_if(snapshot.schema().assertions.begin(),snapshot.schema().assertions.end(),[&](const auto& a){return a.key==kind;});
        j.add("{\"total\":");j.id(keys.size());j.add(",\"assertions\":[");std::size_t nodes=100000,emitted=0;
        for(const auto key:keys){if(emitted==limit)break;if(emitted++)j.add(",");j.add("{\"native_key\":");j.id(key);j.add(",\"payload\":");value(j,*snapshot.payload(key).declared_value(),nodes);j.add(",\"roles\":[");bool first=true;
            for(const auto& role:definition->roles){if(!first)j.add(",");first=false;j.add("{\"role\":");j.id(role.key);j.add(",\"state\":");j.id(static_cast<std::uint64_t>(snapshot.reference_state(key,role.key)));j.add(",\"occurrences\":[");bool f=true;
                for(const auto& o:snapshot.participants(key,role.key)){if(!f)j.add(",");f=false;j.add("{\"key\":");j.id(o.key().key.value);j.add(",\"participant\":");j.id(o.participant().key);j.add(",\"active\":");j.add(o.aspect("exists")?"true":"false");j.add("}");}j.add("]}");}j.add("]}");}j.add("]}");
    }else if(command=="sum"){
        require(argc==5,"sum FILE KIND PATH");const auto snapshot=read_interpreted_snapshot(argv[2]);const auto answer=sum_exact(snapshot,number(argv[3]),factor_path(argv[4]));j.add("{\"exact_sum\":");if(answer)j.str(answer->get_str());else j.add("null");j.add("}");
    }else if(command=="select"){
        require(argc==6 || argc==7,"select FILE KIND PATH OP [LITERAL]");const auto snapshot=read_interpreted_snapshot(argv[2]);const auto kind=number(argv[3]);ExactScalarPredicate p;p.factors=factor_path(argv[4]);
        const std::map<std::string,ExactPredicateKind> ops{{"eq",ExactPredicateKind::Equal},{"ne",ExactPredicateKind::NotEqual},{"lt",ExactPredicateKind::Less},{"le",ExactPredicateKind::LessEqual},{"gt",ExactPredicateKind::Greater},{"ge",ExactPredicateKind::GreaterEqual},{"null",ExactPredicateKind::IsNull},{"not-null",ExactPredicateKind::IsNotNull}};
        const auto it=ops.find(argv[5]);require(it!=ops.end(),"unknown predicate");p.operation=it->second;const bool null=p.operation==ExactPredicateKind::IsNull || p.operation==ExactPredicateKind::IsNotNull;require(argc==(null?6:7),"predicate literal mismatch");
        if(!null) { p.literal=literal(native_query_detail::scalar(snapshot.schema(),kind,p.factors),argv[6]); }
        const auto keys=select_exact(snapshot,kind,p);j.add("{\"native_keys\":[");bool first=true;for(auto k:keys){if(!first)j.add(",");first=false;j.id(k);}j.add("]}");
    }else if(command=="diff"){
        require(argc==5,"diff BASE TARGET OUT");const auto base=read_interpreted_snapshot(argv[2]),target=read_interpreted_snapshot(argv[3]);const auto update=base.difference(target);j.add("{\"written\":true}");write_new(argv[4],encode_interpretation_update(update));
    }else if(command=="replay"){
        require(argc==5,"replay BASE UPDATE OUT");const auto base=read_interpreted_snapshot(argv[2]);storage::ReadOnlyMappedFile bytes(argv[3]);const auto target=base.apply(decode_interpretation_update(bytes.bytes()));inspect(j,target);write_new(argv[4],encode_interpreted_snapshot(target));
    }else if(command=="import-sqlite" || command=="refresh-sqlite"){
#ifdef RFC_LOCAL_SQLITE
        const bool refresh=command=="refresh-sqlite";require(argc>=(refresh?6:5),"SQLite command needs paths and binding options");std::string revision;const auto plan=sql_plan(argc,argv,refresh?5:4,revision);
        const auto read=io::read_sqlite_snapshot(argv[refresh?3:2],plan,revision);
        const auto target=refresh
            ? read_interpreted_snapshot(argv[2]).replace(read.schema,read.snapshot)
            : InterpretedSnapshot::interpret(read.schema,read.snapshot);
        inspect(j,target);write_new(argv[refresh?4:3],encode_interpreted_snapshot(target));
#else
        throw std::invalid_argument("SQLite disabled; rebuild with RFC_ENABLE_SQLITE_INTERPRETATION=ON");
#endif
    }else throw std::invalid_argument("unknown command; use --help");
    std::cout<<j.text<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"rfc_local: "<<e.what()<<'\n';return 1;}}
