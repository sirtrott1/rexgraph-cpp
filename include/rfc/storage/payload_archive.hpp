#pragma once
#include "rfc/storage/payload_manifest.hpp"
#include <mutex>

namespace rfc::storage {
class PayloadArchiveReadError:public std::runtime_error {
public:PayloadArchiveReadError():std::runtime_error("archive range read failed/truncated"){}
};
struct PayloadArchiveLimits {
    std::size_t max_archive_bytes{1u<<30},max_metadata_bytes{64u<<20};
    PayloadManifestLimits manifest;
};
struct PayloadArchiveUsage {
    std::size_t metadata_bytes{},index_bytes{},metadata_io_bytes{},payload_io_bytes{},archive_bytes{};
};
namespace payload_archive_detail {
inline constexpr std::uint64_t magic=0x3141525058434652ULL;
struct Range {std::size_t offset{},size{};};
inline std::vector<std::byte> header(const PayloadManifest& manifest,std::size_t total) {
    rfcx_detail::Writer w;w.u64(magic);w.u64(1);w.u64(total);w.u64(manifest.bytes().size());w.u64(manifest.payloads().size());
    return std::move(w).take_bytes();
}
} // namespace payload_archive_detail

// Initial publication can consume resident inputs without composing a second
// whole-archive image. Runtime opening below reads only metadata, then named ranges.
inline void write_payload_archive(const std::filesystem::path& path,const PayloadManifest& original,
    std::span<const std::span<const std::byte>> payloads,PayloadArchiveLimits limits={}) {
    using namespace payload_manifest_detail;
    const auto manifest=original;
    require(payloads.size()==manifest.payloads().size(),"archive payload count differs from manifest");
    const auto index_bytes=checked_size_mul(payloads.size(),16);
    auto total=checked_size_add(checked_size_add(40,manifest.bytes().size()),index_bytes);
    bound(total,limits.max_metadata_bytes,"archive metadata publication admission");
    for(std::size_t i=0;i<payloads.size();++i) {
        require(payloads[i].size()==manifest.payloads()[i].stored_bytes &&
            rfcx_detail::fnv1a64(payloads[i])==manifest.payloads()[i].checksum,"archive payload differs from manifest");
        total=checked_size_add(total,payloads[i].size());bound(total,limits.max_archive_bytes,"archive byte publication admission");
    }
    const auto prefix=payload_archive_detail::header(manifest,total);
    publish_file_exact(path,FilePublication::CreateNew,[&](std::ostream& out) {
        write_stream_exact(out,prefix);write_stream_exact(out,manifest.bytes());
        auto offset=checked_size_add(checked_size_add(40,manifest.bytes().size()),index_bytes);
        for(auto bytes:payloads) {
            rfcx_detail::Writer w;w.u64(offset);w.u64(bytes.size());write_stream_exact(out,w.bytes());offset=checked_size_add(offset,bytes.size());
        }
        for(auto bytes:payloads)write_stream_exact(out,bytes);
    });
}

// An opened regular file and immutable selected manifest/index. The stream pins
// the opened file through atomic path replacement. In-place changes are detected
// on the selected payload by its checksum and subsequent full native decoding;
// checksums are corruption checks, not authenticity or restored owner authority.
class PayloadArchiveProvider {
    struct State {
        PayloadManifest manifest;
        std::ifstream stream;
        std::vector<payload_archive_detail::Range> ranges;
        PayloadArchiveUsage usage;
        mutable std::mutex mutex;
        explicit State(PayloadManifest m):manifest(std::move(m)){}
    };
    std::shared_ptr<State> state_;
    explicit PayloadArchiveProvider(std::shared_ptr<State> state):state_(std::move(state)){}
    std::vector<std::byte> range(std::size_t offset,std::size_t size,bool metadata,std::function<void()> check) const {
        using namespace payload_manifest_detail;
        require(offset<=state_->usage.archive_bytes && size<=state_->usage.archive_bytes-offset,"archive range exceeds selected file");
        require(offset<=static_cast<std::size_t>(std::numeric_limits<std::streamoff>::max()) &&
                size<=static_cast<std::size_t>(std::numeric_limits<std::streamoff>::max())-offset,"archive stream offset overflow");
        std::vector<std::byte> out(size);
        for(std::size_t p=0;p<size;) {
            if(check){check();}const auto n=std::min(size-p,std::size_t{65536});
            {
                // Callbacks never run under the stream lock; another request can
                // read its own range before this request resumes its next chunk.
                std::lock_guard lock(state_->mutex);state_->stream.clear();
                state_->stream.seekg(static_cast<std::streamoff>(offset+p));
                state_->stream.read(reinterpret_cast<char*>(out.data()+p),static_cast<std::streamsize>(n));
                const auto got=state_->stream.gcount();
                if(got>0) {
                    auto& counter=metadata?state_->usage.metadata_io_bytes:state_->usage.payload_io_bytes;
                    counter=checked_size_add(counter,static_cast<std::size_t>(got));
                }
                if(!state_->stream || got!=static_cast<std::streamsize>(n))throw PayloadArchiveReadError();
            }
            p+=n;
        }
        if(check){check();}
        return out;
    }
public:
    PayloadArchiveProvider(const PayloadArchiveProvider&)=default;
    PayloadArchiveProvider& operator=(const PayloadArchiveProvider&)=delete;
    static PayloadArchiveProvider open(const std::filesystem::path& path,const PayloadManifest& expected,
        PayloadArchiveLimits limits={},std::function<void()> check={}) {
        using namespace payload_manifest_detail;
        const auto manifest=expected;validate_file_path(path);
        const auto index_bytes=checked_size_mul(manifest.payloads().size(),sizeof(payload_archive_detail::Range));
        // Count the opened-stream control allowance separately from payloads.
        const auto metadata=checked_size_add(manifest.usage().metadata_bytes,
            checked_size_add(sizeof(State)+16384,index_bytes));
        const auto scratch=checked_size_add(manifest.bytes().size(),checked_size_mul(manifest.payloads().size(),16));
        bound(checked_size_add(metadata,scratch),limits.max_metadata_bytes,"archive metadata receiving admission");
        require(std::filesystem::is_regular_file(path),"archive requires a regular file");
        auto state=std::make_shared<State>(manifest);state->usage.metadata_bytes=metadata;state->usage.index_bytes=index_bytes;
        (void)state->stream.rdbuf()->pubsetbuf(nullptr,0);
        state->stream.open(path,std::ios::binary|std::ios::ate);
        if(!state->stream)throw std::runtime_error("cannot open payload archive");
        const auto end=state->stream.tellg();require(end>=40,"truncated payload archive header");
        bound(static_cast<std::size_t>(end),limits.max_archive_bytes,"archive file admission");
        state->usage.archive_bytes=static_cast<std::size_t>(end);PayloadArchiveProvider out(state);
        const auto h=out.range(0,40,true,check);rfcx_detail::Reader r(h);
        require(r.u64()==payload_archive_detail::magic && r.u64()==1,"unsupported payload archive version");
        require(r.u64()==state->usage.archive_bytes,"archive extent differs from original index");
        require(r.u64()==manifest.bytes().size() && r.u64()==manifest.payloads().size(),"archive metadata extent differs from selected manifest");
        const auto wire=out.range(40,manifest.bytes().size(),true,check);
        // Decode also revalidates the original manifest at the receiving limits.
        (void)PayloadManifest::decode_exact(wire,manifest,limits.manifest,check);
        const auto index=out.range(checked_size_add(40,wire.size()),checked_size_mul(manifest.payloads().size(),16),true,check);
        rfcx_detail::Reader positions(index);auto cursor=checked_size_add(checked_size_add(40,wire.size()),index.size());
        state->ranges.reserve(manifest.payloads().size());
        for(const auto& p:manifest.payloads()) {
            const auto offset=positions.size_value(),n=positions.size_value();
            require(offset==cursor && n==p.stored_bytes,"archive index overlaps/omits selected payload");
            cursor=checked_size_add(cursor,n);require(cursor<=state->usage.archive_bytes,"archive payload range is truncated");
            state->ranges.push_back({offset,n});if(check){check();}
        }
        require(cursor==state->usage.archive_bytes,"archive contains unindexed trailing bytes");
        return out;
    }
    const PayloadManifest& manifest() const & noexcept{return state_->manifest;}
    const PayloadManifest& manifest() const &&=delete;
    PayloadArchiveUsage usage() const {std::lock_guard lock(state_->mutex);return state_->usage;}
    std::vector<std::byte> read(const PayloadKey& key,std::size_t maximum,std::function<void()> check={}) const {
        const auto i=state_->manifest.find(key);if(!i)throw std::out_of_range("archive payload/source revision is not declared");
        const auto selected=state_->ranges[*i];payload_manifest_detail::bound(selected.size,maximum,"archive payload receiving admission");
        auto bytes=range(selected.offset,selected.size,false,std::move(check));
        payload_manifest_detail::require(rfcx_detail::fnv1a64(bytes)==state_->manifest.payloads()[*i].checksum,"archive payload checksum differs from selected original");
        return bytes;
    }
};
} // namespace rfc::storage
