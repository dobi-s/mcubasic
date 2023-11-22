#pragma once

//=============================================================================
// Defines
//=============================================================================
#define BASIC_OUT_EOL        "\r\n" // EOL for outputs

//-----------------------------------------------------------------------------
// Bytecode
//-----------------------------------------------------------------------------
#define CODE_MEM          1024      // Memory for code and data  [bytes]
#define STRING_MEM         256      // Memory for string storage [bytes]
#define MAX_REG_NUM         16      // Max number of registers
#define MAX_SVC_NUM         16      // Max number of buildin functions

#define MAX_NAME            10      // Max length of names (var, reg, label)

//-----------------------------------------------------------------------------
// Parser
//-----------------------------------------------------------------------------
#define MAX_VAR_NUM         16      // Max number of variables
#define MAX_SUB_NUM         16      // Max number of sub functions
#define MAX_LABELS          16      // Max number of labels
#define MAX_STRING          40      // Max length of individual string
#define STAT                 1      // Enable bookkeeping for statistics

//-----------------------------------------------------------------------------
// Exec
//-----------------------------------------------------------------------------
#define STACK_SIZE          32      // Stack size [entries]
