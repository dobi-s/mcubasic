#include "basic_debug.h"
#include "basic_bytecode.h"
#include "basic_config.h"
#include "basic_parser.h"
#include <stdbool.h>
#include <stdio.h>

//=============================================================================
// Private functions
//=============================================================================
static const char* opStr(eOp op)
{
  // clang-format off
  switch (op)
  {
    case CMD_PRINT:     return "Print";
    case CMD_LET_GLOBAL:return "LetGlobl";
    case CMD_LET_LOCAL: return "LetLocal";
    case CMD_LET_PTR:   return "LetPtr";
    case CMD_LET_REG:   return "LetReg";
    case CMD_IF:        return "If";
    case CMD_GOTO:      return "GoTo";
    case CMD_GOSUB:     return "GoSub";
    case CMD_RETURN:    return "Return";
    case CMD_POP:       return "Pop";
    case CMD_NOP:       return "Nop";
    case CMD_END:       return "End";
    case CMD_SVC:       return "Svc";
    case CMD_GET_GLOBAL:return "GetGlobl";
    case CMD_GET_LOCAL: return "GetLocal";
    case CMD_GET_PTR:   return "GetPtr";
    case CMD_GET_REG:   return "GetReg";
    case CMD_CREATE_PTR:return "CreatPtr";
    case LNK_GOTO:      return "GoTo*";
    case LNK_GOSUB:     return "GoSub*";
    case OP_NEQ:        return "<>";
    case OP_LTEQ:       return "<=";
    case OP_GTEQ:       return ">=";
    case OP_LT:         return "<";
    case OP_GT:         return ">";
    case OP_EQUAL:      return "=";
    case OP_XOR:        return "Xor";
    case OP_OR:         return "Or";
    case OP_AND:        return "And";
    case OP_NOT:        return "Not ";
    case OP_SHL:        return "Shl";
    case OP_SHR:        return "Shr";
    case OP_PLUS:       return "+";
    case OP_MINUS:      return "-";
    case OP_MOD:        return "Mod";
    case OP_MULT:       return "*";
    case OP_DIV:        return "/";
    case OP_IDIV:       return "\\";
    case OP_POW:        return "^";
    case OP_SIGN:       return "Sign";
    case VAL_ZERO:      return "ZERO";
    case VAL_INTEGER:   return "INT";
    case VAL_FLOAT:     return "FLOAT";
    case VAL_STRING:    return "STR";
    case VAL_PTR:       return "PTR";
    default:            return "(?)";
  }
  // clang-format on
}

//-----------------------------------------------------------------------------
static void printCmd(const sSys* sys, idxType i, sCode* c)
{
  switch (c->op)
  {
    case CMD_INVALID:
      break;
    case CMD_PRINT:
    case CMD_LET_PTR:
    case CMD_LET_REG:
    case CMD_IF:
    case CMD_GOTO:
    case LNK_GOTO:
    case CMD_GOSUB:
    case LNK_GOSUB:
    case CMD_RETURN:
    case CMD_POP:
    case CMD_SVC:
    case CMD_GET_PTR:
    case CMD_GET_REG:
      printf("%3d: %-8s (%5d)", i, opStr(c->op), c->param);
      break;
    case CMD_LET_GLOBAL:
    case CMD_LET_LOCAL:
    case CMD_GET_GLOBAL:
    case CMD_GET_LOCAL:
    case CMD_CREATE_PTR:
    case VAL_PTR:
      printf("%3d: %-8s (%-2d%3d)", i, opStr(c->op), c->param2, c->param);
      break;
    case CMD_NOP:
    case CMD_END:
    case OP_NEQ:
    case OP_LTEQ:
    case OP_GTEQ:
    case OP_LT:
    case OP_GT:
    case OP_EQUAL:
    case OP_XOR:
    case OP_OR:
    case OP_AND:
    case OP_SHL:
    case OP_SHR:
    case OP_PLUS:
    case OP_MINUS:
    case OP_MOD:
    case OP_MULT:
    case OP_DIV:
    case OP_IDIV:
    case OP_POW:
    case OP_NOT:
    case OP_SIGN:
    case VAL_ZERO:
      printf("%3d: %-8s (     )", i, opStr(c->op));
      break;
    case VAL_INTEGER:
      printf("%3d: %-8s (%5d)", i, opStr(c->op), c->iValue);
      break;
    case VAL_FLOAT:
      printf("%3d: %-8s (%5.1f)", i, opStr(c->op), c->fValue);
      break;
    case VAL_STRING:
      printf("%3d: %-8s (%-2d%3d) ", i, opStr(c->op), c->str.len, c->str.start);
      debugPrintString(sys, c->str.start, c->str.len);
      break;
    default:
      printf("%3d: ? %3d ?        ", i, c->op);
      break;
  }
}

//=============================================================================
// Public functions
//=============================================================================
const char* errmsg(int err)
{
  // clang-format off
  switch (err)
  {
    case ERR_NAME_INV:        return "Invalid name";
    case ERR_NAME_KEYWORD:    return "Keyword not allowed as name";
    case ERR_MEM_CODE:        return "Not enough code memory";
    case ERR_VAR_UNDEF:       return "Variable undefined";
    case ERR_VAR_COUNT:       return "Too many variables";
    case ERR_REG_NOT_FOUND:   return "Register does not exist";
    case ERR_VAR_NAME:        return "Invalid variable name";
    case ERR_REG_NAME:        return "Invalid register name";
    case ERR_STR_INV:         return "Invalid register name";
    case ERR_STR_MEM:         return "Not enough string memory";
    case ERR_STR_LENGTH:      return "String too long";
    case ERR_EXPR_BRACKETS:   return "Brackets not closed";
    case ERR_NUM_INV:         return "Invalid number";
    case ERR_NEWLINE:         return "Newline expected";
    case ERR_IF_THEN:         return "THEN expected";
    case ERR_IF_ENDIF:        return "END IF expected";
    case ERR_DO_LOOP:         return "LOOP expected";
    case ERR_ASSIGN:          return "Assignment (=) expected";
    case ERR_FOR_TO:          return "TO expected";
    case ERR_FOR_NEXT:        return "NEXT expected";
    case ERR_EXPR_MISSING:    return "Expression missing";
    case ERR_LABEL_COUNT:     return "Too many labels";
    case ERR_LABEL_DUPL:      return "Duplicate label";
    case ERR_EOF:             return "EOF expected";
    case ERR_LABEL_INV:       return "Label invalid";
    case ERR_LABEL_MISSING:   return "Label missing";
    case ERR_BRACKETS_MISS:   return "Brackets missing on function call";
    case ERR_SUB_NOT_FOUND:   return "Sub function not found";
    case ERR_ARG_MISMATCH:    return "Argument mismatch";
    case ERR_END_SUB:         return "End Sub outside sub";
    case ERR_NESTED_SUB:      return "Nested sub";
    case ERR_SUB_CONFLICT:    return "Sub name conflicts with buildin function";
    case ERR_SUB_COUNT:       return "Too many subs";
    case ERR_SUB_REDEF:       return "Redefined sub";
    case ERR_END_SUB_EXP:     return "END SUB expected";
    case ERR_LOCAL_NOT_FOUND: return "Local variable not found";
    case ERR_EXIT_DO:         return "Exit Do outside Do .. Loop";
    case ERR_EXIT_FOR:        return "Exit For outside For .. Next";
    case ERR_EXIT_SUB:        return "Exit Sub outside sub";
    case ERR_DIM_INV:         return "Invalid array dimension";
    case ERR_NOT_ARRAY:       return "Variable is not an array";
    case ERR_ARRAY:           return "Variable is an array";
    case ERR_ARRAY_NOT_FOUND: return "Array not found (Sub called with brackets?)";
    case ERR_NOT_IMPL:        return "Not implemented yet";
    default:                  return "(unknown)";
  }
  // clang-format on
}

//-----------------------------------------------------------------------------
void debugPrintRaw(const sSys* sys)
{
  sCodeIdx c;

  for (idxType i = 0; sys->getCode(&c, i) >= 0; i += sys->getCodeLen(c.code.op))
  {
    if (c.code.op == CMD_INVALID)
      continue;
    printCmd(sys, i, &c.code);
    printf(BASIC_OUT_EOL);
  }
}

//-----------------------------------------------------------------------------
void debugPrintString(const sSys* sys, idxType start, idxType len)
{
  const char* str;
  if (sys->getString(&str, start, len) < 0)
    return;

  putchar('"');
  while (len--)
  {
    switch (*str)
    {
      // clang-format off
      case '\0': printf("\\0"); break;
      case '\a': printf("\\a"); break;
      case '\b': printf("\\b"); break;
      case '\f': printf("\\f"); break;
      case '\n': printf("\\n"); break;
      case '\r': printf("\\r"); break;
      case '\t': printf("\\t"); break;
      case '\v': printf("\\v"); break;
      // clang-format on
      default:
        if (*str >= ' ' && *str < 0x7F)
          putchar(*str);
        else
          printf("\\x%02x", *str);
    }
    str++;
  }
  putchar('"');
}

//-----------------------------------------------------------------------------
void debugState(sSys* sys, sCodeIdx* code, sCode* stack, idxType sp, idxType fp)
{
  printCmd(sys, code->idx, &code->code);
  printf(" %3d/%3d:", fp, sp);
  for (int i = 0; i < sp; i++)
    switch (stack[i].op)
    {
      case VAL_INTEGER:
        printf(" [Int %3d]", stack[i].iValue);
        break;
      case VAL_FLOAT:
        printf(" [Flt %.3f]", stack[i].fValue);
        break;
      case VAL_STRING:
        printf(" [Str %3d %3d]", stack[i].str.start, stack[i].str.len);
        break;
      case VAL_LABEL:
        printf(" [Lbl %3d %3d]", stack[i].lbl.lbl, stack[i].lbl.fp);
        break;
      case VAL_PTR:
        printf(" [Ptr %3d %3d]", stack[i].param, stack[i].param2);
        break;
      default:
        printf(" [%-3d        ]", stack[i].op);
        break;
    }
  printf(BASIC_OUT_EOL);
}
