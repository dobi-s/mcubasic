#pragma once

//=============================================================================
// Defines
//=============================================================================
#define ERR_NAME_INV        -1    // Invalid name
#define ERR_MEM_CODE        -2    // Not enough code memory
#define ERR_VAR_UNDEF       -3    // Variable undefined
#define ERR_VAR_COUNT       -4    // Too many variables
#define ERR_REG_COUNT       -5    // Too many registers
#define ERR_VAR_NAME        -6    // Invalid variable name
#define ERR_REG_NAME        -7    // Invalid register name
#define ERR_STR_INV         -8    // Invalid string
#define ERR_STR_MEM         -9    // Not enough string memory
#define ERR_EXPR_BRACKETS   -10   // Brackets not closed
#define ERR_NUM_INV         -11   // Invalid number
#define ERR_NEWLINE         -12   // Newline expected
#define ERR_IF_THEN         -13   // THEN expected
#define ERR_IF_ENDIF        -14   // ENDIF expected
#define ERR_DO_LOOP         -15   // LOOP expected
#define ERR_ASSIGN          -16   // Assignment (=) expected
#define ERR_FOR_TO          -17   // TO expected
#define ERR_FOR_NEXT        -18   // NEXT expected
#define ERR_EXPR_MISSING    -19   // Expression missing
#define ERR_LABEL_COUNT     -20   // Too many labels
#define ERR_LABEL_DUPL      -21   // Duplicate label
#define ERR_LABEL_INV       -22   // Label invalid (linker error)
#define ERR_LABEL_MISSING   -23   // Label missing (linker error)
#define ERR_NOT_IMPL        -999  // Not implemented yet


//=============================================================================
// Functions
//=============================================================================
int parseAll(const char* filename);
int link(void);