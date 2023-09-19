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
    case CMD_PRINT:   return "PRINT";
    case CMD_LET:     return "LET";
    case CMD_SET:     return "SET";
    case CMD_IF:      return "IF";
    case CMD_GOTO:    return "GOTO";
    case CMD_GOSUB:   return "GOSUB";
    case CMD_RETURN:  return "RETURN";
    case CMD_NOP:     return "NOP";
    case CMD_END:     return "END";
    case LNK_GOTO:    return "GOTO*";
    case LNK_GOSUB:   return "GOSUB*";
    case OP_NEQ:      return "!=";
    case OP_LTEQ:     return "<=";
    case OP_GTEQ:     return ">=";
    case OP_LT:       return "<";
    case OP_GT:       return ">";
    case OP_EQUAL:    return "==";
    case OP_OR:       return "||";
    case OP_AND:      return "&&";
    case OP_NOT:      return "!";
    case OP_PLUS:     return "+";
    case OP_MINUS:    return "-";
    case OP_MOD:      return "%";
    case OP_MULT:     return "*";
    case OP_DIV:      return "/";
    case OP_IDIV:     return "\\";
    case OP_POW:      return "^";
    case OP_SIGN:     return "~";
    case VAL_STRING:  return "STR";
    case VAL_INTEGER: return "INT";
    case VAL_FLOAT:   return "FLT";
    case VAL_VAR:     return "VAR";
    case VAL_REG:     return "REG";
    default:          return "(?)";
  }
}

//-----------------------------------------------------------------------------
static void debugPrintPrettyExpr(idxType e)
{
  switch (code[e].op)
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
      debugPrintPrettyExpr(code[e].expr.lhs);
      printf("%s", opStr(code[e].op));
      debugPrintPrettyExpr(code[e].expr.rhs);
      printf(")");
      break;
    case OP_NOT:
    case OP_SIGN:
      printf("(");
      printf("%s", opStr(code[e].op));
      debugPrintPrettyExpr(code[e].expr.lhs);
      printf(")");
      break;
    case VAL_STRING:
      printf("\"%.*s\"", code[e].str.len, &strings[code[e].str.start]);
      break;
    case VAL_INTEGER:
      printf("%d", code[e].iValue);
      break;
    case VAL_FLOAT:
      printf("%f", code[e].fValue);
      break;
    case VAL_VAR:
      printf("V%d", code[e].param);
      break;
    case VAL_REG:
      printf("R%d", code[e].param);
      break;
    default:
      break;
  }
}

//=============================================================================
// Public functions
//=============================================================================
void debugPrintRaw(void)
{
  for (int i = 0; i < CODE_MEM; i++)
  {
    switch (code[i].op)
    {
      case CMD_INVALID:    break;
      case CMD_PRINT:      printf("%3d: %-7s       %3d\n", i, opStr(code[i].op), code[i].cmd.expr); break;
      case CMD_LET:        printf("%3d: %-7s V%02d = %3d\n", i, opStr(code[i].op), code[i].cmd.param, code[i].cmd.expr); break;
      case CMD_SET:        printf("%3d: %-7s R%02d = %3d\n", i, opStr(code[i].op), code[i].cmd.param, code[i].cmd.expr); break;
      case CMD_IF:         printf("%3d: %-7s       %3d -> %3d\n", i, opStr(code[i].op), code[i].cmd.expr, code[i].cmd.param); break;
      case CMD_GOTO:
      case CMD_GOSUB:      printf("%3d: %-7s           -> %3d\n", i, opStr(code[i].op), code[i].cmd.param); break;
      case CMD_RETURN:
      case CMD_NOP:
      case CMD_END:        printf("%3d: %-7s\n", i, opStr(code[i].op)); break;
      case LNK_GOTO:
      case LNK_GOSUB:      printf("%3d: %-7s           -> %3d\n", i, opStr(code[i].op), code[i].cmd.param); break;
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
      case OP_POW:         printf("%3d: %3d %-2s %3d\n", i, code[i].expr.lhs, opStr(code[i].op), code[i].expr.rhs); break;
      case OP_NOT:
      case OP_SIGN:        printf("%3d: %s %3d\n", i, opStr(code[i].op), code[i].expr.lhs); break;
      case VAL_STRING:     printf("%3d: %-7s %3d %3d\n", i, opStr(code[i].op), code[i].str.start, code[i].str.len); break;
      case VAL_INTEGER:    printf("%3d: %-7s %3d\n", i, opStr(code[i].op), code[i].iValue); break;
      case VAL_FLOAT:      printf("%3d: %-7s %f\n", i, opStr(code[i].op), code[i].fValue); break;
      case VAL_VAR:
      case VAL_REG:        printf("%3d: %-7s %3d\n", i, opStr(code[i].op), code[i].param); break;
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

  printf("Registers:\n");
  for (int i = 0; i < MAX_REG_NUM; i++)
    if (regs[i].name[0])
      printf("  %3d: '%.*s'\n", i, MAX_NAME, regs[i].name);
  printf("\n");

  // printf("Labels:\n");
  // for (int i = 0; i < ARRAY_SIZE(labels); i++)
  //   if (labels[i][0])
  //     printf("  %3d: '%-10.*s' -> %3d\n", i, MAX_NAME, labels[i], labelDst[i]);
  // printf("\n");
}

//-----------------------------------------------------------------------------
void debugPrintPretty(void)
{
  for (int i = 0; i < CODE_MEM; i++)
  {
    switch (code[i].op)
    {
      case CMD_PRINT:
        printf("%3d: %-7s ", i, opStr(code[i].op));
        debugPrintPrettyExpr(code[i].cmd.expr);
        printf("\n");
        break;
      case CMD_LET:
      case CMD_SET:
        printf("%3d: %-7s %c%d = ", i, opStr(code[i].op), (code[i].op == CMD_LET) ? 'V' : 'R', code[i].cmd.param);
        debugPrintPrettyExpr(code[i].cmd.expr);
        printf("\n");
        break;
      case CMD_IF:
        printf("%3d: %-7s !", i, opStr(code[i].op));
        debugPrintPrettyExpr(code[i].cmd.expr);
        printf(" GOTO %d", code[i].cmd.param);
        printf("\n");
        break;
      case CMD_GOTO:
      case CMD_GOSUB:
      case LNK_GOTO:
      case LNK_GOSUB:
        printf("%3d: %-7s %d", i, opStr(code[i].op), code[i].cmd.param);
        printf("\n");
        break;
      case CMD_RETURN:
      case CMD_NOP:
      case CMD_END:
        printf("%3d: %-7s", i, opStr(code[i].op));
        printf("\n");
        break;
      default:
        break;
    }
  }
}
