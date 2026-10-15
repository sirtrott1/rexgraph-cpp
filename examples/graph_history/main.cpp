#include "model.hpp"
#include "rfc/storage/algebraic_archive_graph_durable.hpp"
#include <iostream>

using namespace rfc::storage;
static void emit(const AlgebraicGraphRevision& revision) {
    const auto& snapshot=revision.snapshot();const auto& graph=snapshot.graph();
    const auto& x=std::get<rfc::local::AlgebraicModuleVector>(graph.archive(1).at(10));
    const auto y=graph.carrier(1).apply(x);
    std::cout<<"{\"namespace\":"<<revision.source_namespace()<<",\"sequence\":"<<revision.sequence()
        <<",\"anchor_bundle\":"<<snapshot.anchor_bundle()<<",\"source_bindings\":"<<snapshot.source_bindings().size()
        <<",\"archive_bindings\":"<<snapshot.archive_bindings().size()<<",\"source_coordinates\":[";
    bool first=true;for (const auto& q:x.rational_coordinates()) {if (!first)std::cout<<',';first=false;std::cout<<'"'<<q.get_str()<<'"';}
    std::cout<<"],\"target_coordinates\":[";first=true;
    for (const auto& q:y.rational_coordinates()) {if (!first)std::cout<<',';first=false;std::cout<<'"'<<q.get_str()<<'"';}
    std::cout<<"],\"shared_archive_bundle\":"<<(graph.archive(1).bundle().same_owner(graph.archive(4).bundle())?"true":"false")
        <<",\"independent_equal_field\":"<<(!graph.field(1).same_owner(graph.field(4))?"true":"false")<<"}\n";
}
int main(int argc,char** argv) {
    try {
        const AlgebraicGraphChannel channel{7601,1,100};
        if (argc==3 && std::string_view(argv[1])=="--export") {
            const std::filesystem::path path{argv[2]};
            if (!std::filesystem::create_directory(path)) throw std::runtime_error("export directory must be new");
            write_algebraic_graph_snapshot(graph_history_example::snapshot(0),path/"anchor.rfcxh",FilePublication::CreateNew);
            write_algebraic_graph_snapshot(graph_history_example::snapshot(1,true),path/"mapped.rfcxh",FilePublication::CreateNew);
            return 0;
        }
        if (argc!=2) throw std::invalid_argument("usage: graph_history NEW_STORE_DIRECTORY | --export NEW_DIRECTORY");
        const std::filesystem::path path{argv[1]};
        std::optional<AlgebraicGraphHistoryWindow> selected;
        {
            auto history=AlgebraicGraphDurableHistory::create(path,graph_history_example::snapshot(0),channel,9501,40);
            const std::vector<AlgebraicGraphSnapshot> revisions{graph_history_example::snapshot(1,true),graph_history_example::snapshot(2,true)};
            const auto receipt=history.append(7,history.root(),revisions);
            if (receipt.frame_count!=2 || receipt.root.sequence!=42) throw std::runtime_error("group publication differs");
            selected=history.prefix(41);
        }
        auto history=AlgebraicGraphDurableHistory::open(path,channel);
        if (!history.status(7)) throw std::runtime_error("group receipt absent after reopen");
        emit(selected->at(40));emit(selected->at(41));emit(history.at(42));
        std::cout<<"{\"prefix_cutoff\":"<<selected->cutoff()<<",\"prefix_revisions\":"<<selected->revision_count()
            <<",\"durable_tail\":"<<history.root().sequence<<"}\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
