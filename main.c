#include <stdio.h>
#include <stdbool.h>
#include <string.h>

//=============================================================================
// Defines
//=============================================================================
#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))
#define CHECK(x)       do { int _x = (x); if (_x < 0) return _x; } while(0)

//=============================================================================
// Typedefs
//=============================================================================
typedef enum
{
  AST_STMTS,
  AST_CMD_PRINT,
  AST_CMD_LET,
  AST_CMD_IF,
  AST_CMD_DO,
  AST_CMD_FOR,
  AST_CMD_REM,
  AST_CMD_NOP,
  AST_OP_NEQ,
  AST_OP_LTEQ,
  AST_OP_GTEQ,
  AST_OP_LT,
  AST_OP_GT,
  AST_OP_EQUAL,
  AST_OP_OR,
  AST_OP_AND,
  AST_OP_NOT,
  AST_OP_PLUS,
  AST_OP_MINUS,
  AST_OP_MOD,
  AST_OP_MULT,
  AST_OP_DIV,
  AST_OP_IDIV,
  AST_OP_POW,
  AST_OP_SIGN,
  AST_VAL_STRING,
  AST_VAL_INTEGER,
  AST_VAL_VAR,
  AST_VAL_REG,
} eAstOp;

typedef struct sAst
{
  eAstOp op;
  int    param[4];
} sAst;

//-----------------------------------------------------------------------------
typedef struct
{
  int         level;
  char* const str;
  eAstOp      operator;
} sOperators;

typedef int (*fParser)(int);

//=============================================================================
// Prototypes
//=============================================================================
static int parseDual(int level);
static int parsePrefix(int level);
static int parseVal(int level);
static int parseBlock(void);

//=============================================================================
// Constants
//=============================================================================
static const sOperators operators[] =
{
  { 0, "<>",    AST_OP_NEQ   },
  { 0, "<=",    AST_OP_LTEQ  },
  { 0, ">=",    AST_OP_GTEQ  },
  { 0, "<",     AST_OP_LT    },
  { 0, ">",     AST_OP_GT    },
  { 0, "=",     AST_OP_EQUAL },
  { 1, " OR",   AST_OP_OR    },
  { 2, " AND",  AST_OP_AND   },
  { 3, "NOT ",  AST_OP_NOT   },
  { 4, "+",     AST_OP_PLUS  },
  { 4, "-",     AST_OP_MINUS },
  { 5, "MOD ",  AST_OP_MOD   },
  { 5, "*",     AST_OP_MULT  },
  { 5, "/",     AST_OP_DIV   },
  { 5, "\\",    AST_OP_IDIV  },
  { 6, "^",     AST_OP_POW   },
  { 7, "-",     AST_OP_SIGN  },
};

//-----------------------------------------------------------------------------
static const fParser parser[] =
{
  parseDual,      // 0: <>, <=, >=, <, >, =
  parseDual,      // 1: OR
  parseDual,      // 2: AND
  parsePrefix,    // 3: NOT
  parseDual,      // 4: +, -
  parseDual,      // 5: MOD, \, *, /
  parseDual,      // 6: ^
  parsePrefix,    // 7: -
  parseVal,       // 8: values
};

//=============================================================================
// Private variables
//=============================================================================
const char* s = NULL;
sAst ast[200];
int maxAst = 0;

//=============================================================================
// Private functions
//=============================================================================
static void readChars(int cnt, bool skipSpace)
{
  static char buffer[16];
  static FILE* file = NULL;
  int c;

  if (!s)
  {
    file = fopen("test.bas", "r");
    if (!file) { printf("Error open file\n"); return; }
    buffer[0] = '\n';
    fread(buffer+1, 1, sizeof(buffer)-1, file);
    s = &buffer[1];
  }

  while ((cnt > 0 && cnt--) || (skipSpace && buffer[1] == ' '))
  {
    memmove(buffer, buffer+1, sizeof(buffer) - 1);
    if (file && (c = fgetc(file)) != EOF)
    {
      buffer[sizeof(buffer)-1] = c;
    }
    else
    {
      if (buffer[sizeof(buffer)-2] == '\n')
      {
        buffer[sizeof(buffer)-1] = '\0';
        fclose(file);
        file = NULL;
      }
      else
        buffer[sizeof(buffer)-1] = '\n';
    }

  }
}

//-----------------------------------------------------------------------------
static int keycmp(const char* kw)
{
  if (s[-1] != ' ' && s[-1] != '\n')
    return 0;

  int l = 0;
  while (kw[l] && (s[l] == kw[l])) l++;
  return (kw[l] == '\0' && (s[l] == ' ' || s[l] == '\n')) ? l : 0;
}

//-----------------------------------------------------------------------------
static int keycon(const char* kw)
{
  int l = keycmp(kw);
  if (l > 0)
    readChars(l, true);
  return l;
}

//-----------------------------------------------------------------------------
static int strcon(const char* str)
{
  int l = 0;
  while (str[l] && (s[l] == str[l])) l++;
  if (str[l])
    return 0;
  if (l > 0)
    readChars(l, true);
  return l;
}

//-----------------------------------------------------------------------------
static int namecon(char* name)
{
  if ((*s < 'A' || *s > 'Z') && *s != '$') return -1;

  int l = 1;
  while ((l <= 10) && ((s[l] >= 'A' && s[l] <= 'Z') || (s[l] >= '0' && s[l] <= '9') || s[l] == '_')) l++;
  memcpy(name, s, l);
  readChars(l, true);
  printf("Name parsed '%.*s'\n", l, name);
  return l;
}

//-----------------------------------------------------------------------------
static int makeAst(eAstOp op, int p0, int p1, int p2, int p3)
{
  if (maxAst >= ARRAY_SIZE(ast))
    return -1;
  ast[maxAst].op       = op;
  ast[maxAst].param[0] = p0;
  ast[maxAst].param[1] = p1;
  ast[maxAst].param[2] = p2;
  ast[maxAst].param[3] = p3;
  return maxAst++;
}

//-----------------------------------------------------------------------------
static int parseVar()
{
  char name[16];
  if (*s < 'A' || *s > 'Z' || !namecon(name)) return -1;
  (void)name; // TODO: find variable index
  return makeAst(AST_VAL_VAR, 0, -1, -1, -1);
}

//-----------------------------------------------------------------------------
static int parseReg()
{
  char name[16];
  if (*s != '$' || !namecon(name)) return -1;
  (void)name; // TODO: find variable index
  return makeAst(AST_VAL_REG, 0, -1, -1, -1);
}

//-----------------------------------------------------------------------------
static int parseVal(int level)
{
  (void)level;

  // String
  if (*s == '"')
  {
    char str[80];
    int  len = 0;
    readChars(1, false);
    while (*s != '"')
    {
      if (*s < ' ')
        return -1;

      str[len++] = *s;
      readChars(1, false);
    }
    readChars(1, true);
    return makeAst(AST_VAL_STRING, -1, -1, -1, -1); // TODO: add string data
  }

  // Bracket
  if (strcon("("))
  {
    int a;
    CHECK(a = parser[0](0));
    if (!strcon(")")) return -1;
    return a;
  }

  // Hex
  if (memcmp(s, "0x", 2) == 0 || memcmp(s, "&H", 2) == 0)
  {
    readChars(2, false);
    int res = 0;
    while (1)
    {
      if (*s >= '0' && *s <= '9')
        res = 16*res + (*s - '0');
      else if (*s >= 'A' && *s <= 'F')
        res = 16*res + (*s - 'A' + 10);
      else
        break;
      readChars(1, false);
    }
    if (res == 0 && s[-1] != '0')
      return -1;
    readChars(0, true);
    return makeAst(AST_VAL_INTEGER, res, -1, -1, -1);
  }

  // Decimal
  if (*s >= '0' && *s <= '9')
  {
    int res = 0;
    while(1)
    {
      if (*s >= '0' && *s <= '9')
        res = 10*res + (*s - '0');
      else
        break;
      readChars(1, false);
    }
    readChars(0, true);
    return makeAst(AST_VAL_INTEGER, res, -1, -1, -1);
  }
}

//-----------------------------------------------------------------------------
static int parseDual(int level)
{
  int op;
  int a, b;

  CHECK(a = parser[level+1](level+1));

  while (1)
  {
    for (op = 0; op < ARRAY_SIZE(operators); op++)
    {
      if (operators[op].level != level)
        continue;

      if ((operators[op].str[0] == ' ' && keycon(operators[op].str+1)) ||
          (operators[op].str[0] != ' ' && strcon(operators[op].str)))
        break;
    }
    if (op >= ARRAY_SIZE(operators))
      return a;

    CHECK(b = parser[level+1](level+1));
    a = makeAst(operators[op].operator, a, b, -1, -1);
  }
}

//-----------------------------------------------------------------------------
static int parsePrefix(int level)
{
  int op;
  int a;

  for (op = 0; op < ARRAY_SIZE(operators); op++)
  {
    if (operators[op].level != level)
      continue;

    if (strcon(operators[op].str))
      break;
  }
  if (op >= ARRAY_SIZE(operators))
    return parser[level+1](level+1);

  CHECK (a = parser[level](level));
  return makeAst(operators[op].operator, a, -1, -1, -1);
}

//-----------------------------------------------------------------------------
static int parseStmt(void)
{
  int a = -1;
  int b = -1;
  int c = -1;
  int d = -1;

  while (strcon("\n"));

  if (keycon("PRINT"))
  {
    CHECK (a = parser[0](0));
    if (!strcon("\n")) return -1;
    return makeAst(AST_CMD_PRINT, a, b, c, d);
  }

  if (keycon("IF"))
  {
    CHECK(a = parser[0](0));
    if (!keycon("THEN")) return -1;
    if (!strcon("\n")) return -1;
    CHECK(b = parseBlock());
    if (keycon("ELSE"))
    {
      if (!strcon("\n")) return -1;
      CHECK(c = parseBlock());
    }
    if (!keycon("ENDIF")) return -1;
    if (!strcon("\n")) return -1;
    return makeAst(AST_CMD_IF, a, b, c, d);
  }

  if (keycon("DO"))
  {
    if (keycon("WHILE"))
      CHECK(b = parser[0](0));
    else if (keycon("UNTIL"))
    {
      CHECK(b = parser[0](0));
      CHECK(b = makeAst(AST_OP_NOT, b, -1, -1, -1));
    }
    if (!strcon("\n")) return -1;
    CHECK(a = parseBlock());
    if (!keycon("LOOP")) return -1;
    if (keycon("WHILE"))
      CHECK(c = parser[0](0));
    else if (keycon("UNTIL"))
    {
      CHECK(b = parser[0](0));
      CHECK(b = makeAst(AST_OP_NOT, b, -1, -1, -1));
    }
    if (!strcon("\n")) return -1;
    return makeAst(AST_CMD_DO, a, b, c, d);
  }

  if (keycon("FOR"))
  {
    int x, y;
    if ((x = parseVar()) < 0 && (x = parseReg()) < 0) return x;
    if (!strcon("=")) return -1;
    CHECK(y = parser[0](0));
    CHECK(b = makeAst(AST_CMD_LET, x, y, -1, -1));
    if (!keycon("TO")) return -1;
    CHECK(c = parser[0](0));
    if (keycon("STEP"))
      CHECK(d = parser[0](0));
    if (!strcon("\n")) return -1;
    CHECK(a = parseBlock());
    if (!keycon("NEXT")) return -1;
    if (!strcon("\n")) return -1;
    return makeAst(AST_CMD_FOR, a, b, c, d);
  }

  if (keycon("REM"))
  {
    while (*s != '\n') readChars(1, true);
    return makeAst(AST_CMD_REM, a, b, c, d);
  }

  return -1;
}

//-----------------------------------------------------------------------------
static int parseBlock(void)
{
  int a = -1;
  int b;

  while (1)
  {
    if (strcon("\n"))
      continue; // Skip empty lines

    if (keycmp("ELSE") ||
        keycmp("ENDIF") ||
        keycmp("NEXT") ||
        keycmp("LOOP") ||
        (*s == '\0'))
    {
      return (a != -1) ? a : makeAst(AST_CMD_NOP, -1, -1, -1, -1);
    }

    CHECK(b = parseStmt());
    a = (a == -1) ? b : makeAst(AST_STMTS, a, b, -1, -1);
  }
}

//=============================================================================
// Debug
//=============================================================================
#define INDENT()      for (int n = 0; n < indent; n++) printf("  ");

//-----------------------------------------------------------------------------
static const char* astOp(eAstOp op)
{
  switch (op)
  {
    case AST_STMTS:       return "STM";
    case AST_CMD_PRINT:   return "PRINT";
    case AST_CMD_LET:     return "LET";
    case AST_CMD_IF:      return "IF";
    case AST_CMD_DO:      return "DO";
    case AST_CMD_FOR:     return "FOR";
    case AST_CMD_REM:     return "REM";
    case AST_CMD_NOP:     return "NOP";
    case AST_OP_NEQ:      return "!=";
    case AST_OP_LTEQ:     return "<=";
    case AST_OP_GTEQ:     return ">=";
    case AST_OP_LT:       return "< ";
    case AST_OP_GT:       return "> ";
    case AST_OP_EQUAL:    return "==";
    case AST_OP_OR:       return "||";
    case AST_OP_AND:      return "&&";
    case AST_OP_NOT:      return "!";
    case AST_OP_PLUS:     return "+";
    case AST_OP_MINUS:    return "-";
    case AST_OP_MOD:      return "%";
    case AST_OP_MULT:     return "*";
    case AST_OP_DIV:      return "/";
    case AST_OP_IDIV:     return "\\";
    case AST_OP_POW:      return "^";
    case AST_OP_SIGN:     return "-";
    case AST_VAL_STRING:  return "STR";
    case AST_VAL_INTEGER: return "INT";
    case AST_VAL_VAR:     return "VAR";
    case AST_VAL_REG:     return "REG";
    default:              return "(?)";
  }
}

//-----------------------------------------------------------------------------
void debugPrintAstRaw()
{
  for (int i = 0; i < maxAst; i++)
    printf("%3d: %-5s  %3d %3d %3d %3d\n", i, astOp(ast[i].op), ast[i].param[0], ast[i].param[1], ast[i].param[2], ast[i].param[3]);
}

//-----------------------------------------------------------------------------
void debugPrintAst(int i, int indent)
{
  switch (ast[i].op)
  {
    case AST_STMTS:
      debugPrintAst(ast[i].param[0], indent); printf("\n");
      debugPrintAst(ast[i].param[1], indent);
      break;

    case AST_CMD_PRINT:
      INDENT(); printf("PRINT ");
      debugPrintAst(ast[i].param[0], indent);
      break;
    case AST_CMD_LET:
      INDENT();
      debugPrintAst(ast[i].param[0], indent);
      printf(" = ");
      debugPrintAst(ast[i].param[1], indent);
      break;
    case AST_CMD_IF:
      INDENT(); printf("IF ");
      debugPrintAst(ast[i].param[0], indent);
      INDENT(); printf(" THEN\n");
      debugPrintAst(ast[i].param[1], indent+1); printf("\n");
      INDENT(); printf("ELSE\n");
      debugPrintAst(ast[i].param[2], indent+1); printf("\n");
      INDENT(); printf("ENDIF");
      break;
    case AST_CMD_DO:
      INDENT(); printf("DO");
      if (ast[i].param[1] != -1) { printf(" WHILE "); debugPrintAst(ast[i].param[1], indent+1); }
      printf("\n");
      debugPrintAst(ast[i].param[0], indent+1); printf("\n");
      INDENT(); printf("LOOP");
      if (ast[i].param[2] != -1) { printf(" WHILE "); debugPrintAst(ast[i].param[2], indent+1); }
      break;
    case AST_CMD_FOR:
      INDENT(); printf("FOR ");
      debugPrintAst(ast[i].param[1], 0);
      printf(" TO ");
      debugPrintAst(ast[i].param[2], 0);
      if (ast[i].param[3] != -1) { printf(" STEP "); debugPrintAst(ast[i].param[3], 0); }
      printf("\n");
      debugPrintAst(ast[i].param[0], indent+1); printf("\n");
      INDENT(); printf("NEXT");
      break;
    case AST_CMD_REM:
      INDENT(); printf("REM");
      break;
    case AST_CMD_NOP:
      INDENT(); printf("NOP");
      break;

    case AST_OP_NEQ:
    case AST_OP_LTEQ:
    case AST_OP_GTEQ:
    case AST_OP_LT:
    case AST_OP_GT:
    case AST_OP_EQUAL:
    case AST_OP_OR:
    case AST_OP_AND:
    case AST_OP_PLUS:
    case AST_OP_MINUS:
    case AST_OP_MOD:
    case AST_OP_MULT:
    case AST_OP_DIV:
    case AST_OP_IDIV:
    case AST_OP_POW:
      printf("(");
      debugPrintAst(ast[i].param[0], 0);
      printf(" %s ", astOp(ast[i].op));
      debugPrintAst(ast[i].param[1], 0);
      printf(")");
      break;
    case AST_OP_NOT:
    case AST_OP_SIGN:
      printf("(");
      printf("%s", astOp(ast[i].op));
      debugPrintAst(ast[i].param[0], 0);
      printf(")");
      break;

    case AST_VAL_STRING:
      printf("\"...\"");
      break;
    case AST_VAL_INTEGER:
      printf("%d", ast[i].param[0]);
      break;
    case AST_VAL_VAR:
    case AST_VAL_REG:
      printf("%s%d", astOp(ast[i].op), ast[i].param[0]);
      break;
  }
}

//=============================================================================
// Main
//=============================================================================
int main()
{
  printf("\e[1;1H\e[2J\n=[ Start ]==================================================================\n");

  readChars(0, true);
  if (parseBlock() < 0)
    printf("ERROR\n");
  printf("Unparsed: '%.*s'\n", 15, s);

  printf("\n----------------------------------------------------------------------------\n");
  debugPrintAstRaw();
  printf("\n============================================================================\n");
  debugPrintAst(maxAst-1, 0); printf("\n");
  return 0;
}
