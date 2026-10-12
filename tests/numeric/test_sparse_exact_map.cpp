#include <rfc/numeric/prepared_sparse_map.hpp>
#include <iostream>
#include <random>

using namespace rfc::numeric;
void require(bool value) { if (!value) throw std::logic_error("sparse map check"); }
template<class Fn> void refuse(Fn action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected);
}
void matrix_json(const ExactMatrix& matrix) {
    std::cout << '[';
    for (std::size_t i=0; i<matrix.rows(); ++i) {
        if (i) std::cout << ',';
        std::cout << '[';
        for (std::size_t j=0; j<matrix.cols(); ++j) {
            if (j) std::cout << ',';
            std::cout << '"' << matrix.get(i,j) << '"';
        }
        std::cout << ']';
    }
    std::cout << ']';
}
int main() {
    std::mt19937 random(90231);
    for (std::size_t trial=0; trial<300; ++trial) {
        const std::size_t n=trial%12, m=trial%9, rows=1+trial%7, columns=1+trial%5;
        ExactMatrix a(rows,n), b(columns,m);
        for (std::size_t i=0; i<rows; ++i)
            for (std::size_t k=0; k<n; ++k)
                a.set(i,k,Rational(static_cast<int>(random()%7)-3,11));
        for (std::size_t j=0; j<columns; ++j)
            for (std::size_t l=0; l<m; ++l)
                b.set(j,l,Rational(static_cast<int>(random()%7)-3,7));
        std::vector<SparseRationalTensor> fields;
        for (std::size_t l=0; l<m; ++l) {
            std::vector<std::size_t> active;
            std::vector<SparseRationalEntry> values;
            for (std::size_t k=0; k<n; ++k) {
                if (trial%3==0 && random()%5==0) continue;
                active.push_back(k);
                const Rational value(static_cast<int>(random()%9)-4,5);
                if (value!=0) values.push_back({k,value});
            }
            auto support=trial%3==0 ? (trial%2 ? StructuralSupport::packed(n,active) : StructuralSupport::sparse(n,active))
                                    : StructuralSupport::full(n);
            fields.emplace_back(IndexDomain({n}),std::move(support),SparseRationalVector(n,std::move(values)));
        }
        const PreparedSparseExactMap left(a), right(b);
        auto sum=right.field_accumulator(rows), even=right.field_accumulator(rows), odd=right.field_accumulator(rows);
        for (std::size_t l=0; l<m; ++l) {
            require(left.apply(fields[l].values()).dense()==a.apply(fields[l].values().dense()));
            require(a.apply_sparse(fields[l].values().view()).dense()==a.apply(fields[l].values().dense()));
            auto mapped=left.apply(fields[l]);
            sum.add_column(l,mapped);
            (l%2 ? odd : even).add_column(l,mapped);
        }
        even.merge(odd);
        std::vector<SparseRationalTensor> output;
        for (std::size_t j=0; j<columns; ++j) {
            output.push_back(sum.finish_column(j));
            const auto parallel=even.finish_column(j);
            for (std::size_t i=0; i<rows; ++i) require(parallel.state(i)==output.back().state(i));
        }
        std::cout << "{\"n\":" << n << ",\"m\":" << m << ",\"a\":";
        matrix_json(a); std::cout << ",\"b\":"; matrix_json(b);
        std::cout << ",\"fields\":[";
        for (std::size_t l=0; l<m; ++l) {
            if (l) std::cout << ',';
            std::cout << '[';
            for (std::size_t k=0; k<n; ++k) {
                if (k) std::cout << ',';
                const auto value=fields[l].state(k);
                if (value) std::cout << '"' << *value << '"'; else std::cout << "null";
            }
            std::cout << ']';
        }
        std::cout << "],\"output\":[";
        for (std::size_t i=0; i<rows; ++i) {
            if (i) std::cout << ',';
            std::cout << '[';
            for (std::size_t j=0; j<columns; ++j) {
                if (j) std::cout << ',';
                const auto value=output[j].state(i);
                if (value) std::cout << '"' << *value << '"'; else std::cout << "null";
            }
            std::cout << ']';
        }
        std::cout << "]}\n";
    }
    const std::size_t huge=std::size_t{1}<<33;
    ExactMatrix tiny(2,huge);tiny.set(0,huge-1,Rational{7,3});
    const PreparedSparseExactMap map(tiny);
    const SparseRationalTensor input(IndexDomain({huge}),StructuralSupport::full(huge),
                                     SparseRationalVector(huge,{{huge-1,Rational{3,7}}}));
    require(map.apply(input).state(0)==Rational{1});require(map.apply(input).state(1)==Rational{0});
    std::vector<SparseRationalEntry> bad{{1,1},{0,1}};
    refuse([&]{(void)map.apply(SparseRationalView(huge,bad));});
    const PreparedSparseExactMap identity(ExactMatrix::identity(2));
    auto incomplete=identity.field_accumulator(huge);
    refuse([&]{(void)incomplete.finish_column(0);});
    incomplete.add_column(0,input);
    require(incomplete.finish_column(0).state(huge-1)==Rational{3,7});
    refuse([&]{incomplete.add_column(0,input);});
    refuse([&]{(void)incomplete.finish_column(0);});
    auto failed=identity.field_accumulator(huge);
    refuse([&]{failed.add_column(0,input,[]{throw std::runtime_error("stop");});});
    refuse([&]{(void)failed.finish_column(0);});
    const PreparedSparseExactMap independent(ExactMatrix::identity(2));
    auto owner=identity.field_accumulator(huge);
    refuse([&]{owner.merge(independent.field_accumulator(huge));});
    ExactMatrix cancellation(1,2);cancellation.set(0,0,1);cancellation.set(0,1,-1);
    const PreparedSparseExactMap cancel(cancellation);
    const SparseRationalTensor supported(IndexDomain({2}),StructuralSupport::full(2),SparseRationalVector(2,{{0,1},{1,1}}));
    require(cancel.apply(supported).state(0)==Rational{0});
    const SparseRationalTensor missing(IndexDomain({2}),StructuralSupport::sparse(2,{0}),SparseRationalVector(2,{{0,1}}));
    require(!cancel.apply(missing).state(0));
    std::cerr << "300 sparse map cases and ownership/domain/cancellation checks PASS\n";
}
