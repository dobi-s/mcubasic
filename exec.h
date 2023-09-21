#include "bytecode.h"

//=============================================================================
// Defines
//=============================================================================
#define ERR_EXEC_END        -800  // END reached
#define ERR_EXEC_PC         -801  // PC invalid
#define ERR_EXEC_CMD_INV    -802  // Invalid command
#define ERR_EXEC_PRINT      -803  // Can't print expression
#define ERR_EXEC_DIV_ZERO   -804  // Div by zero
#define ERR_EXEC_VAR_INV    -805  // Invalid variable
#define ERR_EXEC_REG_INV    -806  // Invalid register
#define ERR_EXEC_REG_READ   -807  // Can't read register
#define ERR_EXEC_REG_WRITE  -808  // Can't write register
#define ERR_EXEC_STACK_UF   -809  // STack underflow
#define ERR_EXEC_STACK_OF   -810  // STack overflow

//=============================================================================
// Internal functions (optimizer)
//=============================================================================
int eval(sSys* sys, sCode* value, idxType idx);

//=============================================================================
// Functions
//=============================================================================
int exec_init();
int exec(sSys* sys, idxType pc);
