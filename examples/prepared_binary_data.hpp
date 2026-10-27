#pragma once
#include "rfc/storage/prepared_relational_binary.hpp"
namespace prepared_binary_example {
using namespace rfc;
using namespace rfc::storage;
using namespace rfc::composite;
inline RelationalCodeState mux() {
    RelationalCodeState s;s.namespace_key=0x4558454331;
    s.types={{1,RelationalCodeTypeKind::Finite,{0,1},{}},
             {2,RelationalCodeTypeKind::Product,{},{}},
             {3,RelationalCodeTypeKind::Product,{},{}}};
    for(std::uint64_t i=0;i<16;++i)s.types[1].factors.push_back({100+i,1,0,{}});
    for(std::uint64_t i=0;i<8;++i)s.types[2].factors.push_back({200+i,1,0,{}});
    BinaryProgram program(32);
    for(std::uint16_t i=0;i<16;++i)program.append({BinaryOpcode::LoadAspect,i,0,0,0,i});
    for(std::uint16_t i=0;i<8;++i){
        program.append({(0xA5u>>i)&1u?BinaryOpcode::LoadOne:BinaryOpcode::LoadZero,static_cast<std::uint16_t>(16+i)});
        program.append({BinaryOpcode::Select,static_cast<std::uint16_t>(24+i),static_cast<std::uint16_t>(16+i),i,static_cast<std::uint16_t>(8+i)});
    }
    RelationalCodeOperation op;op.key=166;op.input=2;op.output=3;op.opcode=RelationalCodeOpcode::Binary;op.binary=program;
    for(std::uint16_t i=0;i<8;++i)op.registers.push_back(24+i);
    s.operations.push_back(op);return s;
}
inline std::vector<machine::PackedBits> byte_pairs() {
    std::vector<machine::PackedBits> columns(16,machine::PackedBits(65536));
    for(std::size_t row=0;row<65536;++row)for(std::size_t bit=0;bit<8;++bit){
        columns[bit].set(row,((row/256)>>bit)&1);
        columns[8+bit].set(row,((row%256)>>bit)&1);
    }
    return columns;
}
} // namespace prepared_binary_example
