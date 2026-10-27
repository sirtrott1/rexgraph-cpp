#pragma once
// Optional downstream database envelope over native temporal storage. No RFC semantics change.
#include "rfc/storage/native_series_codec.hpp"
#include "rfc/storage/native_accession.hpp"
#include <openssl/evp.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <system_error>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace rfc_preview {
using Bytes = std::vector<std::byte>;
using Digest = std::array<std::byte, 32>;
using rfc::storage::NativeTemporalSeries;
using rfc::storage::NativeSeriesLimits;
inline void require(bool ok, const char* msg) { if (!ok) throw std::invalid_argument(msg); }
inline std::size_t add(std::size_t a, std::size_t b) { return rfc::machine::checked_size_add(a,b); }
inline std::size_t mul(std::size_t a, std::size_t b) { return rfc::machine::checked_size_mul(a,b); }
inline Digest digest(std::span<const std::byte> bytes) {
    Digest out{}; unsigned n{};
    if (EVP_Digest(bytes.data(),bytes.size(),reinterpret_cast<unsigned char*>(out.data()),&n,EVP_sha256(),nullptr)!=1 || n!=out.size())
        throw std::runtime_error("SHA-256 computation failed");
    return out;
}
inline std::string hex(const Digest& d) {
    constexpr char digits[]="0123456789abcdef"; std::string s; s.reserve(64);
    for(auto b:d) { auto v=std::to_integer<unsigned>(b);s.push_back(digits[v>>4]);s.push_back(digits[v&15]); }
    return s;
}
inline bool equal(std::span<const std::byte> a,std::span<const std::byte> b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin());
}
struct Limits {
    std::size_t max_file_bytes{256u<<20},max_components{100000},max_component_bytes{128u<<20};
    NativeSeriesLimits series{};
};
inline void bounded(std::size_t n,std::size_t limit,const char* what) {
    if(n>limit)throw std::length_error(what);
}
// Component identity is (kind, sequence), never its SHA or physical offset.
enum class Kind : std::uint64_t { BaseSource=1,BaseChannels=2,Segment=3,CheckpointSource=4,CheckpointChannels=5 };
struct Entry {
    Kind kind{}; std::uint64_t sequence{}; std::size_t offset{},length{}; Digest sha{};
    bool operator==(const Entry&) const = default;
};
struct Manifest {
    std::uint64_t name_space{},first{},last{}; std::vector<Entry> entries;
    bool operator==(const Manifest&) const = default;
};
struct PublicationStats {std::size_t components{},created_objects{},reused_objects{},written_object_bytes{},manifest_bytes{};};
namespace detail {
inline constexpr std::array<std::byte,8> file_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'D'},std::byte{'B'},std::byte{1},std::byte{0}};
inline constexpr std::array<std::byte,8> manifest_magic{std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'X'},std::byte{'D'},std::byte{'M'},std::byte{1},std::byte{0}};
struct Writer {
    Bytes b;
    void raw(std::span<const std::byte> s){b.insert(b.end(),s.begin(),s.end());}
    void u32(std::uint32_t v){for(unsigned i=0;i<4;++i)b.push_back(std::byte((v>>(8*i))&255));}
    void u64(std::uint64_t v){for(unsigned i=0;i<8;++i)b.push_back(std::byte((v>>(8*i))&255));}
};
struct Reader {
    std::span<const std::byte> b;std::size_t p{};
    std::span<const std::byte> take(std::size_t n){require(n<=b.size()-p,"truncated database bytes");auto s=b.subspan(p,n);p+=n;return s;}
    std::uint32_t u32(){auto s=take(4);std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(std::to_integer<unsigned>(s[i]))<<(8*i);return v;}
    std::uint64_t u64(){auto s=take(8);std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(std::to_integer<unsigned>(s[i]))<<(8*i);return v;}
    std::size_t size(){return rfc::machine::checked_size(u64());}
    Digest sha(){Digest d{};auto s=take(32);std::copy(s.begin(),s.end(),d.begin());return d;}
    bool end()const{return p==b.size();}
};
inline Bytes encode_manifest(const Manifest& m){
    Writer w;w.b.reserve(add(48,mul(m.entries.size(),64)));w.raw(manifest_magic);w.u32(1);w.u32(0);
    w.u64(m.name_space);w.u64(m.first);w.u64(m.last);w.u64(m.entries.size());
    for(const auto& e:m.entries){w.u64(static_cast<std::uint64_t>(e.kind));w.u64(e.sequence);w.u64(e.offset);w.u64(e.length);w.raw(e.sha);}
    return std::move(w.b);
}
inline void validate_order(const Manifest& m,const Limits& limits) {
    require(m.name_space!=0 && m.first<=m.last,"invalid database namespace/interval");
    bounded(m.entries.size(),limits.max_components,"database component count budget");
    bounded(rfc::machine::checked_size(m.last-m.first),limits.series.max_segments,"database history step budget");
    require(m.entries.size()>=2,"database needs canonical base source and channels");
    require(m.entries[0].kind==Kind::BaseSource && m.entries[0].sequence==m.first &&
            m.entries[1].kind==Kind::BaseChannels && m.entries[1].sequence==m.first,"invalid base components");
    const auto frames=rfc::machine::checked_size(m.last-m.first);
    require(frames<=m.entries.size()-2,"database missing segment entries");
    std::size_t cursor=2;
    for(std::size_t i=0;i<frames;++i,++cursor)
        require(m.entries[cursor].kind==Kind::Segment && m.entries[cursor].sequence==m.first+i+1,"missing/reordered temporal segment");
    require((m.entries.size()-cursor)%2==0,"unpaired checkpoint components");
    bounded((m.entries.size()-cursor)/2,limits.series.max_checkpoints,"database checkpoint budget");
    std::optional<std::uint64_t> last;
    for(;cursor<m.entries.size();cursor+=2){
        const auto& a=m.entries[cursor];const auto& b=m.entries[cursor+1];
        require(a.kind==Kind::CheckpointSource && b.kind==Kind::CheckpointChannels && a.sequence==b.sequence &&
                a.sequence>=m.first && a.sequence<=m.last && (!last || a.sequence>*last),"invalid checkpoint order/pair");last=a.sequence;
    }
    std::map<Digest,std::pair<std::size_t,std::size_t>> extents;
    std::size_t offset=0,logical=0;
    for(const auto& e:m.entries){
        require(e.length>0,"empty native component");bounded(e.length,limits.max_component_bytes,"database component size budget");
        logical=add(logical,e.length);bounded(logical,limits.max_file_bytes,"database expanded component budget");
        auto [it,newly]=extents.emplace(e.sha,std::pair{e.offset,e.length});
        if(newly){require(e.offset==offset,"noncanonical offset, gap, or overlap");offset=add(offset,e.length);}
        else require(it->second==std::pair{e.offset,e.length},"inconsistent deduplicated extent");
    }
    bounded(add(add(64,add(48,mul(m.entries.size(),64))),offset),limits.max_file_bytes,"database file budget");
}
inline Manifest decode_manifest(std::span<const std::byte> b,const Limits& limits){
    bounded(b.size(),limits.max_file_bytes,"manifest byte budget");Reader r{b};
    require(equal(r.take(8),manifest_magic),"invalid database manifest magic");
    require(r.u32()==1 && r.u32()==0,"unsupported database manifest version/flags");
    Manifest m;m.name_space=r.u64();m.first=r.u64();m.last=r.u64();auto n=r.size();
    bounded(n,limits.max_components,"manifest entry budget");require(n<=(b.size()-r.p)/64,"truncated manifest entry array");
    m.entries.reserve(n);
    for(std::size_t i=0;i<n;++i){Entry e;auto k=r.u64();require(k>=1&&k<=5,"unknown database component kind");e.kind=static_cast<Kind>(k);
        e.sequence=r.u64();e.offset=r.size();e.length=r.size();e.sha=r.sha();m.entries.push_back(e);}
    require(r.end(),"trailing manifest bytes");validate_order(m,limits);return m;
}
class Fd {
    int fd_{-1};
public:
    explicit Fd(int f):fd_(f){if(f<0)throw std::system_error(errno,std::generic_category(),"database file operation");}
    ~Fd(){if(fd_>=0)::close(fd_);} Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;
    Fd(Fd&& x)noexcept:fd_(std::exchange(x.fd_,-1)){} int get()const{return fd_;}
};
inline Bytes read_fd(int fd,std::size_t limit) {
    struct stat st{};if(::fstat(fd,&st)!=0)throw std::system_error(errno,std::generic_category(),"fstat");
    require(S_ISREG(st.st_mode)&&st.st_size>=0,"database input must be a regular file");
    const auto n=static_cast<std::uintmax_t>(st.st_size);require(n<=limit && n<=std::numeric_limits<std::size_t>::max(),"database file size budget");
    Bytes bytes(static_cast<std::size_t>(n));std::size_t pos=0;
    while(pos<bytes.size()) { auto got=::read(fd,bytes.data()+pos,std::min(bytes.size()-pos,std::size_t{1u<<20}));
        if(got<0){if(errno==EINTR)continue;throw std::system_error(errno,std::generic_category(),"read");}
        require(got>0,"database input truncated during read");pos+=static_cast<std::size_t>(got); }
    std::byte extra{};ssize_t got;do{got=::read(fd,&extra,1);}while(got<0&&errno==EINTR);
    require(got==0,"database input changed during read");return bytes;
}
inline Bytes read_at(int dir,const char* name,std::size_t limit) {
    Fd f(::openat(dir,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));return read_fd(f.get(),limit);
}
inline void sync(int fd){if(::fsync(fd)!=0)throw std::system_error(errno,std::generic_category(),"database fsync");}
inline void write_fd(int fd,std::span<const std::byte> b){std::size_t p=0;while(p<b.size()){
    const auto n=::write(fd,b.data()+p,std::min(b.size()-p,std::size_t{1u<<20}));
    if(n<0){if(errno==EINTR)continue;throw std::system_error(errno,std::generic_category(),"database write");}
    require(n>0,"zero-length database write");p+=static_cast<std::size_t>(n);}sync(fd);}
inline void write_at(int dir,const char* name,std::span<const std::byte> b,bool immutable=false){
    Fd f(::openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600));write_fd(f.get(),b);
    if(immutable){if(::fchmod(f.get(),0400)!=0)throw std::system_error(errno,std::generic_category(),"protect immutable component");sync(f.get());}
}
} // namespace detail

inline Bytes read_file(const std::filesystem::path& p,std::size_t limit=256u<<20){
    rfc::storage::validate_file_path(p);
    detail::Fd f(::open(p.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));return detail::read_fd(f.get(),limit);
}
inline void write_new_file(const std::filesystem::path& p,std::span<const std::byte> b){
    rfc::storage::validate_file_path(p);
    detail::Fd f(::open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600));detail::write_fd(f.get(),b);
}

// Immutable ownership. Every component is a slice of one pinned canonical file;
// lookup performs no tensor allocation, SHA computation, or history replay.
class Database {
    struct State { Manifest manifest;std::shared_ptr<const Bytes> image;std::size_t payload{};
        NativeTemporalSeries series;
        State(Manifest m,std::shared_ptr<const Bytes> b,std::size_t p,NativeTemporalSeries s)
            :manifest(std::move(m)),image(std::move(b)),payload(p),series(std::move(s)){} };
    std::shared_ptr<const State> state_;
    explicit Database(std::shared_ptr<const State> s):state_(std::move(s)){}
    const State& s()const{require(bool(state_),"moved-from database");return *state_;}
public:
    struct ComponentPin {
        std::shared_ptr<const Bytes> owner;std::size_t offset{},length{};
        std::span<const std::byte> bytes()const{require(bool(owner),"moved-from component pin");require(offset<=owner->size() && length<=owner->size()-offset,"component pin out of bounds");return std::span<const std::byte>(*owner).subspan(offset,length);}
    };
    const Manifest& manifest()const &{return s().manifest;}
    const Manifest& manifest()const &&=delete;
    const NativeTemporalSeries& series()const &{return s().series;}
    const NativeTemporalSeries& series()const &&=delete;
    std::span<const std::byte> bytes()const &{return *s().image;}
    std::span<const std::byte> bytes()const &&=delete;
    ComponentPin component(std::size_t i)const{const auto& e=s().manifest.entries.at(i);return {s().image,add(s().payload,e.offset),e.length};}
    Digest publication_digest()const{return digest(*s().image);}
    // A *derived* exact allocation tensor, not identities cast to host pointers.
    // Axis 0: manifest component order. Axis 1: [object ordinal, offset, length].
    rfc::numeric::SparseRationalTensor placement_tensor()const{
        using namespace rfc::numeric;std::map<Digest,std::size_t> objects;std::vector<SparseRationalEntry> entries;
        for(std::size_t i=0;i<s().manifest.entries.size();++i){const auto& e=s().manifest.entries[i];auto [it,_]=objects.emplace(e.sha,objects.size());
            const std::array<std::size_t,3> values{it->second,e.offset,e.length};
            for(std::size_t j=0;j<3;++j)if(values[j])entries.push_back({i*3+j,Rational(Integer(std::to_string(values[j])))});}
        IndexDomain domain({s().manifest.entries.size(),3});auto size=domain.logical_size();
        return SparseRationalTensor(std::move(domain),StructuralSupport::full(size),SparseRationalVector(size,entries));
    }
    static Database decode(std::span<const std::byte> input,const Limits& limits={}){
        bounded(input.size(),limits.max_file_bytes,"database file byte budget");require(input.size()>=64,"truncated database file header");
        // Copy before validation: no mutable caller alias is retained.
        auto image=std::make_shared<const Bytes>(input.begin(),input.end());detail::Reader r{*image};
        require(equal(r.take(8),detail::file_magic),"invalid database magic");
        require(r.u32()==1&&r.u32()==0,"unsupported database version/flags");require(r.size()==image->size(),"database length mismatch");
        const auto mn=r.size();auto mh=r.sha();auto mb=r.take(mn);require(digest(mb)==mh,"database manifest integrity mismatch");
        auto m=detail::decode_manifest(mb,limits);const auto payload=r.p;
        std::size_t extent=0;std::map<Digest,std::span<const std::byte>> verified;
        std::vector<std::span<const std::byte>> components;components.reserve(m.entries.size());
        for(const auto& e:m.entries){require(e.offset<=image->size()-payload && e.length<=image->size()-payload-e.offset,"database extent outside payload");
            auto b=std::span<const std::byte>(*image).subspan(payload+e.offset,e.length);extent=std::max(extent,add(e.offset,e.length));
            auto [it,fresh]=verified.emplace(e.sha,b);if(fresh)require(digest(b)==e.sha,"database component integrity mismatch");
            else { require(equal(it->second,b),"SHA collision: unequal component contents"); }
            components.push_back(b);
        }
        require(extent==image->size()-payload,"trailing database payload");
        // Reconstruct through the existing canonical RFCXS decoder, including
        // every ordered operation and independent checkpoint validation.
        std::size_t archive_size=56;
        for(const auto& e:m.entries)archive_size=add(archive_size,add(8,e.length));
        const auto frame_count=static_cast<std::size_t>(m.last-m.first);
        archive_size=add(archive_size,mul((m.entries.size()-2-frame_count)/2,8));
        bounded(archive_size,limits.series.max_archive_bytes,"reconstructed series byte budget");
        detail::Writer archive;archive.b.reserve(archive_size);archive.raw(rfc::storage::native_series_codec_detail::magic);archive.u32(1);archive.u32(0);archive.u64(0);archive.u64(0);
        archive.u64(m.name_space);auto blob=[&](std::span<const std::byte> b){archive.u64(b.size());archive.raw(b);};
        blob(components[0]);blob(components[1]);const auto frames=static_cast<std::size_t>(m.last-m.first);archive.u64(frames);
        for(std::size_t i=0;i<frames;++i) { blob(components[2+i]); }
        auto cursor=2+frames;archive.u64((components.size()-cursor)/2);
        for(;cursor<components.size();cursor+=2){archive.u64(m.entries[cursor].sequence);blob(components[cursor]);blob(components[cursor+1]);}
        bounded(archive.b.size(),limits.series.max_archive_bytes,"reconstructed series byte budget");
        auto patch=[&](std::size_t p,std::uint64_t v){for(unsigned j=0;j<8;++j)archive.b[p+j]=std::byte((v>>(8*j))&255);};
        patch(16,archive.b.size());patch(24,rfc::storage::rfcx_detail::fnv1a64(std::span<const std::byte>(archive.b).subspan(32)));
        auto series=rfc::storage::decode_native_series(archive.b,limits.series);
        require(series.first_sequence()==m.first && series.last_sequence()==m.last,"manifest interval disagrees with native history");
        return Database(std::make_shared<const State>(std::move(m),std::move(image),payload,std::move(series)));
    }
    static Database from_series(const NativeTemporalSeries& series,const Limits& limits={}){
        // Use canonical public bytes as the boundary; no dependency on private
        // series state structures or alternative source serialization.
        bounded(series.archive_bytes(),limits.series.max_archive_bytes,"input series byte budget");bounded(series.archive_bytes(),limits.max_file_bytes,"input series exceeds database budget");
        Manifest m;m.name_space=series.source_namespace();m.first=series.first_sequence();m.last=series.last_sequence();
        std::vector<Bytes> objects;std::map<Digest,std::pair<std::size_t,std::size_t>> offsets;std::size_t next=0,expanded=0;
        auto push=[&](Kind kind,std::uint64_t seq,std::span<const std::byte> bytes){
            bounded(add(m.entries.size(),1),limits.max_components,"input component count budget");
            bounded(bytes.size(),limits.max_component_bytes,"input component size budget");expanded=add(expanded,bytes.size());
            bounded(expanded,limits.max_file_bytes,"input expanded component budget");auto d=digest(bytes);auto it=offsets.find(d);std::size_t offset{};
            if(it==offsets.end()){offset=next;next=add(next,bytes.size());bounded(add(add(112,mul(add(m.entries.size(),1),64)),next),limits.max_file_bytes,"input database size budget");offsets.emplace(d,std::pair{offset,objects.size()});objects.emplace_back(bytes.begin(),bytes.end());}
            else{offset=it->second.first;require(equal(objects[it->second.second],bytes),"SHA collision in database packing");}
            m.entries.push_back({kind,seq,offset,bytes.size(),d});};
        series.visit_components([&](const rfc::storage::NativeSeriesComponent& part){push(component_kind(part.kind),part.sequence,*part.bytes);});
        detail::validate_order(m,limits);auto mb=detail::encode_manifest(m);
        detail::Writer w;w.b.reserve(add(add(64,mb.size()),next));w.raw(detail::file_magic);w.u32(1);w.u32(0);w.u64(add(add(64,mb.size()),next));
        w.u64(mb.size());w.raw(digest(mb));w.raw(mb);for(const auto& o:objects)w.raw(o);
        return decode(w.b,limits);
    }
    // Fresh immutable directory publication in the existing database v1 format.
    // Equal digests only select candidates; complete bytes qualify reuse. The
    // component identities and ordered temporal history remain in MANIFEST.
    static PublicationStats save_series_directory(const NativeTemporalSeries& series,const std::filesystem::path& path,
        const std::filesystem::path& previous={},const Limits& limits={},const std::function<void(std::size_t)>& hook={}) {
        bounded(series.archive_bytes(),limits.series.max_archive_bytes,"input series byte budget");
        bounded(series.archive_bytes(),limits.max_file_bytes,"input series exceeds database budget");
        rfc::storage::validate_file_path(path);
        if(::mkdir(path.c_str(),0700)!=0)throw std::system_error(errno,std::generic_category(),"create fresh database directory");
        detail::Fd root(::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        if(::mkdirat(root.get(),"objects",0700)!=0)throw std::system_error(errno,std::generic_category(),"mkdir objects");
        detail::Fd objects(::openat(root.get(),"objects",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        std::optional<detail::Fd> prior;
        if(!previous.empty() && std::filesystem::is_directory(previous)) {
            // Validate the complete previous envelope before physical sharing.
            (void)open_directory(previous,limits);
            prior.emplace(::open((previous/"objects").c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        }
        Manifest m{series.source_namespace(),series.first_sequence(),series.last_sequence(),{}};
        PublicationStats stats;std::map<Digest,std::pair<std::size_t,std::size_t>> extents;std::size_t offset{},expanded{};
        series.visit_components([&](const rfc::storage::NativeSeriesComponent& part) {
            const auto bytes=std::span<const std::byte>(*part.bytes);const auto sha=digest(bytes);const auto name=hex(sha)+".blob";
            bounded(add(m.entries.size(),1),limits.max_components,"input component count budget");
            bounded(bytes.size(),limits.max_component_bytes,"input component size budget");expanded=add(expanded,bytes.size());
            bounded(expanded,limits.max_file_bytes,"input expanded component budget");
            const auto [entry,first]=extents.emplace(sha,std::pair{offset,bytes.size()});
            if(first) {
                bool reused=false;
                if(prior) {
                    struct stat status{};
                    if(::fstatat(prior->get(),name.c_str(),&status,AT_SYMLINK_NOFOLLOW)==0) {
                        const auto old=detail::read_at(prior->get(),name.c_str(),limits.max_component_bytes);
                        require(equal(old,bytes),"database object collision/corruption");
                        if((status.st_mode&0222)==0) {
                            reused=::linkat(prior->get(),name.c_str(),objects.get(),name.c_str(),0)==0;
                            if(!reused && errno!=EXDEV && errno!=EPERM && errno!=EOPNOTSUPP && errno!=EMLINK && errno!=ENOSYS)
                                throw std::system_error(errno,std::generic_category(),"share immutable database object");
                        }
                    } else if(errno!=ENOENT)throw std::system_error(errno,std::generic_category(),"inspect previous object");
                }
                if(reused)++stats.reused_objects;
                else {detail::write_at(objects.get(),name.c_str(),bytes,true);++stats.created_objects;stats.written_object_bytes=add(stats.written_object_bytes,bytes.size());}
                offset=add(offset,bytes.size());if(hook)hook(stats.created_objects+stats.reused_objects);
            } else {
                const auto existing=detail::read_at(objects.get(),name.c_str(),limits.max_component_bytes);
                require(equal(existing,bytes),"database component hash collision");
            }
            m.entries.push_back({component_kind(part.kind),part.sequence,entry->second.first,bytes.size(),sha});
        });
        detail::validate_order(m,limits);stats.components=m.entries.size();detail::sync(objects.get());
        const auto manifest=detail::encode_manifest(m);stats.manifest_bytes=manifest.size();detail::write_at(root.get(),"MANIFEST",manifest);detail::sync(root.get());
        const auto marker=hex(digest(manifest))+"\n";detail::write_at(root.get(),"COMMIT",std::as_bytes(std::span(marker.data(),marker.size())));detail::sync(root.get());
        return stats;
    }
    static Database open_file(const std::filesystem::path& path,const Limits& limits={}){auto b=read_file(path,limits.max_file_bytes);return decode(b,limits);}
    void save_file(const std::filesystem::path& path)const{write_new_file(path,*s().image);}
    // The directory is an immutable new publication. Objects and MANIFEST are
    // synced first; COMMIT is written last. Failure never overwrites an old DB.
    // Parent directory and concurrent mutation by other OS users are trusted.
    void save_directory(const std::filesystem::path& path,const std::function<void(std::size_t)>& after_object={})const{
        const auto state=state_;require(bool(state),"moved-from database");const auto hook=after_object;
        rfc::storage::validate_file_path(path);
        if(::mkdir(path.c_str(),0700)!=0)throw std::system_error(errno,std::generic_category(),"create fresh database directory");
        rfc::storage::validate_file_path(path);
        detail::Fd root(::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        if(::mkdirat(root.get(),"objects",0700)!=0)throw std::system_error(errno,std::generic_category(),"mkdir objects");
        detail::Fd objects(::openat(root.get(),"objects",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        std::set<Digest> written;std::size_t n=0;
        for(std::size_t i=0;i<state->manifest.entries.size();++i){auto d=state->manifest.entries[i].sha;if(written.insert(d).second){
            const auto name=hex(d)+".blob";detail::write_at(objects.get(),name.c_str(),std::span<const std::byte>(*state->image).subspan(state->payload+state->manifest.entries[i].offset,state->manifest.entries[i].length));if(hook)hook(++n);}}
        detail::sync(objects.get());auto mb=detail::encode_manifest(state->manifest);detail::write_at(root.get(),"MANIFEST",mb);detail::sync(root.get());
        const auto marker=hex(digest(mb))+"\n";
        detail::write_at(root.get(),"COMMIT",std::as_bytes(std::span(marker.data(),marker.size())));detail::sync(root.get());
    }
    static Database open_directory(const std::filesystem::path& path,const Limits& limits={}){
        rfc::storage::validate_file_path(path);
        detail::Fd root(::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        const auto marker=detail::read_at(root.get(),"COMMIT",65);require(marker.size()==65,"invalid/incomplete database commit marker");
        auto mb=detail::read_at(root.get(),"MANIFEST",limits.max_file_bytes);auto expected=hex(digest(mb))+"\n";
        require(equal(marker,std::as_bytes(std::span(expected.data(),expected.size()))),"manifest not bound by commit marker");
        auto m=detail::decode_manifest(mb,limits);detail::Fd objects(::openat(root.get(),"objects",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        detail::Writer w;w.raw(detail::file_magic);w.u32(1);w.u32(0);w.u64(0);w.u64(mb.size());w.raw(digest(mb));w.raw(mb);
        std::set<Digest> seen;
        for(const auto& e:m.entries)if(seen.insert(e.sha).second){auto name=hex(e.sha)+".blob";auto b=detail::read_at(objects.get(),name.c_str(),std::min(e.length,limits.max_component_bytes));
            require(b.size()==e.length&&digest(b)==e.sha,"changed/truncated database object");w.raw(b);}
        for(unsigned j=0;j<8;++j)w.b[16+j]=std::byte((std::uint64_t(w.b.size())>>(8*j))&255);
        return decode(w.b,limits);
    }
    static Database open(const std::filesystem::path& path,const Limits& limits={}){
        rfc::storage::validate_file_path(path);
        auto st=std::filesystem::symlink_status(path);require(!std::filesystem::is_symlink(st),"database root symlink rejected");
        return std::filesystem::is_directory(st)?open_directory(path,limits):open_file(path,limits);
    }
private:
    static Kind component_kind(rfc::storage::NativeSeriesComponentKind kind) {
        using C=rfc::storage::NativeSeriesComponentKind;
        switch(kind) {
            case C::BaseSource:return Kind::BaseSource;case C::BaseChannels:return Kind::BaseChannels;
            case C::Segment:return Kind::Segment;case C::CheckpointSource:return Kind::CheckpointSource;
            case C::CheckpointChannels:return Kind::CheckpointChannels;
        }
        throw std::invalid_argument("unknown native component kind");
    }
};
} // namespace rfc_preview
