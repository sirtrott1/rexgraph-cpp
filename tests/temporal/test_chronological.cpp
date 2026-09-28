#include "rfc/temporal/chronological.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

static void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}

int main(){
try{
    using rfc::numeric::ExactMatrix; using rfc::numeric::Rational; using rfc::numeric::RationalVector;
    const auto R0=ExactMatrix::from_rows({{1,0,0},{0,Rational{1,2},0},{0,Rational{1,2},1}});
    const auto R1=ExactMatrix::from_rows({{Rational{1,2},0,0},{Rational{1,2},1,0},{0,0,1}});
    rfc::temporal::ChronologicalEvolution original(RationalVector{1,0,0},{R0,R1},{RationalVector(3,0),RationalVector(3,0)});
    rfc::temporal::ChronologicalEvolution reversed(RationalVector{1,0,0},{R1,R0},{RationalVector(3,0),RationalVector(3,0)});
    require(original.terminal()==RationalVector({Rational{1,2},Rational{1,2},0}),"chronological original order mismatch");
    require(reversed.terminal()==RationalVector({Rational{1,2},Rational{1,4},Rational{1,4}}),"chronological reversed order mismatch");

    const rfc::temporal::OperationWord w_old({R0,R1});
    const rfc::temporal::OperationWord w_new({R1,R0});
    const auto I=ExactMatrix::identity(3);
    const auto defect=rfc::temporal::finite_word_defect(w_old,w_new,{I,I,I});
    require(defect.exact_identity(),"chronological word defect identity failed");
    const auto delta=defect.direct.apply(RationalVector{1,0,0});
    require(delta==RationalVector({0,Rational{-1,4},Rational{1,4}}),"chronological order delta mismatch");

    const std::vector<rfc::temporal::ScheduledContact> contacts{
        {0,2,0,5},{0,1,0,1},{1,2,1,2}
    };
    const auto earliest=rfc::temporal::earliest_arrival(3,contacts,0,2);
    const auto fewest=rfc::temporal::fewest_contacts(3,contacts,0,2);
    require(earliest.reachable && earliest.arrival==2 && earliest.contacts==2,"earliest arrival objective mismatch");
    require(fewest.reachable && fewest.contacts==1 && fewest.arrival==5,"fewest contacts objective mismatch");

    const auto Inj=ExactMatrix::from_rows({{1},{0},{0}});
    const auto Obs=ExactMatrix::from_rows({{0,0,1}});
    const auto channel=rfc::temporal::terminal_channel({R0,R1},0,Inj,Obs);
    require(channel.get(0,0)==0,"terminal channel should not reach c in original order");
    const auto reverse_channel=rfc::temporal::terminal_channel({R1,R0},0,Inj,Obs);
    require(reverse_channel.get(0,0)==Rational{1,4},"terminal channel reversed order mismatch");

    std::cout<<"chronological tests passed\n";
    return EXIT_SUCCESS;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
