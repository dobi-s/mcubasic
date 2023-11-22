#pragma once

#include "basic_bytecode.h"

//=============================================================================
// Functions
//=============================================================================
const char* errmsg(int err);
void        debugPrintString(const sSys* sys, idxType start, idxType len);
void        debugPrintRaw(const sSys* sys);
