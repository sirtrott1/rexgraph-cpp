#include "rfc/topology/rational_zigzag.hpp"
#include <iostream>

int main(){try{
    using namespace rfc;std::size_t cases;std::cin>>cases;
    for(std::size_t c=0;c<cases;++c){std::size_t n;std::cin>>n;std::vector<std::size_t> dims(n);for(auto& d:dims)std::cin>>d;
        std::vector<topology::RationalZigzagArrow> arrows;
        for(std::size_t i=1;i<n;++i){unsigned direction;std::cin>>direction;const bool forward=direction!=0;
            numeric::ExactMatrix a(dims[forward?i:i-1],dims[forward?i-1:i]);
            for(std::size_t row=0;row<a.rows();++row)for(std::size_t col=0;col<a.cols();++col){std::string s;std::cin>>s;numeric::Rational q{s};q.canonicalize();a.set(row,col,q);}
            arrows.push_back({forward,std::move(a)});
        }
        if(!std::cin)throw std::runtime_error("incomplete consumer input");
        numeric::ExactActionGuard guard({100000,10000000,100000000,1000000});
        topology::PreparedRationalZigzag diagram(dims,std::move(arrows),guard);
        std::cout<<diagram.intervals().size()<<'\n';
        for(const auto& q:diagram.intervals()){
            std::cout<<q.begin<<' '<<q.end<<' '<<q.generators.front().cols()<<'\n';
            for(const auto& frame:q.generators)for(std::size_t i=0;i<frame.rows();++i)for(std::size_t j=0;j<frame.cols();++j)std::cout<<frame.get(i,j)<<' ';
            std::cout<<'\n';
        }
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
