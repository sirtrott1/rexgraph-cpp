#include "rfc/database/native_execution.hpp"
#include "rfc/core/builder.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
struct Counts {std::size_t cpp_calls{},cpp_bytes{},gmp_calls{},gmp_bytes{},gmp_reallocations{},gmp_reallocation_bytes{};};
Counts counts;
bool accounting=false;
void record_cpp(std::size_t size) {if(accounting){++counts.cpp_calls;counts.cpp_bytes+=size;}}
// Keep the replacement allocator's release hook out of line. GCC otherwise
// diagnoses the valid malloc-backed replacement delete after inlining it into
// standard-library containers as a mismatched new/free pair.
[[gnu::noinline]] void release_cpp(void* p) noexcept {std::free(p);}
void* gmp_allocate(std::size_t size) {
    if(accounting){++counts.gmp_calls;counts.gmp_bytes+=size;}
    if(auto* p=std::malloc(size?size:1))return p;
    std::abort();
}
void* gmp_reallocate(void* p,std::size_t,std::size_t size) {
    if(accounting){++counts.gmp_reallocations;counts.gmp_reallocation_bytes+=size;}
    if(auto* q=std::realloc(p,size?size:1))return q;
    std::abort();
}
void gmp_free(void* p,std::size_t) {std::free(p);}
}
void* operator new(std::size_t size) {
    record_cpp(size);if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* p) noexcept {release_cpp(p);}
void operator delete[](void* p) noexcept {release_cpp(p);}
void operator delete(void* p,std::size_t) noexcept {release_cpp(p);}
void operator delete[](void* p,std::size_t) noexcept {release_cpp(p);}
void* operator new(std::size_t size,std::align_val_t alignment) {
    record_cpp(size);void* p=nullptr;
    if(posix_memalign(&p,static_cast<std::size_t>(alignment),size?size:1)==0)return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size,std::align_val_t alignment) {return ::operator new(size,alignment);}
void operator delete(void* p,std::align_val_t) noexcept {release_cpp(p);}
void operator delete[](void* p,std::align_val_t) noexcept {release_cpp(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept {release_cpp(p);}
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept {release_cpp(p);}

namespace {
using namespace rfc;
using namespace rfc::storage;
TemporalEndpoint fan(std::size_t arity) {
    core::RelationSignature signature;auto type=signature.add_type("retained-fan",core::BoundaryPolicy::DeclaredExact,0,100000);
    core::RelationalComplexBuilder b(signature);std::vector<core::BoundaryOccurrenceInput> legs;
    for(std::size_t i=0;i<arity;++i)legs.push_back({10,i%7!=6,i%3==1,numeric::Rational{1,3},{},core::OccurrenceKey{1000+i}});
    b.add_grade_one(core::RelationKey{100},type,legs);TemporalEndpoint e(b.freeze());
    e.carriers={{10,TemporalCarrierKind::CellStalk,core::Grade{0},StableCellKey{core::Grade{0},10},900,numeric::IndexDomain::linear(1)},
                {100,TemporalCarrierKind::CellStalk,core::Grade{1},StableCellKey{core::Grade{1},100},900,numeric::IndexDomain::linear(1)}};
    TemporalTensor field;field.key=200;field.role=TemporalTensorRole::Field;field.output=100;field.owner.kind=TemporalOwnerKind::Cell;
    field.owner.cell={core::Grade{1},100};field.interpretation="exact fan profiling input";
    field.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,numeric::Rational{7,13}}})};e.tensors={field};
    return e;
}
void print(std::size_t arity,const char* phase,const Counts& c,long long ns,const runtime::ExecutionRequirements& r) {
    std::cout<<"{\"arity\":"<<arity<<",\"phase\":\""<<phase<<"\",\"nanoseconds\":"<<ns
        <<",\"cpp_allocation_calls\":"<<c.cpp_calls<<",\"cpp_requested_bytes\":"<<c.cpp_bytes
        <<",\"gmp_allocation_calls\":"<<c.gmp_calls<<",\"gmp_requested_bytes\":"<<c.gmp_bytes
        <<",\"gmp_reallocation_calls\":"<<c.gmp_reallocations<<",\"gmp_reallocation_requested_bytes\":"<<c.gmp_reallocation_bytes
        <<",\"rational_slots_bound\":"<<r.rational_slots<<",\"sparse_entries_bound\":"<<r.sparse_entries
        <<",\"scalar_work_bound\":"<<r.scalar_work<<",\"integer_bits_bound\":"<<r.integer_bits<<"}\n";
}
}
int main() {
    // Install hooks before constructing any GMP value. This isolated process
    // reports requested allocation totals, not peak/resident heap bytes.
    mp_set_memory_functions(gmp_allocate,gmp_reallocate,gmp_free);
    for(const std::size_t arity:{16u,256u,4096u}) {
        const auto e=fan(arity);NativeAccessionDirectory directory(e,77);
        NativeAccessionRequest field;field.kind=NativeAccessionKind::Field;field.subject=200;const auto pin=directory.pin(field);
        NativeFieldRequest request;request.stalk_carriers={10,100};request.fields={pin};
        counts={};accounting=true;const auto start=std::chrono::steady_clock::now();
        const auto operation=database::prepare_field_action(pin,request,NativeFieldRealization::constant_fiber(900));
        const auto prepared=std::chrono::steady_clock::now();accounting=false;const auto preparation=counts;
        print(arity,"prepare",preparation,std::chrono::duration_cast<std::chrono::nanoseconds>(prepared-start).count(),operation.report().requirements);
        counts={};accounting=true;const auto run=std::chrono::steady_clock::now();
        const auto result=database::execute(operation);
        const auto done=std::chrono::steady_clock::now();accounting=false;const auto application=counts;
        if(result.carriers().size()!=1 || operation.binding().occurrences().size()!=arity)return 1;
        long signed_count=0;
        for(std::size_t i=0;i<arity;++i)if(i%7!=6)signed_count+=i%3==1?-1:1;
        numeric::Rational expected{signed_count*7,39};expected.canonicalize();
        if(result.state({core::Grade{0},10},0)!=expected)return 1;
        print(arity,"execute",application,std::chrono::duration_cast<std::chrono::nanoseconds>(done-run).count(),operation.report().requirements);
    }
}
