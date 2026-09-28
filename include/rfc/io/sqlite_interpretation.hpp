#pragma once
// Optional leaf adapter. No SQL/client dependency enters native mathematics.
#include "rfc/database/native_interpretation.hpp"
#include <sqlite3.h>
#include <cctype>

namespace rfc::io {
struct SQLiteSubjectBinding {std::uint64_t kind{};std::string table,identity_column;};
struct SQLiteReferenceBinding {std::uint64_t role{},participant_kind{};std::string column;};
struct SQLiteAssertionBinding {std::uint64_t kind{};std::string table,lineage_column;std::vector<SQLiteReferenceBinding> references;};
struct SQLiteInterpretationPlan {
    std::uint64_t source_namespace{},binding_namespace{},value_namespace{},version{1};
    std::vector<SQLiteSubjectBinding> subjects;
    std::vector<SQLiteAssertionBinding> assertions;
    std::size_t max_value_octets{65536}; // bound of the admitted repeated-byte constructor
    std::size_t max_vm_steps{10000000}; // resource policy, not a semantic type
};
struct SQLiteLogicalRead {database::InterpretationSchema schema;database::LogicalSnapshot snapshot;};
namespace sqlite_interpretation_detail {
using database::interpretation_detail::require;using database::interpretation_detail::bound;using database::interpretation_detail::add;
using V=composite::DeclaredValue;
enum class ScalarKind {Integer,Text,Blob};
inline std::string encoding(ScalarKind k){return k==ScalarKind::Integer?"INTEGER":k==ScalarKind::Text?"TEXT":"BLOB";}
struct Column {std::string name;ScalarKind kind;bool nullable;};
inline std::string quote_identifier(const std::string& name){
    require(!name.empty() && name.find('\0')==std::string::npos,"invalid SQLite identifier");
    std::string out="\"";for(char c:name){if(c=='\"')out+='\"';out+=c;}return out+'\"';
}
inline std::string uppercase(std::string s){
    for(auto& c:s)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==std::string::npos?std::string{}:s.substr(a,b-a+1);
}
inline ScalarKind scalar_kind(std::string name){
    name=uppercase(std::move(name));if(name=="INTEGER")return ScalarKind::Integer;
    if(name=="TEXT")return ScalarKind::Text;
    if(name=="BLOB")return ScalarKind::Blob;
    throw std::invalid_argument("SQLite profile admits only declared INTEGER/TEXT/BLOB; no affinity/floating coercion");
}
class Connection {
public:
    Connection(const std::filesystem::path& path,std::size_t steps):remaining_(steps){
        require(steps>0,"SQLite read work budget required");const auto filename=path.string();require(filename.find('\0')==std::string::npos,"SQLite filename NUL");
        const int rc=sqlite3_open_v2(filename.c_str(),&db_,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX,nullptr);
        if(rc!=SQLITE_OK){const auto message=db_?std::string(sqlite3_errmsg(db_)):"SQLite allocation failed";if(db_)sqlite3_close(db_);db_=nullptr;throw std::runtime_error(message);}
        sqlite3_progress_handler(db_,1,[](void* p){auto& n=static_cast<Connection*>(p)->remaining_;if(!n)return 1;--n;return 0;},this);
        try{exec("PRAGMA query_only=ON");exec("BEGIN");}catch(...){sqlite3_close(db_);db_=nullptr;throw;}
    }
    ~Connection(){if(db_)sqlite3_close(db_);}
    Connection(const Connection&)=delete;Connection& operator=(const Connection&)=delete;
    sqlite3* get()const{return db_;}
    void exec(const char* sql){char* error=nullptr;const auto rc=sqlite3_exec(db_,sql,nullptr,nullptr,&error);if(rc!=SQLITE_OK){std::string message=error?error:sqlite3_errmsg(db_);sqlite3_free(error);throw std::runtime_error(message);}}
private:sqlite3* db_{};std::size_t remaining_;
};
class Statement {
public:
    Statement(sqlite3* db,const std::string& sql):db_(db){
        bound(sql.size(),static_cast<std::size_t>(std::numeric_limits<int>::max()),"SQLite query length");
        if(sqlite3_prepare_v2(db,sql.c_str(),static_cast<int>(sql.size()),&statement_,nullptr)!=SQLITE_OK){if(statement_)sqlite3_finalize(statement_);throw std::runtime_error(sqlite3_errmsg(db));}}
    ~Statement(){if(statement_)sqlite3_finalize(statement_);}
    Statement(const Statement&)=delete;Statement& operator=(const Statement&)=delete;
    bool next(){const auto rc=sqlite3_step(statement_);if(rc==SQLITE_ROW)return true;if(rc==SQLITE_DONE)return false;throw std::runtime_error(sqlite3_errmsg(db_));}
    sqlite3_stmt* get()const{return statement_;}
    void bind_text(int index,const std::string& text){bound(text.size(),static_cast<std::size_t>(std::numeric_limits<int>::max()),"SQLite parameter size");if(sqlite3_bind_text(statement_,index,text.data(),static_cast<int>(text.size()),SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db_));}
private:sqlite3* db_;sqlite3_stmt* statement_{};
};
inline std::string octets(sqlite3_stmt* s,int index,bool text,std::size_t cap){
    // Read storage class before conversion; pointer before count; copy before
    // stepping. Empty BLOB may have a null pointer. Embedded NUL is retained.
    const void* p=text?static_cast<const void*>(sqlite3_column_text(s,index)):sqlite3_column_blob(s,index);
    if(sqlite3_errcode(sqlite3_db_handle(s))==SQLITE_NOMEM)throw std::bad_alloc();
    const int n=sqlite3_column_bytes(s,index);require(n>=0,"negative SQLite value size");bound(static_cast<std::size_t>(n),cap,"SQLite value bytes");
    require(p || n==0,"SQLite value buffer");return n?std::string(static_cast<const char*>(p),static_cast<std::size_t>(n)):std::string{};
}
inline std::string identity(sqlite3_stmt* s,int index,ScalarKind kind,std::size_t cap){
    const auto type=sqlite3_column_type(s,index);
    if(kind==ScalarKind::Integer && type==SQLITE_INTEGER)return "I"+std::to_string(sqlite3_column_int64(s,index));
    if(kind==ScalarKind::Text && type==SQLITE_TEXT)return "T"+octets(s,index,true,cap);
    if(kind==ScalarKind::Blob && type==SQLITE_BLOB)return "B"+octets(s,index,false,cap);
    throw std::invalid_argument("SQLite identity/reference must be nonnull and match declared storage class");
}
inline std::vector<Column> columns(Connection& db,const std::string& table,const database::InterpretationLimits& limits){
    {Statement check(db.get(),"SELECT type,sql FROM sqlite_schema WHERE name=?1");check.bind_text(1,table);require(check.next(),"SQLite table absent");
        const auto type=octets(check.get(),0,true,limits.max_origin_bytes);const auto sql=uppercase(octets(check.get(),1,true,limits.max_metadata_bytes));
        require(type=="table" && sql.starts_with("CREATE TABLE"),"ordinary SQLite tables required; views/virtual tables unsupported");require(!check.next(),"ambiguous SQLite object");}
    Statement query(db.get(),"PRAGMA table_xinfo("+quote_identifier(table)+")");std::vector<Column> out;
    while(query.next()){
        bound(out.size()+1,limits.max_records,"SQLite column budget");require(sqlite3_column_count(query.get())==7,"unsupported SQLite metadata");
        require(sqlite3_column_int(query.get(),6)==0,"generated/hidden SQLite columns unsupported");
        out.push_back({octets(query.get(),1,true,limits.max_origin_bytes),scalar_kind(octets(query.get(),2,true,limits.max_origin_bytes)),sqlite3_column_int(query.get(),3)==0});}
    require(!out.empty(),"SQLite table has no columns");std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.name<b.name;});return out;
}
inline const Column& column(const std::vector<Column>& columns,const std::string& name){
    const auto it=std::find_if(columns.begin(),columns.end(),[&](const auto& c){return c.name==name;});require(it!=columns.end(),"SQLite binding names absent column");return *it;
}
inline V value(sqlite3_stmt* s,int index,const Column& c,std::size_t cap){
    const auto type=sqlite3_column_type(s,index);if(type==SQLITE_NULL){require(c.nullable,"SQLite NULL violates nonnull declaration");return V::absent();}V out;
    if(c.kind==ScalarKind::Integer && type==SQLITE_INTEGER)out=V::integer(numeric::Integer(std::to_string(sqlite3_column_int64(s,index))));
    else if((c.kind==ScalarKind::Text && type==SQLITE_TEXT) || (c.kind==ScalarKind::Blob && type==SQLITE_BLOB)){
        const auto bytes=octets(s,index,type==SQLITE_TEXT,cap);std::vector<V> values;values.reserve(bytes.size());for(const char b:bytes)values.push_back(V::symbol(static_cast<unsigned char>(b)));out=V::repeated(std::move(values));}
    else throw std::invalid_argument("SQLite runtime storage class differs from declaration; coercion rejected");
    return c.nullable?V::present(std::move(out)):out;
}
} // namespace sqlite_interpretation_detail

// One read-only transaction, closed before return. Remaining columns are
// discovered from metadata: callers declare identity/role interpretation, not
// every payload field or bit width. This is NOT a universal SQL compiler.
inline SQLiteLogicalRead read_sqlite_snapshot(const std::filesystem::path& path,const SQLiteInterpretationPlan& plan,
                                               const std::string& revision,const database::InterpretationLimits& limits={}){
    using namespace sqlite_interpretation_detail;
    require(plan.source_namespace && plan.binding_namespace && plan.value_namespace && plan.version,"SQLite namespaces/version required");
    require(plan.binding_namespace!=plan.value_namespace,"SQLite payload/binding namespaces overlap");
    bound(machine::checked_size_add(plan.assertions.size(),plan.subjects.size()),limits.max_records,"SQLite binding count");
    bound(plan.max_value_octets,limits.declarations.max_value_nodes,"SQLite byte constructor bound");
    std::size_t metadata=0;database::interpretation_detail::token(revision,metadata,limits);
    for(const auto& s:plan.subjects){database::interpretation_detail::token(s.table,metadata,limits);database::interpretation_detail::token(s.identity_column,metadata,limits);}
    for(const auto& a:plan.assertions){database::interpretation_detail::token(a.table,metadata,limits);database::interpretation_detail::token(a.lineage_column,metadata,limits);for(const auto& r:a.references)database::interpretation_detail::token(r.column,metadata,limits);}
    Connection db(path,plan.max_vm_steps);SQLiteLogicalRead out;out.snapshot.revision=revision;
    out.schema.source_namespace=plan.source_namespace;out.schema.binding_namespace=plan.binding_namespace;out.schema.version=plan.version;
    const auto id=[&](std::uint64_t key){return composite::DeclarationId{plan.value_namespace,key,plan.version};};
    std::vector<std::uint64_t> byte_symbols(256);for(std::size_t i=0;i<256;++i)byte_symbols[i]=i;
    using D=composite::Declaration;
    std::vector<D> definitions{D::integer(id(1)),D::finite(id(2),std::move(byte_symbols)),D::repeated(id(3),id(2),0,plan.max_value_octets),
        D::repeated(id(4),id(2),0,plan.max_value_octets),D::optional(id(5),id(1)),D::optional(id(6),id(3)),D::optional(id(7),id(4))};
    std::map<std::uint64_t,ScalarKind> key_types;std::size_t records=0,nodes=0;
    auto subjects=plan.subjects;std::sort(subjects.begin(),subjects.end(),[](const auto& a,const auto& b){return a.kind<b.kind;});
    for(const auto& s:subjects){
        const auto cols=columns(db,s.table,limits);const auto& key=column(cols,s.identity_column);
        require(s.kind && key_types.emplace(s.kind,key.kind).second,"duplicate/zero SQLite subject kind");out.schema.subjects.push_back({s.kind,s.table,s.identity_column,encoding(key.kind)});
        Statement rows(db.get(),"SELECT "+quote_identifier(s.identity_column)+" FROM "+quote_identifier(s.table));
        while(rows.next()){add(records,1,limits.max_records,"SQLite record budget");auto token=identity(rows.get(),0,key.kind,limits.max_origin_bytes);database::interpretation_detail::token(token,metadata,limits);out.snapshot.subjects.push_back({s.kind,std::move(token)});}}
    auto bindings=plan.assertions;std::sort(bindings.begin(),bindings.end(),[](const auto& a,const auto& b){return a.kind<b.kind;});
    std::set<std::uint64_t> kinds;std::uint64_t definition_key=8;
    for(auto binding:bindings){
        require(binding.kind && kinds.insert(binding.kind).second,"duplicate/zero SQLite assertion kind");const auto cols=columns(db,binding.table,limits);const auto& key=column(cols,binding.lineage_column);
        std::sort(binding.references.begin(),binding.references.end(),[](const auto& a,const auto& b){return a.role<b.role;});
        database::ExternalAssertionKind type;type.key=binding.kind;type.label=binding.table;type.payload=id(definition_key++);type.lineage_label=binding.lineage_column;type.lineage_encoding=encoding(key.kind);
        std::set<std::string> excluded{binding.lineage_column};std::set<std::uint64_t> roles;
        for(const auto& r:binding.references){const auto& c=column(cols,r.column);require(r.role && roles.insert(r.role).second,"duplicate/zero SQLite role");const auto target=key_types.find(r.participant_kind);
            require(target!=key_types.end() && target->second==c.kind,"SQLite reference/target require identical declared storage classes");
            type.roles.push_back({r.role,r.participant_kind,1,1,false,c.nullable});type.reference_labels.push_back({r.role,r.column});excluded.insert(r.column);}
        std::vector<Column> payload;std::vector<composite::DeclarationFactor> factors;
        for(const auto& c:cols)if(!excluded.contains(c.name)){const std::uint64_t scalar=c.kind==ScalarKind::Integer?1:c.kind==ScalarKind::Text?3:4;
            const std::uint64_t optional=c.kind==ScalarKind::Integer?5:c.kind==ScalarKind::Text?6:7;const auto f=static_cast<std::uint64_t>(payload.size()+1);
            factors.push_back({f,id(c.nullable?optional:scalar),0});payload.push_back(c);type.fields.push_back({f,c.name});}
        definitions.push_back(D::product(type.payload,std::move(factors)));out.schema.assertions.push_back(type);
        std::string sql="SELECT "+quote_identifier(binding.lineage_column);for(const auto& r:binding.references)sql+=","+quote_identifier(r.column);for(const auto& c:payload)sql+=","+quote_identifier(c.name);sql+=" FROM "+quote_identifier(binding.table);
        Statement rows(db.get(),sql);
        while(rows.next()){
            add(records,1,limits.max_records,"SQLite record budget");database::ExternalAssertion a;a.origin={binding.kind,identity(rows.get(),0,key.kind,limits.max_origin_bytes)};database::interpretation_detail::origin(a.origin,metadata,limits);int index=1;
            for(const auto& reference:binding.references){add(records,1,limits.max_records,"SQLite role/occurrence budget");database::ExternalRoleValue r;r.role=reference.role;
                if(sqlite3_column_type(rows.get(),index)==SQLITE_NULL){require(column(cols,reference.column).nullable,"nonnull SQLite reference is NULL");r.state=database::ReferenceState::Null;}
                else{auto token=identity(rows.get(),index,key_types.at(reference.participant_kind),limits.max_origin_bytes);database::interpretation_detail::token(token,metadata,limits);
                    r.occurrences.push_back({"role/"+std::to_string(reference.role),{reference.participant_kind,std::move(token)},true});}a.roles.push_back(std::move(r));++index;}
            std::vector<V> values;values.reserve(payload.size());
            for(const auto& c:payload){std::size_t count=2;const auto storage_class=sqlite3_column_type(rows.get(),index);
                if(storage_class==SQLITE_TEXT || storage_class==SQLITE_BLOB)count=machine::checked_size_add(count,octets(rows.get(),index,storage_class==SQLITE_TEXT,plan.max_value_octets).size());
                add(nodes,count,limits.declarations.max_value_nodes,"SQLite aggregate value-node budget");bound(machine::checked_size_mul(nodes,sizeof(V)),limits.declarations.max_materialized_bytes,"SQLite materialization budget");
                values.push_back(value(rows.get(),index++,c,plan.max_value_octets));}
            a.payload=V::product(std::move(values));out.snapshot.assertions.push_back(std::move(a));}
    }
    out.schema.values=composite::DeclarationRegistry(std::move(definitions),limits.declarations);
    database::interpretation_detail::CompiledInterpretation compiled(out.schema,limits);out.schema=compiled.schema;
    out.snapshot=database::interpretation_detail::normalize_snapshot(out.snapshot,compiled,limits);db.exec("COMMIT");return out;
}
inline database::InterpretedSnapshot interpret_sqlite_snapshot(const std::filesystem::path& path,const SQLiteInterpretationPlan& plan,
                                                               const std::string& revision,const database::InterpretationLimits& limits={}){
    const auto input=read_sqlite_snapshot(path,plan,revision,limits);return database::InterpretedSnapshot::interpret(input.schema,input.snapshot,limits);
}
} // namespace rfc::io
