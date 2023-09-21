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
static idxType stack[STACK_SIZE];
static idxType sp = 0;

sCode  var[MAX_VAR_NUM];

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
static inline int makeInt(sCode* var, iType value)
{
  var->op     = VAL_INTEGER;
  var->iValue = value;
  return 0;
}

//-----------------------------------------------------------------------------
static inline int makeFloat(sCode* var, fType value)
{
  var->op     = VAL_FLOAT;
  var->fValue = value;
  return 0;
}

//-----------------------------------------------------------------------------
static int getVar(sCode* value, idxType idx)
{
  ENSURE(idx >= 0 && idx < MAX_VAR_NUM, ERR_EXEC_VAR_INV);
  memcpy(value, &var[idx], sizeof(sCode));
  return 0;
}

//-----------------------------------------------------------------------------
static int setVar(idxType idx, sCode* value)
{
  ENSURE(idx >= 0 && idx < MAX_VAR_NUM, ERR_EXEC_VAR_INV);
  memcpy(&var[idx], value, sizeof(sCode));
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
static int printExpr(sSys* sys, idxType idx)
{
  sCodeIdx expr;
  CHECK(sys->getCode(&expr, idx));

  if (expr.code.op == OP_CONCAT)
  {
    printExpr(sys, expr.code.expr.lhs);
    printExpr(sys, expr.code.expr.rhs);
  }
  else if (expr.code.op == VAL_STRING)
  {
    char str[MAX_STRING];
    CHECK(sys->getString(str, expr.code.str.start, expr.code.str.len));
    printf("%.*s", expr.code.str.len, str);
  }
  else
  {
    sCode value;
    CHECK(eval(sys, &value, idx));
    if (value.op == VAL_INTEGER)
      printf("%d", value.iValue);
    else if (value.op == VAL_FLOAT)
      printf("%f", value.fValue);
    else
      return ERR_EXEC_PRINT;
  }
  return 0;
}

//-----------------------------------------------------------------------------
static int push(idxType value)
{
  ENSURE(sp < STACK_SIZE, ERR_EXEC_STACK_OF);
  stack[sp++] = value;
  return 0;
}

//-----------------------------------------------------------------------------
static int pop(void)
{
  ENSURE(sp > 0, ERR_EXEC_STACK_UF);
  return stack[--sp];
}

//=============================================================================
// Public functions
//=============================================================================
int eval(sSys* sys, sCode* value, idxType idx)
{
  sCodeIdx expr;
  sCode lhs;
  sCode rhs;
  CHECK(sys->getCode(&expr, idx));

  switch (expr.code.op)
  {
    case OP_NEQ:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, (IS_INT(lhs) && IS_INT(rhs)) ? (lhs.iValue != rhs.iValue) : (castFloat(&lhs) != castFloat(&rhs)));
    case OP_LTEQ:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, (IS_INT(lhs) && IS_INT(rhs)) ? (lhs.iValue <= rhs.iValue) : (castFloat(&lhs) <= castFloat(&rhs)));
    case OP_GTEQ:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, (IS_INT(lhs) && IS_INT(rhs)) ? (lhs.iValue >= rhs.iValue) : (castFloat(&lhs) >= castFloat(&rhs)));
    case OP_LT:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, (IS_INT(lhs) && IS_INT(rhs)) ? (lhs.iValue <  rhs.iValue) : (castFloat(&lhs) <  castFloat(&rhs)));
    case OP_GT:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, (IS_INT(lhs) && IS_INT(rhs)) ? (lhs.iValue >  rhs.iValue) : (castFloat(&lhs) >  castFloat(&rhs)));
    case OP_EQUAL:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, (IS_INT(lhs) && IS_INT(rhs)) ? (lhs.iValue == rhs.iValue) : (castFloat(&lhs) == castFloat(&rhs)));
    case OP_OR:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, castBool(&lhs) || castBool(&rhs));
    case OP_AND:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, castBool(&lhs) && castBool(&rhs));
    case OP_NOT:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      return makeInt(value, !castBool(&lhs));
    case OP_PLUS:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      if (IS_INT(lhs) && IS_INT(rhs))
        return makeInt(value, lhs.iValue + rhs.iValue);
      return makeFloat(value, castFloat(&lhs) + castFloat(&rhs));
    case OP_MINUS:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      if (IS_INT(lhs) && IS_INT(rhs))
        return makeInt(value, lhs.iValue - rhs.iValue);
      return makeFloat(value, castFloat(&lhs) - castFloat(&rhs));
    case OP_MOD:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      return makeInt(value, castInt(&lhs) % castInt(&rhs));
    case OP_MULT:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      if (IS_INT(lhs) && IS_INT(rhs))
        return makeInt(value, lhs.iValue * rhs.iValue);
      return makeFloat(value, castFloat(&lhs) * castFloat(&rhs));
    case OP_DIV:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      ENSURE(castFloat(&rhs) != 0.0f, ERR_EXEC_DIV_ZERO);
      return makeFloat(value, castFloat(&lhs) / castFloat(&rhs));
    case OP_IDIV:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      ENSURE(castInt(&rhs) != 0, ERR_EXEC_DIV_ZERO);
      return makeInt(value, castInt(&lhs) / castInt(&rhs));
    case OP_POW:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      CHECK(eval(sys, &rhs, expr.code.expr.rhs));
      if (IS_INT(lhs) && IS_INT(rhs) && rhs.iValue >= 0)
        return makeInt(value, powf(lhs.iValue, rhs.iValue));
      return makeFloat(value, powf(castFloat(&lhs), castFloat(&rhs)));
    case OP_SIGN:
      CHECK(eval(sys, &lhs, expr.code.expr.lhs));
      if (IS_INT(lhs))
        return makeInt(value, -lhs.iValue);
      return makeFloat(value, -castFloat(&lhs));
    case VAL_INTEGER:
      return makeInt(value, expr.code.iValue);
    case VAL_FLOAT:
      return makeFloat(value, expr.code.fValue);
    case VAL_VAR:
      return getVar(value, expr.code.param);
    case VAL_REG:
      return getReg(sys, value, expr.code.param);
    default:
      return ERR_EXEC_OP_INV;
  }
}

//-----------------------------------------------------------------------------
int exec_init(void)
{
  for (int i = 0; i < MAX_VAR_NUM; i++)
  {
    var[i].op = VAL_INTEGER;
    var[i].iValue = 0;
  }
  return 0;
}

//-----------------------------------------------------------------------------
int exec(sSys* sys, idxType pc)
{
  sCodeIdx code;
  sCode value;

  ENSURE(pc >= 0, ERR_EXEC_PC);
  CHECK(sys->getCode(&code, pc));

  switch (code.code.op)
  {
    case CMD_PRINT:
      CHECK(printExpr(sys, code.code.cmd.expr));
      return pc + 1;
    case CMD_LET:
      CHECK(eval(sys, &value, code.code.cmd.expr));
      CHECK(setVar(code.code.cmd.param, &value));
      return pc + 1;
    case CMD_SET:
      CHECK(eval(sys, &value, code.code.cmd.expr));
      CHECK(setReg(sys, code.code.cmd.param, &value));
      return pc + 1;
    case CMD_IF:
      CHECK(eval(sys, &value, code.code.cmd.expr));
      return (castBool(&value)) ? pc + 1 : code.code.cmd.param;
    case CMD_GOTO:
      return code.code.cmd.param;
    case CMD_GOSUB:
      CHECK(push(pc + 1));
      return code.code.cmd.param;
    case CMD_RETURN:
      return pop();
    case CMD_NOP:
      return pc + 1;
    case CMD_END:
      return ERR_EXEC_END;
    default:
      return ERR_EXEC_CMD_INV;
  }
}