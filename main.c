#include <stdio.h>
#include <stdbool.h>
#include <string.h>

//=============================================================================
// Defines
//=============================================================================
#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))

//=============================================================================
// Test program
//=============================================================================
const char prog[] =
"\n  \n"
"IF 1=1 THEN   \n"
"  PRINT \"abc\"\n"
"ELSE\n"
"  PRINT \"xyz\"\n"
"ENDIF\n"
"REM  ( 1    +   0xBEEF  )   MOD   \"2 * -3\"    OR    4 <> 7    THEN \n"
"PRINT \"Hallo\"\n"
"\0";


//=============================================================================
// Typedefs
//=============================================================================
typedef enum
{
  AST_STMTS,
  AST_CMD_PRINT,
  AST_CMD_IF,
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
  int         len;
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
  { 0, "<>",    2, AST_OP_NEQ   },
  { 0, "<=",    2, AST_OP_LTEQ  },
  { 0, ">=",    2, AST_OP_GTEQ  },
  { 0, "<",     1, AST_OP_LT    },
  { 0, ">",     1, AST_OP_GT    },
  { 0, "=",     1, AST_OP_EQUAL },
  { 1, " OR ",  4, AST_OP_OR    },
  { 2, " AND ", 5, AST_OP_AND   },
  { 3, "NOT ",  4, AST_OP_NOT   },
  { 4, "+",     1, AST_OP_PLUS  },
  { 4, "-",     1, AST_OP_MINUS },
  { 5, " MOD ", 5, AST_OP_MOD   },
  { 5, "*",     1, AST_OP_MULT  },
  { 5, "/",     1, AST_OP_DIV   },
  { 5, "\\",    1, AST_OP_IDIV  },
  { 6, "^",     1, AST_OP_POW   },
  { 7, "-",     1, AST_OP_SIGN  },
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
sAst ast[200];
int maxAst = 0;

//=============================================================================
// Private functions
//=============================================================================
static const char* readChars(int cnt, bool skipSpace)
{
  static char buffer[16];
  static int  pos = -1;

  if (pos < 0)
  {
    buffer[0] = '\0';
    memcpy(buffer+1, prog, sizeof(buffer)-1);
    pos = sizeof(buffer) - 1;
  }

  while ((cnt > 0 && cnt--) || (skipSpace && buffer[1] == ' '))
  {
    memmove(buffer, buffer+1, sizeof(buffer) - 1);
    buffer[sizeof(buffer)-1] = prog[pos++];
  }

  return &buffer[1];
}

//-----------------------------------------------------------------------------
static bool keycmp(const char* s, const char* kw)
{
  while (*kw && (*(s++) == *(kw++)));
  return (*kw == '\0' && (*s == ' ' || *s == '\n' || *s == '\0'));
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
static int parseVal(int level)
{
  (void)level;
  const char* s = readChars(0, true);

  // String
  if (*s == '"')
  {
    char str[80];
    int  len = 0;
    s = readChars(1, false);
    while (*s != '"')
    {
      if (*s < ' ')
        return -1;

      str[len++] = *s;
      s = readChars(1, false);
    }
    readChars(1, true);

    return makeAst(AST_VAL_STRING, -1, -1, -1, -1); // TODO: add string data
  }

  // Bracket
  if (*s == '(')
  {
    readChars(1, false);
    int a = parser[0](0);
    if (a < 0) return a;
    s = readChars(0, true);
    if (*s != ')')
      return -1;
    readChars(1, true);
    return a;
  }

  // Hex
  if (memcmp(s, "0x", 2) == 0 || memcmp(s, "&H", 2) == 0)
  {
    s = readChars(2, false);
    int res = 0;
    while (1)
    {
      if (*s >= '0' && *s <= '9')
        res = 16*res + (*s - '0');
      else if (*s >= 'A' && *s <= 'F')
        res = 16*res + (*s - 'A' + 10);
      else
        break;
      s = readChars(1, false);
    }
    if (res == 0 && *(s-1) != '0')
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
      s = readChars(1, false);
    }
    readChars(0, true);
    return makeAst(AST_VAL_INTEGER, res, -1, -1, -1);
  }
}

//-----------------------------------------------------------------------------
static int parseDual(int level)
{
  int a = parser[level+1](level+1);
  if (a < 0) return a;

  const char* s;
  int op;

  while (1)
  {
    s = readChars(0, true);
    for (op = 0; op < ARRAY_SIZE(operators); op++)
    {
      if (operators[op].level != level)
        continue;

      if ((operators[op].len == 1 && *s == operators[op].str[0]) ||
          (operators[op].str[0] == ' ' && memcmp(operators[op].str, s-1, operators[op].len) == 0) ||
          (memcmp(operators[op].str, s, operators[op].len) == 0))
        break;
    }
    if (op >= ARRAY_SIZE(operators))
      return a;

    readChars((operators[op].str[0] == ' ') ? operators[op].len - 1 : operators[op].len, true);
    int b = parser[level+1](level+1);
    if (b < 0) return b;

    a = makeAst(operators[op].operator, a, b, -1, -1);
  }
}

//-----------------------------------------------------------------------------
static int parsePrefix(int level)
{
  const char* s = readChars(0, true);
  int op;

  for (op = 0; op < ARRAY_SIZE(operators); op++)
  {
    if (operators[op].level != level)
      continue;

    if (memcmp(operators[op].str, s, operators[op].len) == 0)
      break;
  }
  if (op >= ARRAY_SIZE(operators))
    return parser[level+1](level+1);

  readChars(operators[op].len, true);
  int a = parser[level](level);
  if (a < 0) return a;

  return makeAst(operators[op].operator, a, -1, -1, -1);
}

//-----------------------------------------------------------------------------
static int parseStmt()
{
  const char* s = readChars(0, true);

  while (*s == '\n') s = readChars(1, true);

  if (keycmp(s, "PRINT"))
  {
    readChars(5, true);
    int a = parser[0](0);
    if (a < 0) return a;
    s = readChars(0, true);
    if (*s != '\n') return -1;
    readChars(1, true);
    return makeAst(AST_CMD_PRINT, a, -1, -1, -1);
  }

  if (keycmp(s, "IF"))
  {
    int c = -1;
    readChars(2, true);
    int a = parser[0](0);
    if (a < 0) return a;
    s = readChars(0, true);
    if (!keycmp(s-1, " THEN")) return -1;
    readChars(4, true);
    if (*s != '\n') return -1;
    readChars(1, true);
    int b = parseBlock();
    if (b < 0) return b;
    if (keycmp(s, "ELSE"))
    {
      readChars(4, true);
      if (*s != '\n') return -1;
      readChars(1, true);
      c = parseBlock();
      if (c < 0) return c;
    }
    if (!keycmp(s, "ENDIF")) return -1;
    readChars(5, true);
    if (*s != '\n') return -1;
    readChars(1, true);
    return makeAst(AST_CMD_IF, a, b, c, -1);
  }

  if (keycmp(s, "REM"))
  {
    readChars(3, true);
    while (*s != '\n') s = readChars(1, true);
    return makeAst(AST_CMD_REM, -1, -1, -1, -1);
  }

  return -1;
}

//-----------------------------------------------------------------------------
static int parseBlock()
{
  const char* s = readChars(0, true);
  int a = -1;

  while (1)
  {
    if (*s == '\n')
    {
      s = readChars(1, true);    // Skip empty lines
      continue;
    }

    if (keycmp(s, "ELSE") ||
        keycmp(s, "ENDIF") ||
        keycmp(s, "NEXT") ||
        keycmp(s, "LOOP") ||
        (*s == '\0'))
    {
      if (a == -1)
        return makeAst(AST_CMD_NOP, -1, -1, -1, -1);
      return a;
    }

    int b = parseStmt();
    if (b < 0) return b;
    a = (a == -1) ? a = b : makeAst(AST_STMTS, a, b, -1, -1);
  }
}

//=============================================================================
// Debug
//=============================================================================
void debugPrintAst(int i)
{
  switch (ast[i].op)
  {
    case AST_STMTS:
      debugPrintAst(ast[i].param[0]);
      printf("\n");
      debugPrintAst(ast[i].param[1]);
      break;
    case AST_CMD_PRINT:
      printf("PRINT ");
      debugPrintAst(ast[i].param[0]);
      break;
    case AST_CMD_IF:
      printf("IF ");
      debugPrintAst(ast[i].param[0]);
      printf(" THEN\n");
      debugPrintAst(ast[i].param[1]);
      printf("\nELSE\n");
      debugPrintAst(ast[i].param[2]);
      printf("\nENDIF");
      break;
    case AST_CMD_REM:
      printf("REM");
      break;
    case AST_CMD_NOP:
      printf("NOP");
      break;
    case AST_OP_NEQ:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" != "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_LTEQ:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" <= "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_GTEQ:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" >= "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_LT:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" < "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_GT:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" > "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_EQUAL:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" == "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_OR:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" || "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_AND:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" && "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_NOT:
      printf("(!"); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_PLUS:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" + "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_MINUS:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" - "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_MOD:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" MOD "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_MULT:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" * "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_DIV:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" / "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_IDIV:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" \\ "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_POW:
      printf("("); debugPrintAst(ast[i].param[0]); printf(" ^ "); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_OP_SIGN:
      printf("(-"); debugPrintAst(ast[i].param[1]); printf(")");
      break;
    case AST_VAL_STRING:
      printf("\"...\"");
      break;
    case AST_VAL_INTEGER:
      printf("%d", ast[i].param[0]);
      break;
    case AST_VAL_VAR:
      printf("VAR");
      break;
  }
}

//=============================================================================
// Main
//=============================================================================
int main()
{
  printf("\e[1;1H\e[2J\n============================================================================\n");

  if (parseBlock() < 0)
    printf("ERROR\n");
  printf("...%.*s\n", 15, readChars(0, false));

  printf("\n============================================================================\n");
  debugPrintAst(maxAst-1); printf("\n");
  return 0;
}
