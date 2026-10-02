#pragma once

#include "rfc/numeric/checked_exact_actions.hpp"
#include "rfc/numeric/index_domain.hpp"
#include <set>
#include <optional>

namespace rfc::local {
struct LinearSolveDeclaration {
    std::uint64_t input_carrier{},output_carrier{},input_coordinates{},output_coordinates{};
    numeric::IndexDomain input_domain,output_domain;
};
// Conditional exact block elimination for a declared square operator. The
// partition is physical only; it neither infers a chain boundary nor a metric.
class PreparedInteriorInterfaceSolve {
    struct State {
        LinearSolveDeclaration declaration;
        numeric::ExactMatrix original,cross,interior_to_interface,schur;
        std::vector<std::size_t> interior,interface;
        std::optional<numeric::PreparedExactSolve> interior_solve,interface_solve;
        State(LinearSolveDeclaration d,numeric::ExactMatrix a,std::vector<std::size_t> indices,
            numeric::ExactActionLimits limits,const std::function<void()>& check)
            :declaration(std::move(d)),original(std::move(a)),interior(std::move(indices)) {
            numeric::ExactActionGuard guard(limits,check);
            interior_solve.emplace(prepare_interior(guard),guard);
            interface_solve.emplace(prepare_interface(guard),guard);
            guard.entries(retained_entries());
        }
        numeric::ExactMatrix block(const std::vector<std::size_t>& rows,const std::vector<std::size_t>& cols) const {
            numeric::ExactMatrix out(rows.size(),cols.size());
            std::map<std::size_t,std::size_t> row_index,col_index;
            for(std::size_t i=0;i<rows.size();++i)row_index.emplace(rows[i],i);
            for(std::size_t j=0;j<cols.size();++j)col_index.emplace(cols[j],j);
            for(const auto& [key,q]:original.entries())if(row_index.contains(key.first) && col_index.contains(key.second))
                out.set(row_index.at(key.first),col_index.at(key.second),q);
            return out;
        }
        numeric::ExactMatrix prepare_interior(numeric::ExactActionGuard& guard) {
            guard.matrix(original);
            if(!declaration.input_carrier || !declaration.output_carrier || !declaration.input_coordinates || !declaration.output_coordinates)
                throw std::invalid_argument("solve requires declared carriers and coordinates");
            if(original.rows()!=original.cols() || declaration.input_domain.logical_size()!=original.cols() ||
                declaration.output_domain.logical_size()!=original.rows())throw std::invalid_argument("solve operator/domain dimensions");
            std::set<std::size_t> selected;
            for(const auto i:interior)if(i>=original.rows() || !selected.insert(i).second)
                throw std::invalid_argument("interior coordinates must be distinct declared operator coordinates");
            for(std::size_t i=0;i<original.rows();++i)if(!selected.contains(i))interface.push_back(i);
            return block(interior,interior);
        }
        numeric::ExactMatrix prepare_interface(numeric::ExactActionGuard& guard) {
            const auto right=block(interior,interface);cross=block(interface,interior);
            interior_to_interface=numeric::ExactMatrix(interior.size(),interface.size());
            // W solves A_II W=A_IΓ, column by column. W can fill in; it is
            // retained and charged, with no inverse of A_II constructed.
            for(std::size_t j=0;j<interface.size();++j) {
                numeric::RationalVector rhs(interior.size());
                for(std::size_t i=0;i<rhs.size();++i)rhs[i]=right.get(i,j);
                const auto column=interior_solve->apply(rhs,guard);
                for(std::size_t i=0;i<column.size();++i)if(column[i]!=0) {
                    interior_to_interface.set(i,j,column[i]);guard.entries(interior_to_interface.nonzeros());
                }
            }
            schur=block(interface,interface);
            const auto correction=numeric::checked_multiply(cross,interior_to_interface,guard);
            for(const auto& [key,q]:correction.entries()) {
                schur.set(key.first,key.second,guard.sub(schur.get(key.first,key.second),q));guard.entries(schur.nonzeros());
            }
            return schur;
        }
        std::size_t retained_entries() const {
            return machine::checked_size_add(machine::checked_size_add(original.nonzeros(),cross.nonzeros()),
                machine::checked_size_add(machine::checked_size_add(interior_to_interface.nonzeros(),schur.nonzeros()),
                    machine::checked_size_add(interior_solve->stored_entries(),interface_solve->stored_entries())));
        }
    };
public:
    PreparedInteriorInterfaceSolve(const PreparedInteriorInterfaceSolve&)=default;
    PreparedInteriorInterfaceSolve& operator=(const PreparedInteriorInterfaceSolve&)=default;
    PreparedInteriorInterfaceSolve(LinearSolveDeclaration declaration,numeric::ExactMatrix a,std::vector<std::size_t> interior,
        numeric::ExactActionLimits limits={},std::function<void()> checkpoint={})
        :state_(std::make_shared<const State>(std::move(declaration),std::move(a),std::move(interior),limits,checkpoint)) {
        numeric::ExactActionGuard(limits).entries(state_->retained_entries());
    }
    numeric::RationalVector apply(const numeric::RationalVector& rhs,numeric::ExactActionLimits limits={},
        std::function<void()> checkpoint={}) const {
        numeric::ExactActionGuard guard(limits,std::move(checkpoint));guard.entries(state_->retained_entries());
        if(rhs.size()!=state_->original.rows())throw std::invalid_argument("interior/interface rhs dimension");
        const auto input=guard.canonical_input(rhs);
        numeric::RationalVector bi,bg;bi.reserve(state_->interior.size());bg.reserve(state_->interface.size());
        for(const auto i:state_->interior)bi.push_back(input[i]);
        for(const auto i:state_->interface)bg.push_back(input[i]);
        auto z=state_->interior_solve->apply(bi,guard);const auto correction=numeric::checked_apply(state_->cross,z,guard);
        for(std::size_t i=0;i<bg.size();++i)bg[i]=guard.sub(bg[i],correction[i]);
        const auto xg=state_->interface_solve->apply(bg,guard);
        const auto w=numeric::checked_apply(state_->interior_to_interface,xg,guard);
        numeric::RationalVector x(rhs.size());
        for(std::size_t i=0;i<z.size();++i)x[state_->interior[i]]=guard.sub(z[i],w[i]);
        for(std::size_t i=0;i<xg.size();++i)x[state_->interface[i]]=xg[i];
        if(numeric::checked_apply(state_->original,x,guard)!=input)throw std::logic_error("interior/interface reconstruction has nonzero residual");
        return x;
    }
    const LinearSolveDeclaration& declaration() const & {return state_->declaration;}
    const numeric::ExactMatrix& schur() const & {return state_->schur;}
    const std::vector<std::size_t>& interior() const & {return state_->interior;}
    const std::vector<std::size_t>& interface() const & {return state_->interface;}
    const LinearSolveDeclaration& declaration() const &&=delete;
    const numeric::ExactMatrix& schur() const &&=delete;
    const std::vector<std::size_t>& interior() const &&=delete;
    const std::vector<std::size_t>& interface() const &&=delete;
    std::size_t retained_entries() const {return state_->retained_entries();}
private:
    std::shared_ptr<const State> state_;
};
} // namespace rfc::local
