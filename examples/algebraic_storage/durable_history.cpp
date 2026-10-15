#include "model.hpp"
#include "rfc/storage/algebraic_archive_durable.hpp"
#include <iostream>

int main(int argc,char** argv) {
    using namespace archive_example; using namespace rfc::storage;
    if (argc!=2) {std::cerr<<"usage: algebraic_durable_history new-store-directory\n";return 1;}
    try {
        const auto sample=cases().front(); const auto first=make(sample),second=make(sample,false,Q{7});
        const AlgebraicArchiveChannel channel{701,1,100};
        const std::vector<Archive> revisions{second};
        DurableNativeRoot committed;
        {auto store=AlgebraicDurableHistory::create(argv[1],first,channel,9001,40);committed=store.append(1,store.root(),revisions).root;}
        auto store=AlgebraicDurableHistory::open(argv[1],channel);if (store.root()!=committed) throw std::runtime_error("recovered root differs");
        const auto snapshot=store.snapshot();
        const auto first_endpoint=snapshot.at(40),last_endpoint=snapshot.at(41);
        const auto past=read_algebraic_archive(first_endpoint.endpoint(),channel);
        const auto present=read_algebraic_archive(last_endpoint.endpoint(),channel);
        if (encode_algebraic_archive(past)!=encode_algebraic_archive(first) || encode_algebraic_archive(present)!=encode_algebraic_archive(second))
            throw std::runtime_error("recovered typed history differs");
        std::cout<<"{\"first_sequence\":40,\"last_sequence\":41,\"reopened\":true,\"prefix_preserved\":true}\n";
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
