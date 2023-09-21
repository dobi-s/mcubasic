#include <stdbool.h>
#include "common.h"
#include "optimizer.h"
#include "bytecode.h"
#include "exec.h"

//=============================================================================
// Private functions
//=============================================================================
static int precalc(sSys* sys)
{
  sCodeIdx cur, lhs, rhs;
  bool found = true;
  int  startData = 0;

  for (int i = 0; i < CODE_MEM; i++)
  {
    CHECK(sys->getCode(&cur, i));
    if (cur.code.op >= DATA_START && cur.code.op <= DATA_END)
    {
      startData = i;
      break;
    }
  }


  while (found)
  {
    found = false;
    for (int i = startData; i < CODE_MEM; i++)
    {
      CHECK(sys->getCode(&cur, i));

      switch (cur.code.op)
      {
        case OP_NEQ:
        case OP_LTEQ:
        case OP_GTEQ:
        case OP_LT:
        case OP_GT:
        case OP_EQUAL:
        case OP_OR:
        case OP_AND:
        case OP_PLUS:
        case OP_MINUS:
        case OP_MOD:
        case OP_MULT:
        case OP_DIV:
        case OP_IDIV:
        case OP_POW:
          CHECK(sys->getCode(&rhs, cur.code.expr.rhs));
          if (rhs.code.op != VAL_INTEGER && rhs.code.op != VAL_FLOAT)
            continue;
          // no break;
        case OP_NOT:
        case OP_SIGN:
          CHECK(sys->getCode(&lhs, cur.code.expr.lhs));
          if (lhs.code.op != VAL_INTEGER && lhs.code.op != VAL_FLOAT)
            continue;
          eval(sys, &cur.code, i);
          CHECK(sys->setCode(&cur));
          found = true;
          break;
      }
    }
  }
}

//=============================================================================
// Public functions
//=============================================================================
int optimize(sSys* sys)
{
  CHECK(precalc(sys));
  return 0;
}