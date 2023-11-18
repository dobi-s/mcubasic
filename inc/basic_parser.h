#pragma once

#include "basic_bytecode.h"

//=============================================================================
// Defines
//=============================================================================
#define ERR_NAME_INV        -1    // Invalid name
#define ERR_NAME_KEYWORD    -2    // Keyword not allowed as name
#define ERR_MEM_CODE        -3    // Not enough code memory
#define ERR_VAR_UNDEF       -4    // Variable undefined
#define ERR_VAR_COUNT       -5    // Too many variables
#define ERR_REG_NOT_FOUND   -6    // Register not found
#define ERR_VAR_NAME        -7    // Invalid variable name
#define ERR_REG_NAME        -8    // Invalid register name
#define ERR_STR_INV         -9    // Invalid string
#define ERR_STR_LENGTH      -10   // String too long
#define ERR_STR_MEM         -11   // Not enough string memory
#define ERR_EXPR_BRACKETS   -12   // Brackets not closed
#define ERR_NUM_INV         -13   // Invalid number
#define ERR_NEWLINE         -14   // Newline expected
#define ERR_IF_THEN         -15   // THEN expected
#define ERR_IF_ENDIF        -16   // ENDIF expected
#define ERR_DO_LOOP         -17   // LOOP expected
#define ERR_ASSIGN          -18   // Assignment (=) expected
#define ERR_FOR_TO          -19   // TO expected
#define ERR_FOR_NEXT        -20   // NEXT expected
#define ERR_EXPR_MISSING    -21   // Expression missing
#define ERR_LABEL_COUNT     -22   // Too many labels
#define ERR_LABEL_DUPL      -23   // Duplicate label
#define ERR_EOF             -24   // EOF expected
#define ERR_LABEL_INV       -25   // Label invalid (linker error)
#define ERR_LABEL_MISSING   -26   // Label missing (linker error)
#define ERR_BRACKETS_MISS   -27   // Brackets missing on function call
#define ERR_SUB_NOT_FOUND   -28   // Sub function not found
#define ERR_ARG_MISMATCH    -29   // Argument mismatch
#define ERR_END_SUB         -30   // EndSub outside sub
#define ERR_NESTED_SUB      -31   // Nested sub
#define ERR_SUB_CONFLICT    -32   // Sub name conflicts with buildin function
#define ERR_SUB_COUNT       -33   // Too many subs
#define ERR_SUB_REDEF       -34   // Redefined sub
#define ERR_END_SUB_EXP     -35   // EndSub expected
#define ERR_LOCAL_NOT_FOUND -36   // Local variable not found
#define ERR_EXIT_DO         -37   // Exit Do outside Do .. Loop
#define ERR_EXIT_FOR        -38   // Exit For outside For .. Next
#define ERR_EXIT_SUB        -39   // Exit Sub outside sub

#define ERR_DIM_INV         -40   // Invalid array dimension
#define ERR_NOT_ARRAY       -41   // Variable is not an array
#define ERR_ARRAY           -42   // Variable is an array

#define ERR_NOT_IMPL        -999  // Not implemented yet


//=============================================================================
// Functions
//=============================================================================
int parseAll(const sSys* system, int* errline, int* errcol);
int link(const sSys* system);
