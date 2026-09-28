#include "rfc/io/sqlite_interpretation.hpp"
#include "rfc/database/interpretation_codec.hpp"
#include "rfc/database/native_query.hpp"
#include <iostream>
#include <sstream>

namespace {
using namespace rfc;
using namespace database;
using V=composite::DeclaredValue;
std::size_t checks{};
void check(bool x,const char* message){++checks;if(!x)throw std::runtime_error(message);}
template<class F>void rejects(F&& f,const char* message){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,message);}
struct Database {
    sqlite3* db{};
    explicit Database(const std::filesystem::path& path){if(sqlite3_open(path.string().c_str(),&db)!=SQLITE_OK){if(db)sqlite3_close(db);db=nullptr;throw std::runtime_error("SQLite fixture open");}}
    ~Database(){if(db)sqlite3_close(db);}
    Database(const Database&)=delete;Database& operator=(const Database&)=delete;
    void exec(const std::string& sql){char* error=nullptr;const auto rc=sqlite3_exec(db,sql.c_str(),nullptr,nullptr,&error);if(rc!=SQLITE_OK){std::string message=error?error:"SQLite failure";sqlite3_free(error);throw std::runtime_error(message);}}
    std::optional<std::string> scalar(const std::string& sql){io::sqlite_interpretation_detail::Statement s(db,sql);check(s.next(),"oracle scalar row absent");if(sqlite3_column_type(s.get(),0)==SQLITE_NULL)return {};return io::sqlite_interpretation_detail::octets(s.get(),0,true,4096);}
};
io::SQLiteInterpretationPlan plan(){return {7001,7002,7003,1,{{1,"people","id"}},{{10,"events","event_id",{{11,1,"buyer"},{12,1,"seller"}}}}};}
std::uint64_t field(const InterpretedSnapshot& s,const std::string& label){for(const auto& f:s.schema().assertions.at(0).fields)if(f.label==label)return f.factor;throw std::invalid_argument("absent fixture factor");}
std::string hex(std::string_view bytes){constexpr char digits[]="0123456789abcdef";std::string out;for(const char raw:bytes){const auto b=static_cast<unsigned char>(raw);out+=digits[b>>4];out+=digits[b&15];}return out;}
std::string bytes_value(const V& v){const V* value=&v;if(const auto* o=std::get_if<V::Optional>(&v.data)){if(o->element.empty())return "null";value=&o->element.at(0);}std::string bytes;for(const auto& x:std::get<V::Repeated>(value->data).elements)bytes+=static_cast<char>(std::get<V::Symbol>(x.data).key);return hex(bytes);}
std::string integer_value(const V& v){const V* value=&v;if(const auto* o=std::get_if<V::Optional>(&v.data)){if(o->element.empty())return "null";value=&o->element.at(0);}return std::get<numeric::Integer>(value->data).get_str();}
std::string native_summary(const InterpretedSnapshot& s){
    const auto amount=field(s,"amount");std::ostringstream out;
    out<<"{\"rows\":"<<s.assertions(10).size()<<",\"positive\":"<<select_exact(s,10,{{amount},ExactPredicateKind::Greater,V::integer(0)}).size()
       <<",\"zero\":"<<select_exact(s,10,{{amount},ExactPredicateKind::Equal,V::integer(0)}).size()
       <<",\"nulls\":"<<select_exact(s,10,{{amount},ExactPredicateKind::IsNull,{}}).size();
    const auto sum=sum_exact(s,10,{amount});out<<",\"sum\":"<<(sum?sum->get_str():"null");
    std::size_t buyer_null=0,same_role=0,legs=0;for(const auto key:s.assertions(10)){buyer_null+=s.reference_state(key,11)==ReferenceState::Null;const auto a=s.participants(key,11),b=s.participants(key,12);legs+=a.size()+b.size();same_role+=!a.empty()&&!b.empty()&&a[0].participant()==b[0].participant();}
    out<<",\"buyer_null\":"<<buyer_null<<",\"same_role\":"<<same_role<<",\"legs\":"<<legs<<",\"values\":[";
    const auto exported=s.export_snapshot();bool first=true;for(const auto& a:exported.assertions){if(!first)out<<',';first=false;const auto& parts=std::get<V::Product>(a.payload.data).elements;
        out<<"[\""<<hex(a.origin.token)<<"\","<<integer_value(parts.at(static_cast<std::size_t>(amount-1)))<<",\""<<bytes_value(parts.at(static_cast<std::size_t>(field(s,"note")-1)))<<"\",\""<<bytes_value(parts.at(static_cast<std::size_t>(field(s,"raw")-1)))<<"\"]";}
    out<<"]}";return out.str();
}
void schema(Database& d){d.exec("CREATE TABLE people(id INTEGER PRIMARY KEY); CREATE TABLE events(event_id INTEGER NOT NULL,buyer INTEGER,seller INTEGER,amount INTEGER,note TEXT,raw BLOB);");}
void baseline(const std::filesystem::path& out){
    const auto path=out/"external.sqlite";std::optional<std::string> oracle_sum,oracle_positive,oracle_null;
    {Database d(path);schema(d);d.exec("INSERT INTO people VALUES(1),(2),(3); INSERT INTO events VALUES(1,1,1,5,'same',x'00ff'),(2,1,1,5,'same',x'00ff'),(3,NULL,2,NULL,NULL,NULL),(4,2,NULL,0,'',x''),(5,3,2,-9,CAST(x'610062' AS TEXT),x'001000');");
        oracle_sum=d.scalar("SELECT sum(amount) FROM events");oracle_positive=d.scalar("SELECT count(*) FROM events WHERE amount>0");oracle_null=d.scalar("SELECT count(*) FROM events WHERE amount IS NULL");}
    const auto before=std::filesystem::file_size(path);auto read=io::read_sqlite_snapshot(path,plan(),"sql/0");auto s=InterpretedSnapshot::interpret(read.schema,read.snapshot);
    check(std::filesystem::file_size(path)==before,"read-only import modified source size");
    // The SQLite connection is closed: rename/delete the input before native reads.
    const auto closed=out/"closed.sqlite";std::filesystem::rename(path,closed);
    const auto amount=field(s,"amount");check(sum_exact(s,10,{amount})->get_str()==*oracle_sum,"native SUM differs from SQL");
    check(std::to_string(select_exact(s,10,{{amount},ExactPredicateKind::Greater,V::integer(0)}).size())==*oracle_positive,"native positive filter differs from SQL");
    check(std::to_string(select_exact(s,10,{{amount},ExactPredicateKind::IsNull,{}}).size())==*oracle_null,"native null filter differs from SQL");
    check(s.assertions(10).size()==5,"duplicate records collapsed");check(s.participants(1,11)[0].participant()==s.participants(1,12)[0].participant(),"shared reference identity lost");
    check(s.participants(1,11)[0].key()!=s.participants(1,12)[0].key(),"roles merged on shared participant");
    check(s.schema().assertions[0].fields.size()==3,"payload columns were not discovered");
    check(bytes_value(*s.payload(5,{field(s,"note")}).declared_value())=="610062","embedded text NUL lost");
    check(bytes_value(*s.payload(4,{field(s,"raw")}).declared_value()).empty(),"empty BLOB became null");
    check(bytes_value(*s.payload(3,{field(s,"raw")}).declared_value())=="null","null BLOB became empty");
    const auto image=encode_interpreted_snapshot(s);interpretation_codec_detail::write(out/"sqlite_base.rfcxi",image);
    check(native_summary(decode_interpreted_snapshot(image))==native_summary(s),"disk-only SQL interpretation differs");
    check(encode_interpreted_snapshot(decode_interpreted_snapshot(image))==image,"SQLite native bytes not canonical");
    {Database d(closed);d.exec("DELETE FROM events WHERE event_id=2; UPDATE events SET amount=0 WHERE event_id=1; INSERT INTO people VALUES(0);INSERT INTO events VALUES(6,0,1,17,'new',x'ff');");}
    const auto read2=io::read_sqlite_snapshot(closed,plan(),"sql/1");const auto target=s.replace(read2.schema,read2.snapshot);const auto delta=encode_interpretation_update(s.difference(target));
    interpretation_codec_detail::write(out/"sqlite_update.rfcxiu",delta);check(encode_interpreted_snapshot(s.apply(decode_interpretation_update(delta)))==encode_interpreted_snapshot(target),"SQLite temporal replay differs");
    interpretation_codec_detail::write(out/"sqlite_target.rfcxi",encode_interpreted_snapshot(target));
    check(target.lineage().assertions.size()==6 && target.assertions(10).size()==5,"producer tombstone lost");
    {Database d(closed);d.exec("ALTER TABLE events ADD COLUMN extra INTEGER;");}const auto drift=io::read_sqlite_snapshot(closed,plan(),"sql/2");rejects([&]{s.replace(drift.schema,drift.snapshot);},"DDL drift silently changed interpretation");
    const auto summary=native_summary(s);std::ofstream summary_out(out/"sql_summary.json");summary_out<<summary<<'\n';std::filesystem::remove(closed);
    check(native_summary(s)==summary,"native operations depend on external connection");
}
void invalid(const std::filesystem::path& out){
    std::size_t serial=0;const auto bad=[&](const std::string& sql,const char* message){const auto path=out/("invalid"+std::to_string(serial++)+".sqlite");{Database d(path);d.exec(sql);}rejects([&]{io::interpret_sqlite_snapshot(path,plan(),"bad");},message);std::filesystem::remove(path);};
    const std::string people="CREATE TABLE people(id INTEGER); INSERT INTO people VALUES(1);";
    bad(people+"CREATE TABLE events(event_id INTEGER,buyer INTEGER,seller INTEGER,amount REAL,note TEXT,raw BLOB);","floating type admitted");
    bad(people+"CREATE TABLE events(event_id INTEGER,buyer INTEGER,seller INTEGER,amount INTEGER,note TEXT,raw BLOB);INSERT INTO events VALUES(1,1,1,'not integer','',x'');","runtime affinity coercion admitted");
    bad(people+"CREATE TABLE events(event_id INTEGER,buyer INTEGER,seller INTEGER,amount INTEGER,note TEXT,raw BLOB);INSERT INTO events VALUES(1,99,1,2,'',x'');","dangling reference accepted");
    bad(people+"CREATE TABLE events(event_id INTEGER,buyer INTEGER,seller INTEGER,amount INTEGER,note TEXT,raw BLOB);INSERT INTO events VALUES(1,1,1,2,'',x''),(1,1,1,2,'',x'');","unkeyed duplicate lineage accepted");
    bad(people+"CREATE TABLE events(event_id INTEGER,buyer INTEGER,seller INTEGER,amount INTEGER,note TEXT,raw BLOB);INSERT INTO events VALUES(NULL,1,1,2,'',x'');","NULL producer lineage accepted");
    bad(people+"CREATE VIEW events AS SELECT 1 event_id,1 buyer,1 seller,1 amount,'' note,x'' raw;","view mistaken for snapshot table");
    bad(people+"CREATE TABLE events(event_id INTEGER,buyer INTEGER,seller INTEGER,amount INTEGER GENERATED ALWAYS AS (buyer+seller),note TEXT,raw BLOB);","generated column silently accepted");
    bad(people+"CREATE TABLE events(event_id INTEGER,buyer TEXT,seller INTEGER,amount INTEGER,note TEXT,raw BLOB);","mismatched identity domains admitted");
    const auto path=out/"limits.sqlite";{Database d(path);schema(d);d.exec("INSERT INTO people VALUES(1);INSERT INTO events VALUES(1,1,1,1,'abc',x'0011');");}
    auto p=plan();p.max_vm_steps=1;rejects([&]{io::interpret_sqlite_snapshot(path,p,"limit");},"VM budget not enforced");
    p=plan();p.max_value_octets=1;rejects([&]{io::interpret_sqlite_snapshot(path,p,"limit");},"byte payload bound ignored");
    p=plan();p.assertions[0].lineage_column="event_id\";DROP TABLE people;--";rejects([&]{io::interpret_sqlite_snapshot(path,p,"limit");},"identifier injection accepted");
    check(io::interpret_sqlite_snapshot(path,plan(),"good").assertions(10).size()==1,"injection attempt changed source");
    InterpretationLimits l;l.max_records=1;rejects([&]{io::interpret_sqlite_snapshot(path,plan(),"limit",l);},"record limit ignored");
    l={};l.max_metadata_bytes=1;rejects([&]{io::interpret_sqlite_snapshot(path,plan(),"limit",l);},"metadata limit ignored");
    p=plan();p.subjects[0].identity_column="rowid";rejects([&]{io::interpret_sqlite_snapshot(path,p,"limit");},"implicit rowid used as producer identity");
    std::filesystem::remove(path);rejects([&]{io::interpret_sqlite_snapshot(path,plan(),"missing");},"read-only adapter created missing file");check(!std::filesystem::exists(path),"missing database created");
}
void exact_queries(){
    InterpretationSchema s;s.source_namespace=8101;s.binding_namespace=8102;const auto id=[](std::uint64_t k){return composite::DeclarationId{8103,k,1};};using D=composite::Declaration;
    s.values=composite::DeclarationRegistry({D::integer(id(1)),D::optional(id(2),id(1)),D::boolean(id(3)),D::finite(id(4),{7,9}),D::product(id(5),{{1,id(2),0},{2,id(3),0},{3,id(4),0}})});s.assertions={{10,"scalars",id(5),{}, {}}};
    LogicalSnapshot input;input.revision="scalars";const numeric::Integer huge("1234567890123456789012345678901234567890");
    for(unsigned i=0;i<8;++i)input.assertions.push_back({{10,std::to_string(i)},V::product({i<2?V::absent():V::present(V::integer(i<4?huge:numeric::Integer{-2})),V::boolean(i%2!=0),V::symbol(i<4?7:9)}),{}});
    const auto native=InterpretedSnapshot::interpret(s,input);
    check(*sum_exact(native,10,{1})==numeric::Rational(huge*2-8),"sum overflowed machine integer");
    check(select_exact(native,10,{{1},ExactPredicateKind::IsNull,{}}).size()==2,"optional scalar null selection");
    check(select_exact(native,10,{{1},ExactPredicateKind::Greater,V::integer(0)}).size()==2,"unknown became false value instead of excluded comparison");
    check(select_exact(native,10,{{2},ExactPredicateKind::Equal,V::boolean(true)}).size()==4,"Boolean predicate");
    check(select_exact(native,10,{{3},ExactPredicateKind::NotEqual,V::symbol(7)}).size()==4,"finite predicate");
    rejects([&]{select_exact(native,10,{{3},ExactPredicateKind::Less,V::symbol(9)});},"finite states given implicit numeric ordering");
    rejects([&]{select_exact(native,10,{{1},ExactPredicateKind::Equal,V::rational(1)});},"numeric domains silently coerced");
    rejects([&]{select_exact(native,10,{{1},static_cast<ExactPredicateKind>(255),{}});},"unknown predicate no-op");
    rejects([&]{sum_exact(native,10,{1},7);},"query work budget ignored");
    input.assertions.clear();const auto empty=InterpretedSnapshot::interpret(s,input);check(!sum_exact(empty,10,{1}),"empty sum fabricated zero");
    rejects([&]{select_exact(empty,10,{{3},ExactPredicateKind::Less,V::symbol(7)});},"empty source skipped query typing");
}
}
int main(int argc,char** argv){try{
    if(argc==3 && std::string(argv[1])=="--oracle"){const auto native=io::interpret_sqlite_snapshot(argv[2],plan(),"oracle");std::cout<<native_summary(native)<<'\n';return 0;}
    const auto out=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"rfc-f5-sqlite-tests";
    if(std::filesystem::exists(out)){if(argc>1)throw std::invalid_argument("fresh output directory required");std::filesystem::remove_all(out);}std::filesystem::create_directories(out);
    baseline(out);invalid(out);exact_queries();std::cout<<checks<<" SQLite interpretation/query checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
