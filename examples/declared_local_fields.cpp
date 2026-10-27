#include "declared_local_field_data.hpp"
#include <iostream>

int main() {
    using namespace declared_field_example;
    try {
        const auto f=make();
        const auto source_image=encode_rfcx(f.endpoint.source);
        const auto channels=encode_temporal_endpoint(f.endpoint);
        MappedRfcx source(source_image.bytes);
        const auto restored=decode_temporal_endpoint(source.materialize(),channels);
        const DeclaredLocalFieldBinding binding(restored,f.contract);
        const auto down=field::SparseStalkFieldActions(binding.fibers()).boundary(binding.bind_downward(g1,f.down_inputs));
        const auto up=local::SparseSheafActions(binding.sheaf()).coboundary(binding.bind_upward(g0,f.up_inputs));
        std::cout << "Downward: ";
        for(const auto& [cell,v]:down.field.values()) for(const auto& x:v.entries())
            std::cout<<"cell "<<cell<<" coordinate "<<x.index<<" = "<<x.value<<"; ";
        std::cout << "\nUpward: ";
        for(const auto& [cell,v]:up.field.values()) for(const auto& x:v.entries())
            std::cout<<"cell "<<cell<<" coordinate "<<x.index<<" = "<<x.value<<"; ";
        auto output=binding.endpoint();
        const auto records=binding.export_downward(down.field,300,1300);
        output.tensors.insert(output.tensors.end(),records.tensors.begin(),records.tensors.end());
        output.declarations->values.insert(output.declarations->values.end(),records.values.begin(),records.values.end());
        const auto encoded=encode_temporal_endpoint(output);
        const auto verified=decode_temporal_endpoint(source.materialize(),encoded);
        if(encode_temporal_endpoint(verified)!=encoded)throw std::runtime_error("output encoding differs");
        std::cout<<"\nShared source: "<<binding.fibers().source_owner().same_owner(binding.sheaf().source_owner())
                 <<"; retained occurrences: "<<binding.source_owner().source().grade(g1).incidence_count()
                 <<"; encoded result bytes: "<<encoded.size()<<"\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
