#pragma once

//=============================================================================
// Defines
//=============================================================================
#define ERR_NAME_INV        -1    // Invalid name
#define ERR_MEM_CODE        -2    // Not enough code memory
#define ERR_VAR_UNDEF       -3    // Variable undefined
#define ERR_VAR_COUNT       -4    // Too many variables
#define ERR_REG_NOT_FOUND   -5    // Register not found
#define ERR_VAR_NAME        -6    // Invalid variable name
#define ERR_REG_NAME        -7    // Invalid register name
#define ERR_STR_INV         -8    // Invalid string
#define ERR_STR_LENGTH      -9    // String too long
#define ERR_STR_MEM         -10   // Not enough string memory
#define ERR_EXPR_BRACKETS   -11   // Brackets not closed
#define ERR_NUM_INV         -12   // Invalid number
#define ERR_NEWLINE         -13   // Newline expected
#define ERR_IF_THEN         -14   // THEN expected
#define ERR_IF_ENDIF        -15   // ENDIF expected
#define ERR_DO_LOOP         -16   // LOOP expected
#define ERR_ASSIGN          -17   // Assignment (=) expected
#define ERR_FOR_TO          -18   // TO expected
#define ERR_FOR_NEXT        -19   // NEXT expected
#define ERR_EXPR_MISSING    -20   // Expression missing
#define ERR_LABEL_COUNT     -21   // Too many labels
#define ERR_LABEL_DUPL      -22   // Duplicate label
#define ERR_EOF             -23   // EOF expected
#define ERR_LABEL_INV       -24   // Label invalid (linker error)
#define ERR_LABEL_MISSING   -25   // Label missing (linker error)
#define ERR_NOT_IMPL        -999  // Not implemented yet


//=============================================================================
// Functions
//=============================================================================
int parseAll(const sSys* system, int* errline, int* errcol);
int link(const sSys* system);