#pragma once

//=============================================================================
// Defines
//=============================================================================
//-----------------------------------------------------------------------------
// Bytecode
//-----------------------------------------------------------------------------
#define CODE_MEM           200    // Memory for code and data  [operations]
#define STRING_MEM         200    // Memory for string storage [bytes]
#define MAX_REG_NUM         16    // Max number of registers

#define MAX_NAME            10    // Max length of names (var, reg, label)

//-----------------------------------------------------------------------------
// Parser
//-----------------------------------------------------------------------------
#define MAX_VAR_NUM         16    // Max number of variables
#define MAX_LABELS          16    // Max number of labels
#define MAX_STRING          40    // Max length of individual string

//-----------------------------------------------------------------------------
// Exec
//-----------------------------------------------------------------------------
#define STACK_SIZE          16    // Stack size [entries]
