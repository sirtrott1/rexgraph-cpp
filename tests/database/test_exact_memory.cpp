#include "rfc/core/builder.hpp"
#include "rfc/database/accession.hpp"
#include "rfc/database/store.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        rfc::core::RelationSignature signature;
        const auto fact_type = signature.add_type("fact", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
        const auto other_type = signature.add_type("other", rfc::core::BoundaryPolicy::DeclaredExact, 2, 2);
        rfc::core::RelationalComplexBuilder builder(signature);
        builder.add_grade_one(fact_type, {{10,true,true,false,1},{11,true,false,false,1}});
        builder.add_grade_one(other_type, {{11,true,true,false,1},{12,true,false,false,1}});
        const auto complex = builder.freeze();

        rfc::database::ExactRelationalMemory memory(complex);
        memory.assert_exact({rfc::core::Grade{1},0}, {rfc::numeric::rational(7,3), {41,42}});
        const auto stored = memory.lookup({rfc::core::Grade{1},0});
        if (!stored || stored->value != rfc::numeric::rational(7,3)) throw std::runtime_error("exact memory lookup failed");
        if (stored->provenance.size() != 2u) throw std::runtime_error("provenance retention failed");

        const auto mask = rfc::database::select_relation_type(complex, rfc::core::Grade{1}, fact_type);
        if (mask.count() != 1u || !mask.get(0)) throw std::runtime_error("relation-type accession failed");

        std::cout << "exact database tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
