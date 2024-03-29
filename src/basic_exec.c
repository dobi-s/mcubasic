#include "basic_exec.h"
#include "basic_common.h"
#include "basic_config.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

//=============================================================================
// Defines
//=============================================================================
#define IS_INT(x) ((x).op == VAL_INTEGER)

//=============================================================================
// Private variables
//=============================================================================
static sCode   stack[STACK_SIZE];
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
    return value->fValue + 0.5f;
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
        CHECK(sys->getString(&str, stack[sp + i].str.start,
                             stack[sp + i].str.len));
        printf("%.*s", stack[sp + i].str.len, str);
        break;
      default:
        printf("(%d)", stack[sp + i].op);
        return ERR_EXEC_PRINT;
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------
static int returnSub(idxType cnt)
{
  idxType res;
  ENSURE(sp > fp, ERR_EXEC_STACK_UF);
  ENSURE(stack[fp].op == VAL_LABEL, ERR_EXEC_CMD_INV);
  sp  = fp - cnt;
  res = stack[fp].lbl.lbl;
  fp  = stack[fp].lbl.fp;
  return res;
}

//-----------------------------------------------------------------------------
static int svc(sSys* sys, idxType idx)
{
  ENSURE(idx >= 0 && idx < ARRAY_SIZE(sys->svcs) && sys->svcs[idx].func,
         ERR_EXEC_SVC_INV);
  ENSURE(sp > sys->svcs[idx].argc, ERR_EXEC_STACK_UF);
  sp -= sys->svcs[idx].argc;
  return sys->svcs[idx].func(&stack[sp - 1], &stack[0]);
}

//=============================================================================
// Public functions
//=============================================================================
int exec(sSys* sys, idxType pc)
{
  sCodeIdx code;
  sCode    value;
  iType    iValue;
  sCode*   ptr;

  ENSURE(pc >= 0, ERR_EXEC_PC);
  CHECK(sys->getCode(&code, pc));
  pc += sys->getCodeLen(code.code.op);

  extern void debugState(sSys * sys, sCodeIdx * code, sCode * stack, idxType sp,
                         idxType fp);
  // debugState(sys, &code, stack, sp, fp);

  switch (code.code.op)
  {
    case CMD_PRINT:
      CHECK(print(sys, code.code.param));
      return pc;
    case CMD_LET_GLOBAL:
      iValue = (code.code.param2 > 0) ? castInt(&stack[sp - 2]) : 0;
      if (code.code.param2 > 0)  // Array
      {
        ENSURE(iValue >= 0 && iValue < code.code.param2, ERR_EXEC_OUT_BOUND);
        memcpy(&stack[sp - 2], &stack[sp - 1], sizeof(stack[0]));
        sp--;
      }
      ENSURE(code.code.param >= 0 && code.code.param + iValue < sp,
             ERR_EXEC_VAR_INV);
      memcpy(&stack[code.code.param + iValue], &stack[--sp], sizeof(stack[0]));
      return pc;
    case CMD_LET_LOCAL:
      iValue = (code.code.param2 > 0) ? castInt(&stack[sp - 2]) : 0;
      if (code.code.param2 > 0)  // Array
      {
        ENSURE(iValue >= 0 && iValue < code.code.param2, ERR_EXEC_OUT_BOUND);
        memcpy(&stack[sp - 2], &stack[sp - 1], sizeof(stack[0]));
        sp--;
      }
      ENSURE(fp + code.code.param >= 0 && fp + code.code.param + iValue < sp,
             ERR_EXEC_VAR_INV);
      memcpy(&stack[fp + code.code.param + iValue], &stack[--sp],
             sizeof(stack[0]));
      return pc;
    case CMD_LET_PTR:
      sp -= 2;
      ENSURE(fp + code.code.param >= 0 && fp + code.code.param < sp,
             ERR_EXEC_VAR_INV);
      ptr = &stack[fp + code.code.param];
      ENSURE(ptr->op == VAL_PTR, ERR_EXEC_VAR_INV);
      iValue = castInt(&stack[sp]);
      ENSURE(iValue >= 0 && iValue < ptr->param2, ERR_EXEC_OUT_BOUND);
      ENSURE(ptr->param >= 0 && ptr->param + iValue < sp, ERR_EXEC_VAR_INV);
      memcpy(&stack[ptr->param + iValue], &stack[sp + 1], sizeof(stack[0]));
      return pc;
    case CMD_LET_REG:
      ENSURE(sp > 0, ERR_EXEC_STACK_UF);
      CHECK(setReg(sys, code.code.param, &stack[--sp]));
      return pc;
    case CMD_IF:
      ENSURE(sp > 0, ERR_EXEC_STACK_UF);
      return (castBool(&stack[--sp])) ? pc : code.code.param;
    case CMD_GOTO:
      return code.code.param;
    case CMD_GOSUB:
      CHECK(pushLabel(pc, fp));
      fp = sp - 1;
      return code.code.param;
    case CMD_RETURN:
      return returnSub(code.code.param);
    case CMD_POP:
      ENSURE(sp > code.code.param, ERR_EXEC_STACK_UF);
      sp -= code.code.param + 1;
      return pc;
    case CMD_NOP:
      return pc;
    case CMD_END:
      return ERR_EXEC_END;
    case CMD_SVC:
      CHECK(svc(sys, code.code.param));
      return pc;
    case CMD_GET_GLOBAL:
      iValue = (code.code.param2 > 0) ? castInt(&stack[--sp]) : 0;
      ENSURE(
          code.code.param2 == 0 || (iValue >= 0 && iValue < code.code.param2),
          ERR_EXEC_OUT_BOUND);
      ENSURE(code.code.param >= 0 && code.code.param + iValue < sp,
             ERR_EXEC_VAR_INV);
      CHECK(pushCode(&stack[code.code.param + iValue]));
      return pc;
    case CMD_GET_LOCAL:
      iValue = (code.code.param2 > 0) ? castInt(&stack[--sp]) : 0;
      ENSURE(
          code.code.param2 == 0 || (iValue >= 0 && iValue < code.code.param2),
          ERR_EXEC_OUT_BOUND);
      ENSURE(fp + code.code.param >= 0 && fp + code.code.param + iValue < sp,
             ERR_EXEC_VAR_INV);
      CHECK(pushCode(&stack[fp + code.code.param + iValue]));
      return pc;
    case CMD_GET_PTR:
      ENSURE(fp + code.code.param >= 0 && fp + code.code.param < sp,
             ERR_EXEC_VAR_INV);
      ptr = &stack[fp + code.code.param];
      ENSURE(ptr->op == VAL_PTR, ERR_EXEC_VAR_INV);
      iValue = castInt(&stack[--sp]);
      ENSURE(iValue >= 0 && iValue < ptr->param2, ERR_EXEC_OUT_BOUND);
      ENSURE(ptr->param >= 0 && ptr->param + iValue < sp, ERR_EXEC_VAR_INV);
      CHECK(pushCode(&stack[ptr->param + iValue]));
      return pc;
    case CMD_GET_REG:
      CHECK(getReg(sys, &value, code.code.param));
      CHECK(pushCode(&value));
      return pc;
    case CMD_CREATE_PTR:
      ENSURE(fp + code.code.param >= 0 && fp + code.code.param < sp,
             ERR_EXEC_VAR_INV);
      if (stack[fp + code.code.param].op == VAL_PTR)
      {
        CHECK(pushCode(&stack[fp + code.code.param]));
      }
      else
      {
        value.op     = VAL_PTR;
        value.param  = fp + code.code.param;
        value.param2 = code.code.param2;
        CHECK(pushCode(&value));
      }
      return pc;

    // clang-format off
    case OP_NEQ:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? (stack[sp].iValue      != stack[sp + 1].iValue)
          : (castFloat(&stack[sp]) != castFloat(&stack[sp + 1]))) ? -1 : 0));
      return pc;
    case OP_LTEQ:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? (stack[sp].iValue      <= stack[sp + 1].iValue)
          : (castFloat(&stack[sp]) <= castFloat(&stack[sp + 1]))) ? -1 : 0));
      return pc;
    case OP_GTEQ:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? (stack[sp].iValue      >= stack[sp + 1].iValue)
          : (castFloat(&stack[sp]) >= castFloat(&stack[sp + 1]))) ? -1 : 0));
      return pc;
    case OP_LT:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? (stack[sp].iValue      < stack[sp + 1].iValue)
          : (castFloat(&stack[sp]) < castFloat(&stack[sp + 1]))) ? -1 : 0));
      return pc;
    case OP_GT:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? (stack[sp].iValue      > stack[sp + 1].iValue)
          : (castFloat(&stack[sp]) > castFloat(&stack[sp + 1]))) ? -1 : 0));
      return pc;
    case OP_EQUAL:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? (stack[sp].iValue      == stack[sp + 1].iValue)
          : (castFloat(&stack[sp]) == castFloat(&stack[sp + 1]))) ? -1 : 0));
      return pc;
    case OP_XOR:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(castInt(&stack[sp]) ^ castInt(&stack[sp + 1])));
      return pc;
    case OP_OR:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(castInt(&stack[sp]) | castInt(&stack[sp + 1])));
      return pc;
    case OP_AND:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(castInt(&stack[sp]) & castInt(&stack[sp + 1])));
      return pc;
    case OP_NOT:
      ENSURE(sp >= 1, ERR_EXEC_STACK_UF); sp -= 1;
      CHECK(pushInt(~castInt(&stack[sp])));
      return pc;
    case OP_SHL:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(castInt(&stack[sp]) << castInt(&stack[sp + 1])));
      return pc;
    case OP_SHR:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK(pushInt(castInt(&stack[sp]) >> castInt(&stack[sp + 1])));
      return pc;
    case OP_PLUS:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? pushInt(stack[sp].iValue        + stack[sp + 1].iValue)
          : pushFloat(castFloat(&stack[sp]) + castFloat(&stack[sp + 1])));
      return pc;
    case OP_MINUS:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? pushInt(stack[sp].iValue        - stack[sp + 1].iValue)
          : pushFloat(castFloat(&stack[sp]) - castFloat(&stack[sp + 1])));
      return pc;
    case OP_MOD:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((pushInt(castInt(&stack[sp]) % castInt(&stack[sp + 1]))));
      return pc;
    case OP_MULT:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
                ? pushInt(stack[sp].iValue        * stack[sp + 1].iValue)
                : pushFloat(castFloat(&stack[sp]) * castFloat(&stack[sp + 1])));
      return pc;
    case OP_DIV:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      ENSURE(castFloat(&stack[sp + 1]) != 0.0f, ERR_EXEC_DIV_ZERO);
      CHECK(pushFloat(castFloat(&stack[sp]) / castFloat(&stack[sp + 1])));
      return pc;
    case OP_IDIV:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      ENSURE(castInt(&stack[sp + 1]) != 0, ERR_EXEC_DIV_ZERO);
      CHECK(pushInt(castInt(&stack[sp]) / castInt(&stack[sp + 1])));
      return pc;
    case OP_POW:
      ENSURE(sp >= 2, ERR_EXEC_STACK_UF); sp -= 2;
      CHECK((IS_INT(stack[sp]) && IS_INT(stack[sp + 1]))
          ? pushInt(powf(stack[sp].iValue, stack[sp + 1].iValue) + 0.5f)
          : pushFloat(powf(castFloat(&stack[sp]), castFloat(&stack[sp + 1]))));
      return pc;
    case OP_SIGN:
      ENSURE(sp >= 1, ERR_EXEC_STACK_UF); sp -= 1;
      CHECK(IS_INT(stack[sp])
          ? pushInt(-stack[sp].iValue)
          : pushFloat(-stack[sp].fValue));
      return pc;
      // clang-format on

    case VAL_ZERO:
      CHECK(pushInt(0));
      return pc;
    case VAL_INTEGER:
    case VAL_FLOAT:
    case VAL_STRING:
    case VAL_PTR:
      CHECK(pushCode(&code.code));
      return pc;
    default:
      return ERR_EXEC_CMD_INV;
  }
}

//-----------------------------------------------------------------------------
void exec_reset(void)
{
  sp = fp = 0;
}
