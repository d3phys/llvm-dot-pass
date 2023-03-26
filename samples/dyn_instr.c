#include <stdio.h>
#include <stdint.h>

static void dyn_DumpAddress( uint64_t value_addr)
{
    printf( "%ld\n", value_addr);
}

void dyn_InstrBinaryOperator( uint64_t value_addr, uint64_t lhs, uint64_t rhs)
{
    dyn_DumpAddress( value_addr);
}

void dyn_InstrCallInst( uint64_t value_addr, uint8_t* callee_name)
{
    dyn_DumpAddress( value_addr);
}

void dyn_InstrBranchInst( uint64_t value_addr)
{
    dyn_DumpAddress( value_addr);
}

void dyn_InstrReturnInst( uint64_t value_addr)
{
    dyn_DumpAddress( value_addr);
}

void dyn_InstrStoreInst( uint64_t value_addr)
{
    dyn_DumpAddress( value_addr);
}

void dyn_InstrCmpInst( uint64_t value_addr)
{
    dyn_DumpAddress( value_addr);
}

void dyn_FunctionStart( uint64_t value_addr)
{
    dyn_DumpAddress( value_addr);
}
