#include <stdbool.h>
#include <stdio.h>
#include "bytecode.h"
#include "parser.h"

//=============================================================================
// Private functions
//=============================================================================
static const char* opStr(eOp op)
{
  switch (op)
  {
    case CMD_PRINT:   return "Print";
    case CMD_LET:     return "Let";
    case CMD_SET:     return "Set";
    case CMD_IF:      return "If";
    case CMD_GOTO:    return "GoTo";
    case CMD_GOSUB:   return "GoSub";
    case CMD_RETURN:  return "Return";
    case CMD_NOP:     return "Nop";
    case CMD_END:     return "End";
    case LNK_GOTO:    return "GoTo*";
    case LNK_GOSUB:   return "GoSub*";
    case OP_NEQ:      return "<>";
    case OP_LTEQ:     return "<=";
    case OP_GTEQ:     return ">=";
    case OP_LT:       return "<";
    case OP_GT:       return ">";
    case OP_EQUAL:    return "=";
    case OP_OR:       return "Or";
    case OP_AND:      return "And";
    case OP_NOT:      return "Not ";
    case OP_PLUS:     return "+";
    case OP_MINUS:    return "-";
    case OP_MOD:      return "Mod";
    case OP_MULT:     return "*";
    case OP_DIV:      return "/";
    case OP_IDIV:     return "\\";
    case OP_POW:      return "^";
    case OP_SIGN:     return "-";
    case OP_CONCAT:   return ";";
    case VAL_STRING:  return "STR";
    case VAL_INTEGER: return "INT";
    case VAL_FLOAT:   return "FLT";
    case VAL_VAR:     return "VAR";
    case VAL_REG:     return "REG";
    default:          return "(?)";
  }
}

//-----------------------------------------------------------------------------
const char* errmsg(int err)
{
  switch (err)
  {
    case ERR_NAME_INV:      return "Invalid name";
    case ERR_NAME_KEYWORD:  return "Keyword not allowed as name";
    case ERR_MEM_CODE:      return "Not enough code memory";
    case ERR_VAR_UNDEF:     return "Variable undefined";
    case ERR_VAR_COUNT:     return "Too many variables";
    case ERR_REG_NOT_FOUND: return "Register does not exist";
    case ERR_VAR_NAME:      return "Invalid variable name";
    case ERR_REG_NAME:      return "Invalid register name";
    case ERR_STR_INV:       return "Invalid register name";
    case ERR_STR_MEM:       return "Not enough string memory";
    case ERR_STR_LENGTH:    return "String too long";
    case ERR_EXPR_BRACKETS: return "Brackets not closed";
    case ERR_NUM_INV:       return "Invalid number";
    case ERR_NEWLINE:       return "Newline expected";
    case ERR_IF_THEN:       return "THEN expected";
    case ERR_IF_ENDIF:      return "ENDIF expected";
    case ERR_DO_LOOP:       return "LOOP expected";
    case ERR_ASSIGN:        return "Assignment (=) expected";
    case ERR_FOR_TO:        return "TO expected";
    case ERR_FOR_NEXT:      return "NEXT expected";
    case ERR_EXPR_MISSING:  return "Expression missing";
    case ERR_LABEL_COUNT:   return "Too many labels";
    case ERR_LABEL_DUPL:    return "Duplicate label";
    case ERR_EOF:           return "EOF expected";
    case ERR_LABEL_INV:     return "Label invalid";
    case ERR_LABEL_MISSING: return "Label missing";
    case ERR_NOT_IMPL:      return "Not implemented yet";
    default:                return "(unknown)";
  }
}

//-----------------------------------------------------------------------------
static int debugPrintPrettyExpr(const sSys* sys, idxType e)
{
  sCodeIdx c;
  if (sys->getCode(&c, e) < 0)
    return 0;

  switch (c.code.op)
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
      printf("(");
      debugPrintPrettyExpr(sys, c.code.expr.lhs);
      printf(" %s ", opStr(c.code.op));
      debugPrintPrettyExpr(sys, c.code.expr.rhs);
      printf(")");
      break;
    case OP_NOT:
    case OP_SIGN:
      printf("(%s", opStr(c.code.op));
      debugPrintPrettyExpr(sys, c.code.expr.lhs);
      printf(")");
      break;
    case OP_CONCAT:
      debugPrintPrettyExpr(sys, c.code.expr.lhs);
      {
        sCodeIdx cr;
        char ch;
        sys->getCode(&cr, c.code.expr.rhs);
        if (cr.code.op == VAL_STRING && cr.code.str.len == 1)
        {
          sys->getString(&ch, cr.code.str.start, cr.code.str.len);
          if (ch == '\n')
            return 1;
        }
      }
      printf(";");
      debugPrintPrettyExpr(sys, c.code.expr.rhs);
      break;
    case VAL_STRING:
      {
        char str[MAX_STRING];
        sys->getString(str, c.code.str.start, c.code.str.len);
        printf("\"%.*s\"", c.code.str.len, str);
      }
      break;
    case VAL_INTEGER:
      printf("%d", c.code.iValue);
      break;
    case VAL_FLOAT:
      printf("%f", c.code.fValue);
      break;
    case VAL_VAR:
      printf("V%d", c.code.param);
      break;
    case VAL_REG:
      printf("%s", sys->regs[c.code.param].name);
      break;
    default:
      break;
  }
  return 0;
}

//=============================================================================
// Public functions
//=============================================================================
void debugPrintRaw(const sSys* sys)
{
  sCodeIdx c;

  for (int i = 0; i < CODE_MEM; i++)
  {
    if (sys->getCode(&c, i) < 0)
      break;

    switch (c.code.op)
    {
      case CMD_INVALID:    break;
      case CMD_PRINT:      printf("%3d: %-7s       %3d\n", i, opStr(c.code.op), c.code.cmd.expr); break;
      case CMD_LET:        printf("%3d: %-7s V%02d = %3d\n", i, opStr(c.code.op), c.code.cmd.param, c.code.cmd.expr); break;
      case CMD_SET:        printf("%3d: %-7s R%02d = %3d\n", i, opStr(c.code.op), c.code.cmd.param, c.code.cmd.expr); break;
      case CMD_IF:         printf("%3d: %-7s       %3d -> %3d\n", i, opStr(c.code.op), c.code.cmd.expr, c.code.cmd.param); break;
      case CMD_GOTO:
      case CMD_GOSUB:      printf("%3d: %-7s           -> %3d\n", i, opStr(c.code.op), c.code.cmd.param); break;
      case CMD_RETURN:
      case CMD_NOP:
      case CMD_END:        printf("%3d: %-7s\n", i, opStr(c.code.op)); break;
      case LNK_GOTO:
      case LNK_GOSUB:      printf("%3d: %-7s           -> %3d\n", i, opStr(c.code.op), c.code.cmd.param); break;
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
      case OP_CONCAT:      printf("%3d: %3d %-2s %3d\n", i, c.code.expr.lhs, opStr(c.code.op), c.code.expr.rhs); break;
      case OP_NOT:
      case OP_SIGN:        printf("%3d: %s %3d\n", i, opStr(c.code.op), c.code.expr.lhs); break;
      case VAL_STRING:     printf("%3d: %-7s %3d %3d\n", i, opStr(c.code.op), c.code.str.start, c.code.str.len); break;
      case VAL_INTEGER:    printf("%3d: %-7s %3d\n", i, opStr(c.code.op), c.code.iValue); break;
      case VAL_FLOAT:      printf("%3d: %-7s %f\n", i, opStr(c.code.op), c.code.fValue); break;
      case VAL_VAR:
      case VAL_REG:        printf("%3d: %-7s %3d\n", i, opStr(c.code.op), c.code.param); break;
      default:             printf("???\n"); break;
    }
  }
  // printf("  %3d Code\n", nextCode);
  // printf("  %3d Data\n", ARRAY_SIZE(code) - nextData - 1);
  // printf("\n");

  // printf("Strings:\n");
  // printf("  '%.*s'\n", nextStr, strings);
  // printf("  %d Byte\n", nextStr);
  // printf("\n");

  // printf("Variables:\n");
  // for (int i = 0; i < ARRAY_SIZE(varname); i++)
  //   if (varname[i][0])
  //     printf("  %3d: '%.*s'\n", i, MAX_NAME, varname[i]);
  // printf("\n");

  // printf("Labels:\n");
  // for (int i = 0; i < ARRAY_SIZE(labels); i++)
  //   if (labels[i][0])
  //     printf("  %3d: '%-10.*s' -> %3d\n", i, MAX_NAME, labels[i], labelDst[i]);
  // printf("\n");
}

//-----------------------------------------------------------------------------
void debugPrintPretty(const sSys* sys)
{
  sCodeIdx c;
  uint32_t labels[(CODE_MEM + 31) / 32] = {0};

  for (int i = 0; i < CODE_MEM; i++)
  {
    if (sys->getCode(&c, i) < 0)
      continue;

    switch (c.code.op)
    {
      case CMD_GOSUB:
      case CMD_GOTO:
      case CMD_IF:
        labels[c.code.cmd.param >> 5] |= (1U << (c.code.cmd.param & 0x1f));
        break;
    }
  }

  for (int i = 0; i < CODE_MEM; i++)
  {
    if (sys->getCode(&c, i) < 0 || c.code.op < CODE_START || c.code.op > CODE_END)
      continue;

    if (labels[i >> 5] & (1U << (i & 0x1f)))
      printf("L%03d: ", i);
    else
      printf("      ");

    switch (c.code.op)
    {
      case CMD_PRINT:
        printf("%-7s ", opStr(c.code.op));
        if (!debugPrintPrettyExpr(sys, c.code.cmd.expr))
          putchar(';');
        putchar('\n');
        break;
      case CMD_LET:
        printf("%-7s V%d = ", opStr(c.code.op), c.code.cmd.param);
        debugPrintPrettyExpr(sys, c.code.cmd.expr);
        putchar('\n');
        break;
      case CMD_SET:
        printf("LET     %s = ", opStr(c.code.op), sys->regs[c.code.cmd.param].name);
        debugPrintPrettyExpr(sys, c.code.cmd.expr);
        putchar('\n');
        break;
      case CMD_IF:
        printf("%-7s NOT ", opStr(c.code.op));
        debugPrintPrettyExpr(sys, c.code.cmd.expr);
        printf(" THEN GOTO L%03d\n", c.code.cmd.param);
        break;
      case CMD_GOTO:
      case CMD_GOSUB:
      case LNK_GOTO:
      case LNK_GOSUB:
        printf("%-7s L%03d\n", opStr(c.code.op), c.code.cmd.param);
        break;
      case CMD_RETURN:
      case CMD_NOP:
      case CMD_END:
        printf("%-7s\n", opStr(c.code.op));
        break;
      default:
        break;
    }
  }
}
