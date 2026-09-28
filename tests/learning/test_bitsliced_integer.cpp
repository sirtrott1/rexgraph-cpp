#include "rfc/learning/bitsliced_affine.hpp"
#include "rfc/learning/bitsliced_threshold.hpp"
#include "rfc/machine/bitsliced_int.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        using Word = rfc::machine::BitSlicedInt<8>;
        const std::vector<std::int64_t> lhs_values{-7,-3,-1,0,1,2,5,17};
        const std::vector<std::int64_t> rhs_values{2,-4,1,0,-1,3,6,-5};
        const auto lhs = Word::from_values(lhs_values);
        const auto rhs = Word::from_values(rhs_values);
        const auto sum = lhs.add_mod(rhs);
        for (std::size_t i = 0; i < lhs_values.size(); ++i) {
            require(sum.value(i) == lhs_values[i] + rhs_values[i], "bit-sliced addition mismatch");
        }
        require(lhs.signed_overflow(rhs, sum).none(), "unexpected bit-sliced overflow");

        const auto diff = lhs.sub_mod(rhs);
        for (std::size_t i = 0; i < lhs_values.size(); ++i) {
            require(diff.value(i) == lhs_values[i] - rhs_values[i], "bit-sliced subtraction mismatch");
        }

        const auto ge_zero = lhs.greater_equal(0);
        for (std::size_t i = 0; i < lhs_values.size(); ++i) {
            require(ge_zero.get(i) == (lhs_values[i] >= 0), "bit-sliced signed comparison mismatch");
        }

        const auto high = Word::from_values({120});
        const auto ten = Word::from_values({10});
        const auto wrapped = high.add_mod(ten);
        require(high.signed_overflow(ten, wrapped).get(0), "signed overflow was not detected");

        const auto multiplied = lhs.mul_constant_mod(-3);
        for (std::size_t i = 0; i < lhs_values.size(); ++i) {
            require(multiplied.value(i) == lhs_values[i] * -3, "bit-sliced scalar multiplication mismatch");
        }

        rfc::learning::BitSlicedAffineUnit<16> affine({2,-3}, 1);
        const auto x0 = rfc::machine::BitSlicedInt<16>::from_values({1,2,-1,3});
        const auto x1 = rfc::machine::BitSlicedInt<16>::from_values({4,-2,2,0});
        const auto affine_out = affine.forward({x0,x1});
        const std::vector<std::int64_t> expected_affine{-9,11,-7,7};
        for (std::size_t i = 0; i < expected_affine.size(); ++i) {
            require(affine_out.value(i) == expected_affine[i], "bit-sliced affine unit mismatch");
        }

        // All 8 Boolean assignments for three inputs, packed as lanes.
        rfc::machine::PackedBits a(8), b(8), c(8), target(8);
        for (std::size_t lane = 0; lane < 8; ++lane) {
            const bool av = (lane & 4u) != 0;
            const bool bv = (lane & 2u) != 0;
            const bool cv = (lane & 1u) != 0;
            a.set(lane, av); b.set(lane, bv); c.set(lane, cv);
            target.set(lane, static_cast<unsigned>(av) + static_cast<unsigned>(bv) + static_cast<unsigned>(cv) >= 2u);
        }
        const std::vector<rfc::machine::PackedBits> features{a,b,c};
        const auto fit = rfc::learning::fit_small_threshold_unit<8>(features, target, -2, 2, -3, 4);
        require(fit.hamming_error == 0u, "integer threshold unit did not fit majority exactly");
        rfc::learning::BitSlicedThresholdUnit<8> unit(fit.weights, fit.threshold);
        require((unit.forward(features) ^ target).none(), "fitted integer threshold inference mismatch");

        std::cout << "bit-sliced integer learning tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
