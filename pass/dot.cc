#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormatVariadic.h"

using namespace llvm;


namespace {

template <typename T>
uint64_t
GetValueID( T& value)
{
    return reinterpret_cast<uint64_t>( std::addressof( value));
}

template <typename T>
uint64_t
GetValueID( T* value)
{
    return reinterpret_cast<uint64_t>( value);
}


class InstrumentFunction
{
public:
    /**
     * Predefine return value and argument types:
     * void function_name( uint64_t value_addr)
     */
    template <typename... ArgTypes>
    InstrumentFunction( Module& mod,
                        llvm::StringRef name,
                        ArgTypes&&... arg_types)
        : function_{ mod.getOrInsertFunction( name, Type::getVoidTy( mod.getContext()), Type::getInt64Ty( mod.getContext()),
                                                                                        std::forward<ArgTypes>( arg_types)...) }
    {}

    template <typename... ArgValues>
    void createCall( IRBuilder<>& builder,
                     Instruction* inst,
                     ArgValues&&... arg_values)
    {
        createCall( builder, GetValueID( inst), std::forward<ArgValues>( arg_values)...);
    }

    template <typename... ArgValues>
    void createCall( IRBuilder<>& builder,
                     uint64_t addr,
                     ArgValues&&... arg_values)
    {
        Value *value_addr = ConstantInt::get( builder.getInt64Ty(), addr);
        builder.CreateCall( function_, { value_addr, std::forward<ArgValues>( arg_values)...});
    }

private:
    FunctionCallee function_;
};


template <typename Inst>
class Instrument
    : private InstrumentFunction
{
private:
    template <typename T>
    class CastIterator
        : public BasicBlock::iterator
    {
    public:
        CastIterator<T>( BasicBlock::iterator it)
            : BasicBlock::iterator{ it}
        {}

        operator T*() const { return dyn_cast<T>( &BasicBlock::iterator::operator*()); }
        T& operator*() const { return *dyn_cast<T>( &BasicBlock::iterator::operator*()); }
        T* operator->() const { return &operator*(); }
    };

public:
    Instrument( Module& mod)
        : Instrument{ mod, mod.getContext()}
    {}

    using Iterator = CastIterator<Inst>;
    auto createCall( Iterator it,
                     IRBuilder<>& builder)
    {
        builder.SetInsertPoint( it);
        InstrumentFunction::createCall( builder, it);
        return it;
    }

private:
    Instrument( Module& mod, LLVMContext& ctx);
};

template<>
Instrument<BinaryOperator>::Instrument( Module& mod, LLVMContext& ctx)
    : InstrumentFunction{ mod, "dyn_InstrBinaryOperator", Type::getInt64Ty( ctx), Type::getInt64Ty( ctx)}
{}

template<> auto
Instrument<BinaryOperator>::createCall( Iterator it,
                                        IRBuilder<>& builder)
{
    builder.SetInsertPoint( it->getNextNode());
    Value *lhs = it->getOperand( 0);
    Value *rhs = it->getOperand( 1);
    InstrumentFunction::createCall( builder, it, lhs, rhs);
    return ++it;
}

template<>
Instrument<CallInst>::Instrument( Module& mod, LLVMContext& ctx)
    : InstrumentFunction{ mod, "dyn_InstrCallInst", Type::getInt8Ty( ctx)->getPointerTo()}
{}

template<> auto
Instrument<CallInst>::createCall( Iterator it,
                                  IRBuilder<>& builder)
{
    builder.SetInsertPoint( it);
    Value *callee_name = builder.CreateGlobalStringPtr( it->getCalledFunction()->getName());
    InstrumentFunction::createCall( builder, it, callee_name);
    return it;
}

template<>
Instrument<BranchInst>::Instrument( Module& mod, LLVMContext& ctx)
    : InstrumentFunction{ mod, "dyn_InstrBranchInst"}
{}

template<>
Instrument<ReturnInst>::Instrument( Module& mod, LLVMContext& ctx)
    : InstrumentFunction{ mod, "dyn_InstrReturnInst"}
{}

template<>
Instrument<CmpInst>::Instrument( Module& mod, LLVMContext& ctx)
    : InstrumentFunction{ mod, "dyn_InstrCmpInst"}
{}

template<>
Instrument<StoreInst>::Instrument( Module& mod, LLVMContext& ctx)
    : InstrumentFunction{ mod, "dyn_InstrStoreInst"}
{}

struct DotPass
    : public FunctionPass
{
    static char ID;

    DotPass()
        : FunctionPass{ ID}
    {}

    virtual bool runOnFunction( Function &F)
    {
        for ( auto &U : F.uses() )
        {
            User *user = U.getUser();
            outs() << formatv("\"{0}\"->\"{1}\" [ltail=\"cluster_{0}\"]\n", GetValueID(F), GetValueID( user));
        }

        outs() << formatv("subgraph \"cluster_{0}\" {\n", GetValueID(F));
        outs() << formatv("{ rank = \"same\"\n");
        outs() << formatv("\"{0}\" [label=\"{1} (uses: {2})\", shape=\"plain\"]\n", GetValueID( F), F.getName(), F.getNumUses());

        for ( auto &B : F )
        {
            for ( auto &I : B )
            {
                outs() << formatv( "\"{0}\" [label=\"{1} (uses: {2})\"]\n", GetValueID( I), I, I.getNumUses());
            }
        }
        outs() << formatv("}\n");

        uint64_t previous_id = GetValueID( F);
        for ( auto &B : F )
        {
            for ( auto &I : B )
            {
                outs() << formatv( "\"{0}\"->\"{1}\" [style=\"invis\", weight=5]\n", GetValueID( I), previous_id);

                previous_id = GetValueID( I);
                for (auto &U : I.uses())
                {
                    User *user = U.getUser();
                    outs() << formatv( "\"{0}\"->\"{1}\"\n", GetValueID( I), GetValueID( user));
                }
            }
        }
        outs() << "}\n";

        // Prepare builder for IR modification
        LLVMContext &ctx = F.getContext();
        IRBuilder<> builder{ ctx};
        Module& mod = *F.getParent();

        InstrumentFunction function_start{ mod, "dyn_FunctionStart"};
        builder.SetInsertPoint(&F.getEntryBlock().front());
        function_start.createCall( builder, GetValueID( F));

        Instrument<BinaryOperator> binary_operator{ mod};
        Instrument<CallInst>       call_inst{ mod};
        Instrument<BranchInst>     branch_inst{ mod};
        Instrument<ReturnInst>     return_inst{ mod};
        Instrument<StoreInst>      store_inst{ mod};
        Instrument<CmpInst>        cmp_inst{ mod};

        for ( auto &bblock : F )
        {
            for ( auto it = bblock.begin(); it != bblock.end(); ++it )
            {
                if      ( isa<BinaryOperator>( it) ) { it = binary_operator.createCall( it, builder); }
                else if ( isa<CallInst>( it) )       { it = call_inst.createCall( it, builder); }
                else if ( isa<BranchInst>( it) )     { it = branch_inst.createCall( it, builder); }
                else if ( isa<ReturnInst>( it) )     { it = return_inst.createCall( it, builder); }
                else if ( isa<StoreInst>( it) )      { it = store_inst.createCall( it, builder); }
                else if ( isa<CmpInst>( it) )        { it = cmp_inst.createCall( it, builder); }
            }
        }

        return true;
    }

};

}

// Registering your pass in the LLVM
char DotPass::ID = 0;
static RegisterPass<DotPass> X{ "dotpass", "Dot Graph dump pass", false, true};
