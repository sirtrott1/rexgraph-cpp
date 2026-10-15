#include "fixture.hpp"
#include <iostream>

int main() {
    try {
        using namespace native_covering_example;
        const auto plan=prepare();const auto numeric=plan.numeric_plan();Guard guard;
        const auto walk=plan.pairwise_walk({grade(0),10},loop_steps(plan),available);
        const auto lifted=plan.lift(walk,{grade(0),10},available);
        const auto action=plan.monodromy(walk,available);
        const auto deck=plan.deck(deck_request(plan,3),available);
        const auto base=plan.chain(Channel::Base,grade(1),{{0,Q{1}}},available);
        const auto transferred=plan.transfer(base,available);
        const auto pulled=prepare(cycle(6),cycle(3),true);
        const auto square=Square::prepare(plan,pulled,square_request(plan,pulled),square_available);
        const auto mediator=square.mediate(cone(square),pulled_available.cover,square_available);
        std::cout<<"fiber_size="<<numeric.fiber({grade(0),0},guard).size()<<'\n';
        std::cout<<"lift_endpoint="<<numeric.source().grade(grade(0)).identities[lifted.lift().lifted.finish().index]<<'\n';
        std::cout<<"monodromy="<<action.action().image()[0]<<','<<action.action().image()[1]<<'\n';
        std::cout<<"deck_vertices="<<deck.transformation().map().cells[0].size()<<'\n';
        std::cout<<"transfer_support="<<transferred.field().values().size()<<'\n';
        std::cout<<"mediator_vertices="<<mediator.mediator().map().cells[0].size()<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
