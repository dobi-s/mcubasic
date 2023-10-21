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
#define MAX_SVC_NUM         16    // Max number of buildin functions

#define MAX_NAME            10    // Max length of names (var, reg, label)

//-----------------------------------------------------------------------------
// Parser
//-----------------------------------------------------------------------------
#define MAX_VAR_NUM         16    // Max number of variables
#define MAX_SUB_NUM         16    // Max number of sub functions
#define MAX_LCL_NUM         16    // Max number of local variables
#define MAX_LABELS          16    // Max number of labels
#define MAX_STRING          40    // Max length of individual string

//-----------------------------------------------------------------------------
// Exec
//-----------------------------------------------------------------------------
#define STACK_SIZE          32    // Stack size [entries]

// TODO: Use local variables only + scoping!
// Blöcke sind
// - Sub
// - Do/Loop
// - For/Next
// - If/Else/EndIf
// - Main
//
// Globale Variablen mit fixem SP-Wert
// Locale Variablen mit relativem SP-Wert
//
// Variablen müssen mit DIM angelegt werden
//
// ----
//
// Combine subs and labels
//
// ----
//
// Split End-Keywords (eg "EndIf" -> "End If")
//

