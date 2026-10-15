#include "rfc/numeric/smith_normal_form.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using rfc::numeric::Integer;
using rfc::numeric::IntegerMatrix;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static Integer determinant_minor(const IntegerMatrix& matrix,
                                 const std::vector<std::size_t>& rows,
                                 const std::vector<std::size_t>& cols) {
    // Independent small reference: the signed permutation definition, not
    // elimination or the Smith transformation algorithm under test.
    auto permutation = cols;
    Integer sum{0};
    do {
        Integer term{1};
        std::size_t inversions = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            term *= matrix.get(rows[i], permutation[i]);
            for (std::size_t j = i + 1; j < rows.size(); ++j)
                if (permutation[i] > permutation[j]) ++inversions;
        }
        if (inversions % 2u) sum -= term;
        else sum += term;
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    return sum;
}

static std::vector<std::vector<std::size_t>> selections(std::size_t n, std::size_t k) {
    std::vector<std::vector<std::size_t>> out;
    for (unsigned mask = 0; mask < (1u << n); ++mask) {
        if (std::popcount(mask) != static_cast<int>(k)) continue;
        std::vector<std::size_t> selected;
        for (std::size_t i = 0; i < n; ++i) if (mask & (1u << i)) selected.push_back(i);
        out.push_back(std::move(selected));
    }
    return out;
}

static std::vector<Integer> reference_invariants(const IntegerMatrix& matrix) {
    require(matrix.rows() <= 4 && matrix.cols() <= 4, "determinantal-divisor reference is only for small matrices");
    std::vector<Integer> invariants;
    Integer previous{1};
    for (std::size_t k = 1; k <= std::min(matrix.rows(), matrix.cols()); ++k) {
        Integer divisor{0};
        for (const auto& rows : selections(matrix.rows(), k)) {
            for (const auto& cols : selections(matrix.cols(), k)) {
                const auto determinant = determinant_minor(matrix, rows, cols);
                mpz_gcd(divisor.get_mpz_t(), divisor.get_mpz_t(), determinant.get_mpz_t());
            }
        }
        if (divisor == 0) break;
        require(divisor % previous == 0, "reference determinantal divisors are inconsistent");
        invariants.push_back(divisor / previous);
        previous = divisor;
    }
    return invariants;
}

static void check_reference(const IntegerMatrix& matrix) {
    const auto expected = reference_invariants(matrix);
    const auto actions = rfc::numeric::smith_normal_form_actions(matrix);
    const auto full = rfc::numeric::smith_normal_form(matrix);
    rfc::numeric::ExactActionGuard guard;
    const auto guarded=rfc::numeric::smith_normal_form_actions(matrix,guard);
    require(actions.invariant_factors == expected && full.invariant_factors == expected,
            "Smith invariants disagree with gcds of exact minors");
    require(guarded.invariant_factors==expected&&guarded.diagonal==actions.diagonal,
            "guarded Smith elimination differs from independent exact minors");
    require(guarded.right_inverse.apply_inverse_on_right(guarded.left.apply(matrix,guard),guard)==guarded.diagonal,
            "guarded Smith replay lost the exact action certificate");
    rfc::numeric::ExactMatrix fractional(matrix.rows(),matrix.cols()),expected_fractional(matrix.rows(),matrix.cols());
    for(const auto& [ij,value]:matrix.entries())fractional.set(ij.first,ij.second,rfc::numeric::Rational{value}/rfc::numeric::Rational{3});
    const auto integral_image=actions.left.apply(matrix);
    for(const auto& [ij,value]:integral_image.entries())expected_fractional.set(ij.first,ij.second,rfc::numeric::Rational{value}/rfc::numeric::Rational{3});
    const auto fractional_image=guarded.left.apply_rational(fractional,guard);
    require(fractional_image==expected_fractional&&guarded.left.apply_inverse_rational(fractional_image,guard)==fractional,
            "guarded direct rational replay changed the integral action or required a complete inverse");
    require(actions.rank == expected.size() && full.rank == expected.size(), "Smith rank disagrees with exact minors");
    require(full.left * matrix * full.right == full.diagonal, "complete Smith factorization certificate failed");
    require(actions.diagonal == full.diagonal, "action and complete Smith diagonal disagree");
    require(actions.left.apply(matrix) == full.left * matrix, "left Smith action disagrees with its matrix");
    require(actions.left.apply_inverse(actions.left.apply(matrix)) == matrix, "left Smith inverse action did not round-trip");
    require(actions.right_inverse.apply(full.right) == IntegerMatrix::identity(matrix.cols()),
            "right Smith action is not the inverse basis change");
    std::vector<Integer> vector(matrix.rows(),Integer{0});
    IntegerMatrix::SparseVector sparse_vector;
    for (std::size_t i = 0; i < matrix.rows(); ++i) {
        vector[i] = Integer{static_cast<unsigned long>(i + 1u)};
        sparse_vector.emplace(i,vector[i]);
    }
    const auto expected_vector = full.left.apply(vector);
    const auto sparse_image = actions.left.apply_sparse(sparse_vector);
    require(actions.left.apply(vector) == expected_vector, "Smith vector action disagrees with complete U");
    for (std::size_t i = 0; i < expected_vector.size(); ++i) {
        const auto it = sparse_image.find(i);
        const Integer value = it == sparse_image.end() ? Integer{0} : it->second;
        require(value == expected_vector[i], "sparse Smith vector action disagrees with complete U");
    }
    require(actions.left.apply_inverse(expected_vector) == vector &&
            actions.left.apply_inverse_sparse(sparse_image) == sparse_vector,
            "Smith vector inverse action did not recover its original coordinates");
    require(guarded.left.apply_sparse(sparse_vector,guard)==sparse_image&&
            guarded.left.apply_inverse_sparse(sparse_image,guard)==sparse_vector,
            "guarded sparse unimodular replay differs from the complete exact reference");

    IntegerMatrix rhs(matrix.cols(), 2);
    for (std::size_t i = 0; i < matrix.cols(); ++i) rhs.set(i, i % 2u, Integer{static_cast<unsigned long>(i + 1u)});
    require(actions.right_inverse.apply_inverse(actions.right_inverse.apply(rhs)) == rhs,
            "selected boundary coordinates did not round-trip through Smith actions");
    for (std::size_t begin = 0; begin <= matrix.cols(); ++begin) {
        const auto selected = actions.right_columns(begin, matrix.cols() - begin);
        require(guarded.right_columns(begin,matrix.cols()-begin,guard)==selected,
                "guarded selected Smith columns differ from the complete basis");
        for (std::size_t i = 0; i < selected.rows(); ++i)
            for (std::size_t j = 0; j < selected.cols(); ++j)
                require(selected.get(i,j) == full.right.get(i, begin+j), "selected Smith columns disagree with full V");
    }
    const auto row_indices = selections(matrix.rows(), matrix.rows()).front();
    const auto col_indices = selections(matrix.cols(), matrix.cols()).front();
    require(rfc::numeric::abs_integer(determinant_minor(full.left, row_indices, row_indices)) == 1 &&
            rfc::numeric::abs_integer(determinant_minor(full.right, col_indices, col_indices)) == 1,
            "Smith basis matrices are not unimodular");
}

static void guarded_refusal_tests(){
    using Guard=rfc::numeric::ExactActionGuard;
    using Limits=rfc::numeric::ExactActionLimits;
    IntegerMatrix work(2,2);work.set(0,0,Integer{13});work.set(0,1,Integer{21});
    work.set(1,0,Integer{34});work.set(1,1,Integer{55});
    struct Cancelled {};
    std::size_t checkpoints=0;Guard cancel(Limits{},[&]{if(++checkpoints==40)throw Cancelled{};});
    bool refused=false;try{(void)rfc::numeric::smith_normal_form_actions(work,cancel);}catch(const Cancelled&){refused=true;}
    require(refused&&checkpoints==40,"guarded Smith did not propagate cancellation inside elimination");
    Limits step_limits;step_limits.steps=30;Guard steps(step_limits);refused=false;
    try{(void)rfc::numeric::smith_normal_form_actions(work,steps);}catch(const std::length_error&){refused=true;}
    require(refused&&steps.steps()==30,"guarded Smith did not enforce its internal step budget");
    IntegerMatrix growth(2,2);growth.set(0,0,Integer{7});growth.set(0,1,Integer{6});
    growth.set(1,0,Integer{6});growth.set(1,1,Integer{-7});
    Limits bit_limits;bit_limits.integer_bits=5;Guard bits(bit_limits);refused=false;
    try{(void)rfc::numeric::smith_normal_form_actions(growth,bits);}catch(const std::length_error&){refused=true;}
    require(refused,"guarded Smith admitted intermediate coefficient growth beyond its bit budget");
    rfc::numeric::IntegerUnimodularAction action(2);action.add_row_multiple(0,1,Integer{7});
    IntegerMatrix column(2,1);column.set(0,0,Integer{7});column.set(1,0,Integer{7});
    Guard replay_bits(bit_limits);refused=false;
    try{(void)action.apply(column,replay_bits);}catch(const std::length_error&){refused=true;}
    require(refused,"guarded unimodular replay skipped an intermediate multiplication bit check");
    checkpoints=0;Guard replay_cancel(Limits{},[&]{if(++checkpoints==10)throw Cancelled{};});refused=false;
    try{(void)action.apply_sparse({{0,Integer{1}},{1,Integer{1}}},replay_cancel);}catch(const Cancelled&){refused=true;}
    require(refused&&checkpoints==10,"guarded sparse replay did not propagate cancellation");

    const auto huge=std::numeric_limits<std::size_t>::max();
    Limits sparse_limits;sparse_limits.dimension=huge;sparse_limits.entries=4096;sparse_limits.steps=50000;
    Guard sparse(sparse_limits);IntegerMatrix enormous(huge,huge);
    enormous.set(3,5,Integer{6});enormous.set(3,huge-2,Integer{10});
    enormous.set(huge-1,5,Integer{4});enormous.set(huge-1,huge-2,Integer{14});
    const auto smith=rfc::numeric::smith_normal_form_actions(enormous,sparse);
    require(smith.invariant_factors==std::vector<Integer>({Integer{2},Integer{22}})&&smith.diagonal.stored_entries()==2,
            "guarded sparse Smith allocated an ambient domain or changed exact invariants");
    const auto frame=smith.right_columns(0,2,sparse);
    const auto image=smith.left.apply(enormous.multiply(frame,sparse),sparse);
    require(image.stored_entries()==2&&image.get(0,0)==2&&image.get(1,1)==22,
            "guarded huge-domain selected Smith action failed");
}

static void sparse_domain_tests() {
    const auto huge = std::numeric_limits<std::size_t>::max();
    IntegerMatrix a(huge, 3), b(3, huge);
    a.set(huge-1,0,Integer{6}); a.set(7,1,Integer{4}); a.set(7,2,Integer{-4});
    b.set(0,7,Integer{2}); b.set(1,huge-1,Integer{3}); b.set(2,huge-1,Integer{3});
    auto product = a * b;
    require(product.rows() == huge && product.cols() == huge && product.stored_entries() == 1 &&
            product.get(huge-1,7) == 12, "huge sparse integer product lost cancellation or formed an ambient rectangle");
    require(product.apply_sparse({{7,Integer{5}}}) == IntegerMatrix::SparseVector{{huge-1,Integer{60}}},
            "huge sparse integer vector action failed");
    require(product.transpose().get(7,huge-1) == 12, "huge sparse transpose failed");
    product.swap_rows(0,huge-1); product.swap_cols(0,7);
    product.row_add_multiple(huge-1,0,Integer{2});
    product.col_add_multiple(huge-1,0,Integer{-1});
    require(product.stored_entries() == 4 && product.get(0,0) == 12 && product.get(huge-1,huge-1) == -24,
            "sparse elementary operations changed integer coordinates");
    product.row_add_multiple(huge-1,0,Integer{-2});
    product.col_scale(huge-1,Integer{0});
    product.row_add_multiple(0,0,Integer{-1});
    require(product.stored_entries() == 0, "sparse elementary cancellation retained zeros");
    product.at(0,0) += 1; product.at(0,0) -= 1;
    require(product == IntegerMatrix(huge,huge) && product.nonzeros() == 0, "legacy mutable coordinate zeros changed matrix equality");
    product.prune_zeros();
    require(product.stored_entries() == 0, "legacy mutable zero could not be compacted");
    bool rejected = false;
    try { product.set(huge,0,Integer{1}); } catch (const std::out_of_range&) { rejected = true; }
    require(rejected,"sparse integer matrix lost logical bounds checks");
    rejected = false;
    try { (void)product.apply_sparse({{huge,Integer{1}}}); } catch (const std::out_of_range&) { rejected = true; }
    require(rejected,"sparse integer action accepted a foreign coordinate");

    IntegerMatrix enormous(huge, huge);
    enormous.set(3,5,Integer{6}); enormous.set(3,huge-2,Integer{10});
    enormous.set(huge-1,5,Integer{4}); enormous.set(huge-1,huge-2,Integer{14});
    const auto smith = rfc::numeric::smith_normal_form_actions(enormous);
    require(smith.rank == 2 && smith.invariant_factors == std::vector<Integer>{Integer{2},Integer{22}} &&
            smith.diagonal.stored_entries() == 2, "huge sparse Smith factorization scanned or allocated the ambient domain");
    require(smith.right_inverse.apply_inverse_on_right(smith.left.apply(enormous)) == smith.diagonal,
            "huge Smith action certificate failed");
    const auto selected = smith.right_columns(0,2);
    IntegerMatrix expected(huge,2);
    expected.set(0,0,Integer{2}); expected.set(1,1,Integer{22});
    require(smith.left.apply(enormous * selected) == expected, "huge selected Smith frame has the wrong boundary action");
    const IntegerMatrix::SparseVector vector{{3,Integer{7}},{huge-1,Integer{-3}}};
    require(smith.left.apply_inverse_sparse(smith.left.apply_sparse(vector)) == vector,
            "huge sparse unimodular vector action did not round-trip");
    const auto zero = rfc::numeric::smith_normal_form_actions(IntegerMatrix(huge,huge));
    require(zero.rank == 0 && zero.left.operations().empty() && zero.right_inverse.operations().empty(),
            "zero Smith action created ambient identity operations");
    const auto last = zero.right_columns(huge-1,1);
    require(last.stored_entries() == 1 && last.get(huge-1,0) == 1, "huge identity action did not select its requested column");
}

int main() {
    try {
        sparse_domain_tests();
        guarded_refusal_tests();
        std::size_t cases = 0;
        for (long a = -2; a <= 2; ++a) for (long b = -2; b <= 2; ++b)
            for (long c = -2; c <= 2; ++c) for (long d = -2; d <= 2; ++d) {
                IntegerMatrix matrix(2,2);
                matrix.set(0,0,Integer{a}); matrix.set(0,1,Integer{b});
                matrix.set(1,0,Integer{c}); matrix.set(1,1,Integer{d});
                check_reference(matrix); ++cases;
            }
        std::uint64_t state = 0x534d495448u;
        for (std::size_t rows = 0; rows <= 4; ++rows) for (std::size_t cols = 0; cols <= 4; ++cols) {
            for (std::size_t sample = 0; sample < 6; ++sample) {
                IntegerMatrix matrix(rows,cols);
                for (std::size_t i = 0; i < rows; ++i) for (std::size_t j = 0; j < cols; ++j) {
                    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
                    const long value = static_cast<long>((state >> 32u) % 15u) - 7;
                    matrix.set(i,j,Integer{value});
                }
                if (sample == 0 && rows > 1) matrix.row_add_multiple(1,1,Integer{-1});
                if (sample == 1 && cols > 1) matrix.col_add_multiple(1,1,Integer{-1});
                check_reference(matrix); ++cases;
            }
        }
        std::cout << "sparse integer/Smith actions passed " << cases << " exact determinantal-divisor references\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
