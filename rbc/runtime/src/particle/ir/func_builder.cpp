/**
 * @file func_builder.cpp
 * @brief The Implementation of IR Function Builder
 * @author sailing-innocent
 * @date 2024-06-04
 */

#include "rbc_particle/ir/func_builder.h"
#include "rbc_particle/ir/module.h"
#include "rbc_particle/ir/type.h"
#include "rbc_particle/ir/symbol.h"
#include "rbc_particle/ir/instruction.h"
#include "rbc_particle/ir/literal.h"

#include <luisa/ast/function_builder.h>
#include <luisa/ast/function.h>
#include <luisa/ast/type.h>

namespace rbc::ps {

const Expression *IRFuncBuilder::_find_ref(const luisa::string_view name) const noexcept {
    auto iter = m_expr_map.find(name);
    LUISA_ASSERT(iter != m_expr_map.end(), "Can't find ref: {}", name);
    if (iter != m_expr_map.end()) {
        return iter->second;
    }
}
luisa::shared_ptr<const luisa::compute::detail::FunctionBuilder> IRFuncBuilder::build_func(const IRModule &ir) noexcept {
    using namespace luisa;
    using namespace luisa::compute;
    auto num_blocks = 256;
    auto f = FuncBuilder::define_kernel([&] {
        auto &cur = *FuncBuilder::current();
        ///. set block size
        cur.set_block_size(make_uint3(num_blocks, 1, 1));
        /// bind args
        for (auto id : ir.param_ids()) {
            if (ir.symbols()[id]->type()->primitive() == d6::ps::IRType::Primitive::BUFFER_F32) {
                m_expr_map.emplace(ir.symbols()[id]->identifier(), cur.buffer(Type::of<Buffer<float>>()));
            }
        }
        // builtins
        for (auto id : ir.builtin_ids()) {
            if (ir.symbols()[id]->identifier() == "idx") { _built_in_idx(cur); }
        }
        // local
        for (auto id : ir.local_ids()) {
            LUISA_INFO("local: {} {}", id, ir.symbols()[id]->identifier());

            m_expr_map.emplace(ir.symbols()[id]->identifier(), cur.local(ir.symbols()[id]->type()->to_lc_type()));
        }

        // temp as local
        for (auto id : ir.temp_ids()) {
            LUISA_INFO("temp: {} {}", id, ir.symbols()[id]->identifier());
            m_expr_map.emplace(ir.symbols()[id]->identifier(), cur.local(ir.symbols()[id]->type()->to_lc_type()));
        }

        // const as literal
        for (auto id : ir.const_ids()) {
            LUISA_INFO("const: {} {}", id, ir.symbols()[id]->identifier());
            if (ir.symbols()[id]->type()->is_float()) {
                m_expr_map.emplace(ir.symbols()[id]->identifier(), cur.literal(ir.symbols()[id]->type()->to_lc_type(), ir.symbols()[id]->initial_values()[0].as_float()));
            } else if (ir.symbols()[id]->type()->is_uint()) {
                m_expr_map.emplace(ir.symbols()[id]->identifier(), cur.literal(ir.symbols()[id]->type()->to_lc_type(), ir.symbols()[id]->initial_values()[0].as_uint()));
            } else if (ir.symbols()[id]->type()->is_int()) {
                m_expr_map.emplace(ir.symbols()[id]->identifier(), cur.literal(ir.symbols()[id]->type()->to_lc_type(), ir.symbols()[id]->initial_values()[0].as_int()));
            }
        }
        auto inst_size = ir.instructions().size();
        for (auto inst_id = 0; inst_id < inst_size; inst_id++) {
            LUISA_INFO("inst_id: {}", inst_id);
            auto inst = ir.instructions()[inst_id].get();
            if (inst->opcode() == "mul") { _mul(inst, cur); }
            if (inst->opcode() == "add") { _add(inst, cur); }
            if (inst->opcode() == "sub") { _sub(inst, cur); }
            if (inst->opcode() == "read") { _read(inst, cur); }
            if (inst->opcode() == "write") { _write(inst, cur); }
        }
        // read buffer
        // x_id = 3 * idx + 0;
        // local f32 x
        // read b01 idx x
        // auto buf = _find_ref("b01");
        // auto x   = cur.call(Type::of<float>(), CallOp::BUFFER_READ, { buf, x_id });

        // local f32 v
        // read b02 idx v
        // auto v = cur.call(Type::of<float>(), CallOp::BUFFER_READ, { m_params[1], x_id });

        // local f32 dt 0.001
        // local f32 vnext
        // local f32 xnext
        // vnext = v - dt * x;
        // xnext = x + dt * v;
        // auto dt = cur.literal(Type::of<float>(), 0.001f);
        // auto vp = cur.binary(Type::of<float>(), BinaryOp::SUB, v, cur.binary(Type::of<float>(), BinaryOp::MUL, dt, x));
        // auto xp = cur.binary(Type::of<float>(), BinaryOp::ADD, x, cur.binary(Type::of<float>(), BinaryOp::MUL, dt, v));

        // write b01 idx xnext
        // cur.call(CallOp::BUFFER_WRITE, { m_params[0], x_id, xp });

        // write b02 idx vnext
        // cur.call(CallOp::BUFFER_WRITE, { m_params[1], x_id, vp });
    });

    return f;
}

void IRFuncBuilder::_built_in_idx(FuncBuilder &cur) noexcept {
    auto idx_uint3 = cur.dispatch_id();
    auto idx = cur.local(Type::of<uint>());
    // idx = idx_uint3.x;
    uint64_t swizzle_code = 0ull;
    cur.assign(idx, cur.swizzle(Type::of<uint>(), idx_uint3, 1, swizzle_code));
    m_expr_map.emplace("idx", idx);
}

void IRFuncBuilder::_mul(const Instruction *inst, FuncBuilder &cur) noexcept {
    auto op = BinaryOp::MUL;
    auto arg0 = inst->args()[0];
    auto arg1 = inst->args()[1];
    auto dst = inst->args()[2];
    LUISA_INFO("mul: {} {} {}", arg0->identifier(), arg1->identifier(), dst->identifier());
    auto a0 = _find_ref(arg0->identifier());
    auto a1 = _find_ref(arg1->identifier());
    auto d = _find_ref(dst->identifier());
    cur.assign(d, cur.binary(dst->type()->to_lc_type(), op, a0, a1));
}

void IRFuncBuilder::_add(const Instruction *inst, FuncBuilder &cur) noexcept {
    auto op = BinaryOp::ADD;
    auto arg0 = inst->args()[0];
    auto arg1 = inst->args()[1];
    auto dst = inst->args()[2];
    LUISA_INFO("add: {} {} {}", arg0->identifier(), arg1->identifier(), dst->identifier());
    auto a0 = _find_ref(arg0->identifier());
    auto a1 = _find_ref(arg1->identifier());
    auto d = _find_ref(dst->identifier());
    cur.assign(d, cur.binary(dst->type()->to_lc_type(), op, a0, a1));
}

void IRFuncBuilder::_sub(const Instruction *inst, FuncBuilder &cur) noexcept {
    auto op = BinaryOp::SUB;
    auto arg0 = inst->args()[0];
    auto arg1 = inst->args()[1];
    auto dst = inst->args()[2];
    LUISA_INFO("sub: {} {} {}", arg0->identifier(), arg1->identifier(), dst->identifier());
    auto a0 = _find_ref(arg0->identifier());
    auto a1 = _find_ref(arg1->identifier());
    auto d = _find_ref(dst->identifier());
    cur.assign(d, cur.binary(dst->type()->to_lc_type(), op, a0, a1));
}

void IRFuncBuilder::_read(const Instruction *inst, FuncBuilder &cur) noexcept {
    LUISA_INFO("read: {} {} {}", inst->args()[0]->identifier(), inst->args()[1]->identifier(), inst->args()[2]->identifier());
    auto buf = _find_ref(inst->args()[0]->identifier());
    auto idx = _find_ref(inst->args()[1]->identifier());
    auto dst = _find_ref(inst->args()[2]->identifier());
    cur.assign(dst, cur.call(dst->type(), CallOp::BUFFER_READ, {buf, idx}));
}

void IRFuncBuilder::_write(const Instruction *inst, FuncBuilder &cur) noexcept {
    LUISA_INFO("write: {} {} {}", inst->args()[0]->identifier(), inst->args()[1]->identifier(), inst->args()[2]->identifier());
    auto buf = _find_ref(inst->args()[0]->identifier());
    auto idx = _find_ref(inst->args()[1]->identifier());
    auto src = _find_ref(inst->args()[2]->identifier());
    cur.call(CallOp::BUFFER_WRITE, {buf, idx, src});
}

}// namespace rbc::ps