#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "config.h"
#include "exec.h"

//=============================================================================
// Defines
//=============================================================================
#define IS_INT(x)  ((x).op == VAL_INTEGER)

//=============================================================================
// Private variables
//=============================================================================
static sCode stack[STACK_SIZE];
static idxType sp = 0;
static idxType fp = 0;

//=============================================================================
// Private functions
//=============================================================================
static bool castBool(sCode* value)
{
  if (value->op == VAL_INTEGER)
    return value->iValue;
  if (value->op == VAL_FLOAT)
    return value->fValue;
  return true;
}

//-----------------------------------------------------------------------------
static iType castInt(sCode* value)
{
  if (value->op == VAL_INTEGER)
    return value->iValue;
  if (value->op == VAL_FLOAT)
    return value->fValue;
  return 0;
}

//-----------------------------------------------------------------------------
static fType castFloat(sCode* value)
{
  if (value->op == VAL_INTEGER)
    return value->iValue;
  if (value->op == VAL_FLOAT)
    return value->fValue;
  return 0;
}

//-----------------------------------------------------------------------------
static inline int pushInt(iType value)
{
  ENSURE(sp < ARRAY_SIZE(stack), ERR_EXEC_STACK_OF);
  stack[sp].op     = VAL_INTEGER;
  stack[sp].iValue = value;
  sp++;
  return 0;
}

//-----------------------------------------------------------------------------
static inline int pushFloat(fType value)
{
  ENSURE(sp < ARRAY_SIZE(stack), ERR_EXEC_STACK_OF);
  stack[sp].op     = VAL_FLOAT;
  stack[sp].fValue = value;
  sp++;
  return 0;
}

//-----------------------------------------------------------------------------
static inline int pushLabel(idxType idx, idxType fp)
{
  ENSURE(sp < ARRAY_SIZE(stack), ERR_EXEC_STACK_OF);
  stack[sp].op      = VAL_LABEL;
  stack[sp].lbl.lbl = idx;
  stack[sp].lbl.fp  = fp;
  sp++;
  return 0;
}

//-----------------------------------------------------------------------------
static inline int pushCode(sCode* code)
{
  ENSURE(sp < ARRAY_SIZE(stack), ERR_EXEC_STACK_OF);
  memcpy(&stack[sp++], code, sizeof(sCode));
  return 0;
}

//-----------------------------------------------------------------------------
static int getReg(sSys* sys, sCode* value, idxType idx)
{
  ENSURE(idx >= 0 && idx < MAX_REG_NUM, ERR_EXEC_REG_INV);
  ENSURE(sys->regs[idx].getter, ERR_EXEC_REG_READ);
  return sys->regs[idx].getter(value, sys->regs[idx].cookie);
}

//-----------------------------------------------------------------------------
static int setReg(sSys* sys, idxType idx, sCode* value)
{
  ENSURE(idx >= 0 && idx < MAX_REG_NUM, ERR_EXEC_REG_INV);
  ENSURE(sys->regs[idx].setter, ERR_EXEC_REG_WRITE);
  return sys->regs[idx].setter(value, sys->regs[idx].cookie);
}

//-----------------------------------------------------------------------------
static int print(sSys* sys, idxType cnt)
{
  const char* str;

  ENSURE(sp > cnt, ERR_EXEC_STACK_UF);
  sp -= cnt + 1;

  for (int i = 0; i <= cnt; i++)
  {
    switch (stack[sp + i].op)
    {
      case VAL_INTEGER:
        printf("%d", stack[sp + i].iValue);
        break;
      case VAL_FLOAT:
        printf("%f", stack[sp + i].fValue);
        break;
      case VAL_STRING:
        CHECK(sys->getString(&str, stack[sp + i].str.start, stack[sp + i].str.len));
        printf("%.*s", stack[sp + i].str.len, str);
        break;
      default:
        printf("(%d)\n", stack[sp + i].op);
        return ERR_EXEC_PRINT;
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------
static int returnSub(idxType cnt)
{
  idxType res;
  ENSURE(sp-- > cnt, ERR_EXEC_STACK_UF);
  ENSURE(stack[sp].op == VAL_LABEL, ERR_EXEC_CMD_INV);
  res = stack[sp].lbl.lbl;
  fp  = stack[sp].lbl.fp;
  sp -= cnt;
  return res;
}

//-----------------------------------------------------------------------------
static int svc(sSys* sys, idxType idx)
{
  ENSURE(idx >= 0 && idx < ARRAY_SIZE(sys->svcs), ERR_EXEC_SVC_INV);
  ENSURE(sp > sys->svcs[idx].argc, ERR_EXEC_STACK_UF);
  sp -= sys->svcs[idx].argc;
  return sys->svcs[idx].func(&stack[sp - 1]);
}

//=============================================================================
// Public functions
//=============================================================================
int exec(sSys* sys, idxType pc)
{
  sCodeIdx code;
  sCode value;
  iType iValue;
  fType fValue;

  ENSURE(pc >= 0, ERR_EXEC_PC);
  CHECK(sys->getCode(&code, pc));

  extern void debugState(sCodeIdx* code, sCode* stack, idxType sp);
  debugState(&code, stack, sp);

  switch (code.code.op)
  {
    case CMD_PRINT:
      CHECK(print(sys, code.code.param));
      return pc + 1;
    case CMD_LET_GBL:
      ENSURE(sp > 0, ERR_EXEC_STACK_UF);
      ENSURE(code.code.param >= 0 && code.code.param < sp, ERR_EXEC_VAR_INV);
      memcpy(&stack[code.code.param], &stack[--sp], sizeof(stack[0]));
      return pc + 1;
    case CMD_LET_LCL:
      ENSURE(fp + code.code.param >= 0 && fp + code.code.param < sp, ERR_EXEC_VAR_INV);
      memcpy(&stack[fp + code.code.param], &stack[--sp], sizeof(stack[0]));
      return pc + 1;
    case CMD_LET_REG:
      ENSURE(sp > 0, ERR_EXEC_STACK_UF);
      CHECK(setReg(sys, code.code.param, &stack[--sp]));
      return pc + 1;
    case CMD_IF:
      ENSURE(sp > 0, ERR_EXEC_STACK_UF);
      return (castBool(&stack[--sp])) ? pc + 1 : code.code.param;
    case CMD_GOTO:
      return code.code.param;
    case CMD_GOSUB:
      CHECK(pushLabel(pc + 1, fp));
      fp = sp - 1;
      return code.code.param;
    case CMD_RETURN:
      return returnSub(code.code.param);
    case CMD_POP:
      ENSURE(sp-- > code.code.param, ERR_EXEC_STACK_UF);
      sp -= code.code.param;
      return pc + 1;
    case CMD_NOP:
      return pc + 1;
    case CMD_END:
      return ERR_EXEC_END;
    case CMD_SVC:
      CHECK(svc(sys, code.code.param));
      return pc + 1;
    case OP_NEQ:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? (stack[sp].iValue      != stack[sp+1].iValue)
          : (castFloat(&stack[sp]) != castFloat(&stack[sp+1]))));
      return pc + 1;
    case OP_LTEQ:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? (stack[sp].iValue      <= stack[sp+1].iValue)
          : (castFloat(&stack[sp]) <= castFloat(&stack[sp+1]))));
      return pc + 1;
    case OP_GTEQ:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? (stack[sp].iValue      >= stack[sp+1].iValue)
          : (castFloat(&stack[sp]) >= castFloat(&stack[sp+1]))));
      return pc + 1;
    case OP_LT:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? (stack[sp].iValue      < stack[sp+1].iValue)
          : (castFloat(&stack[sp]) < castFloat(&stack[sp+1]))));
      return pc + 1;
    case OP_GT:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? (stack[sp].iValue      > stack[sp+1].iValue)
          : (castFloat(&stack[sp]) > castFloat(&stack[sp+1]))));
      return pc + 1;
    case OP_EQUAL:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? (stack[sp].iValue      == stack[sp+1].iValue)
          : (castFloat(&stack[sp]) == castFloat(&stack[sp+1]))));
      return pc + 1;
    case OP_OR:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(castBool(&stack[sp]) || castBool(&stack[sp+1])));
      return pc + 1;
    case OP_AND:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(castBool(&stack[sp]) && castBool(&stack[sp+1])));
      return pc + 1;
    case OP_NOT:
      ENSURE(sp >= 1, ERR_EXEC_STACK_UF); sp -= 1;
      CHECK(pushInt(!castBool(&stack[sp])));
      return pc + 1;
    case OP_PLUS:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? pushInt(stack[sp].iValue        + stack[sp+1].iValue)
          : pushFloat(castFloat(&stack[sp]) + castFloat(&stack[sp+1])));
      return pc + 1;
    case OP_MINUS:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? pushInt(stack[sp].iValue        - stack[sp+1].iValue)
          : pushFloat(castFloat(&stack[sp]) - castFloat(&stack[sp+1])));
      return pc + 1;
    case OP_MOD:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((pushInt(castInt(&stack[sp]) % castInt(&stack[sp+1]))));
      return pc + 1;
    case OP_MULT:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? pushInt(stack[sp].iValue        * stack[sp+1].iValue)
          : pushFloat(castFloat(&stack[sp]) * castFloat(&stack[sp+1])));
      return pc + 1;
    case OP_DIV:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      ENSURE(castFloat(&stack[sp+1]) != 0.0f, ERR_EXEC_DIV_ZERO);
      CHECK(pushFloat(castFloat(&stack[sp]) / castFloat(&stack[sp+1])));
      return pc + 1;
    case OP_IDIV:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      ENSURE(castInt(&stack[sp+1]) != 0, ERR_EXEC_DIV_ZERO);
      CHECK(pushInt(castInt(&stack[sp]) / castInt(&stack[sp+1])));
      return pc + 1;
    case OP_POW:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((IS_INT(stack[sp]) && IS_INT(stack[sp+1]))
          ? pushInt(powf(stack[sp].iValue, stack[sp+1].iValue) + 0.5f)
          : pushFloat(powf(castFloat(&stack[sp]), castFloat(&stack[sp+1]))));
      return pc + 1;
    case OP_SIGN:
      ENSURE(sp >= 1, ERR_EXEC_STACK_UF); sp -= 1;
      CHECK(IS_INT(stack[sp])
          ? pushInt(-stack[sp].iValue)
          : pushFloat(-stack[sp].fValue));
      return pc + 1;
    case VAL_STRING:
    case VAL_INTEGER:
    case VAL_FLOAT:
      CHECK(pushCode(&code.code));
      return pc + 1;
    case VAL_VAR:
      ENSURE(code.code.param >= 0 && code.code.param < sp, ERR_EXEC_VAR_INV);
      CHECK(pushCode(&stack[code.code.param]));
      return pc + 1;
    case VAL_REG:
      CHECK(getReg(sys, &value, code.code.param));
      CHECK(pushCode(&value));
      return pc + 1;
    case VAL_STACK:
      ENSURE(fp + code.code.param >= 0 && fp + code.code.param < sp, ERR_EXEC_VAR_INV);
      CHECK(pushCode(&stack[fp + code.code.param]));
      return pc + 1;
    default:
      return ERR_EXEC_CMD_INV;
  }
}
