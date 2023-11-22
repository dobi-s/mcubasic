#include "basic_optimizer.h"
#include "basic_common.h"

//=============================================================================
// Private functions
//=============================================================================
int optimizeGoto(const sSys* sys)
{
  sCodeIdx code;
  sCodeIdx dest;
  int      timeout;

  for (idxType idx = 0; sys->getCode(&code, idx) >= 0;
       idx += sys->getCodeLen(code.code.op))
  {
    if (code.code.op != CMD_GOTO)
      continue;

    timeout         = 100;
    dest.code.param = code.code.param;
    while ((sys->getCode(&dest, dest.code.param) >= 0) &&
           (dest.code.op == CMD_GOTO) &&  // Avoid loops
           (--timeout > 0))
      ;
    if (dest.idx != code.code.param)  // jump several GOTOs
    {
      code.code.param = dest.idx;
      sys->setCode(&code);
    }
  }
  return 0;
}

//=============================================================================
// Public functions
//=============================================================================
int optimize(const sSys* system)
{
  CHECK(optimizeGoto(system));
  return 0;
}
