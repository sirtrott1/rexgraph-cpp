#pragma once
#include "declared_local_field_data.hpp"
namespace prepared_binary_source_example {
using namespace declared_field_example;
inline Fixture make() {
    auto f=declared_field_example::make();auto& d=*f.endpoint.declarations;
    auto definitions=d.registry.definitions();
    definitions.push_back(Declaration::product(id(904),{{11,id(900),0},{12,id(900),0},{13,id(900),0}}));
    d.registry=DeclarationRegistry(std::move(definitions));
    RelationalCodeState codes;codes.namespace_key=777;
    codes.types={{900,RelationalCodeTypeKind::Finite,{0,1},{}},
                 {901,RelationalCodeTypeKind::Product,{},{{1,900,0,"exists"},{2,900,0,"gate"}}},
                 {904,RelationalCodeTypeKind::Product,{},{{11,900,0,{}},{12,900,0,{}},{13,900,0,{}}}}};
    BinaryProgram program(6);
    for(const auto& ins:d.programs.front().program.instructions())program.append(ins);
    program.append({BinaryOpcode::Not,3,1});program.append({BinaryOpcode::And,4,0,3});
    program.append({BinaryOpcode::LoadAspect,5,0,0,0,0}); // keep this unused source read
    RelationalCodeOperation op;op.key=166;op.input=901;op.output=904;op.opcode=RelationalCodeOpcode::Binary;
    op.binary=program;op.registers={2,4,2};codes.operations.push_back(op);
    codes.fields={999};
    RelationalCodeSection output;output.key=1;output.field=999;output.type=904;
    output.occurrence=StableOccurrenceKey{g1,core::OccurrenceKey{1003}};
    output.value=RelationalCodeValue{RelationalCodeValue::Product{{std::uint64_t{1}},{std::uint64_t{0}},{std::uint64_t{1}}}};
    codes.sections.push_back(output);f.endpoint.codes=codes;
    d.code_types={{900,id(900)},{901,id(901)},{904,id(904)}};
    validate_temporal_endpoint(f.endpoint);return f;
}
} // namespace prepared_binary_source_example
