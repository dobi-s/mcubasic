#include <stdio.h>
#include <stdbool.h>
#include <string.h>

//=============================================================================
// Defines
//=============================================================================
#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))
#define CHECK(x)       do { int _x = (x); if (_x < 0) return _x; } while(0)

//=============================================================================
// Test program
//=============================================================================
// const char prog[] =
// "\n  \n"
// "IF 1=NOT 1 THEN   \n"
// "  PRINT \"abc\"\n"
// "ELSE\n"
// "  PRINT \"xyz\"\n"
// "ENDIF\n"
// "REM  ( 1    +   0xBEEF  )   MOD   \"2 * -3\"    OR    4 <> 7    THEN \n"
// "PRINT \"Hallo\"\n"
// "\0";


//=============================================================================
// Typedefs
//=============================================================================
typedef enum
{
  AST_STMTS,
  AST_CMD_PRINT,
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

typedef int (*fParser)(const char **, int);

//=============================================================================
// Prototypes
//=============================================================================
static int parseDual(const char** s, int level);
static int parsePrefix(const char** s, int level);
static int parseVal(const char** s, int level);
static int parseBlock(const char** s);

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
sAst ast[200];
int maxAst = 0;

//=============================================================================
// Private functions
//=============================================================================
static const char* readChars(int cnt, bool skipSpace)
{
  static char buffer[16];
  static bool init = false;
  static FILE* file = NULL;
  int c;

  if (!init)
  {
    file = fopen("test.bas", "r");
    if (!file) { printf("Error open file\n"); return &buffer[1]; }
    buffer[0] = '\n';
    fread(buffer+1, 1, sizeof(buffer)-1, file);
    init = true;
  }

  while ((cnt > 0 && cnt--) || (skipSpace && buffer[1] == ' '))
  {
    memmove(buffer, buffer+1, sizeof(buffer) - 1);
    if (file && (c = fgetc(file)) != EOF)
      buffer[sizeof(buffer)-1] = c;
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

  return &buffer[1];
}

//-----------------------------------------------------------------------------
static int keycmp(const char* s, const char* kw)
{
  if (s[-1] != ' ' && s[-1] != '\n')
    return 0;

  int l = 0;
  while (*kw && (*(s++) == *(kw++))) l++;
  return (*kw == '\0' && (*s == ' ' || *s == '\n' || *s == '\0')) ? l : 0;
}

//-----------------------------------------------------------------------------
static int keycon(const char** s, const char* kw)
{
  int l = keycmp(*s, kw);
  if (l > 0)
    *s = readChars(l, true);
  return l;
}

//-----------------------------------------------------------------------------
static int strcon(const char** s, const char* str)
{
  const char* ss = *s;
  int l = 0;
  while (*str && (*(ss++) == *(str++))) l++;
  if (*str)
    return 0;
  if (l > 0)
    *s = readChars(l, true);
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
static int parseVal(const char** s, int level)
{
  (void)level;

  // String
  if (**s == '"')
  {
    char str[80];
    int  len = 0;
    *s = readChars(1, false);
    while (**s != '"')
    {
      if (**s < ' ')
        return -1;

      str[len++] = **s;
      *s = readChars(1, false);
    }
    *s = readChars(1, true);
    return makeAst(AST_VAL_STRING, -1, -1, -1, -1); // TODO: add string data
  }

  // Bracket
  if (strcon(s, "("))
  {
    int a;
    CHECK(a = parser[0](s, 0));
    if (!strcon(s, ")")) return -1;
    return a;
  }

  // Hex
  if (memcmp(*s, "0x", 2) == 0 || memcmp(*s, "&H", 2) == 0)
  {
    *s = readChars(2, false);
    int res = 0;
    while (1)
    {
      if (**s >= '0' && **s <= '9')
        res = 16*res + (**s - '0');
      else if (**s >= 'A' && **s <= 'F')
        res = 16*res + (**s - 'A' + 10);
      else
        break;
      *s = readChars(1, false);
    }
    if (res == 0 && *(*s-1) != '0')
      return -1;
    *s = readChars(0, true);
    return makeAst(AST_VAL_INTEGER, res, -1, -1, -1);
  }

  // Decimal
  if (**s >= '0' && **s <= '9')
  {
    int res = 0;
    while(1)
    {
      if (**s >= '0' && **s <= '9')
        res = 10*res + (**s - '0');
      else
        break;
      *s = readChars(1, false);
    }
    *s = readChars(0, true);
    return makeAst(AST_VAL_INTEGER, res, -1, -1, -1);
  }
}

//-----------------------------------------------------------------------------
static int parseDual(const char** s, int level)
{
  int op;
  int a, b;

  CHECK(a = parser[level+1](s, level+1));

  while (1)
  {
    for (op = 0; op < ARRAY_SIZE(operators); op++)
    {
      if (operators[op].level != level)
        continue;

      if ((operators[op].str[0] == ' ' && keycon(s, operators[op].str+1)) ||
          (operators[op].str[0] != ' ' && strcon(s, operators[op].str)))
        break;
    }
    if (op >= ARRAY_SIZE(operators))
      return a;

    CHECK(b = parser[level+1](s, level+1));
    a = makeAst(operators[op].operator, a, b, -1, -1);
  }
}

//-----------------------------------------------------------------------------
static int parsePrefix(const char** s, int level)
{
  int op;
  int a;

  for (op = 0; op < ARRAY_SIZE(operators); op++)
  {
    if (operators[op].level != level)
      continue;

    if (strcon(s, operators[op].str))
      break;
  }
  if (op >= ARRAY_SIZE(operators))
    return parser[level+1](s, level+1);

  CHECK (a = parser[level](s, level));
  return makeAst(operators[op].operator, a, -1, -1, -1);
}

//-----------------------------------------------------------------------------
static int parseStmt(const char** s)
{
  while (strcon(s, "\n"));

  if (keycon(s, "PRINT"))
  {
    int a;
    CHECK (a = parser[0](s, 0));
    if (!strcon(s, "\n")) return -1;
    return makeAst(AST_CMD_PRINT, a, -1, -1, -1);
  }

  if (keycon(s, "IF"))
  {
    int c = -1;
    int a, b;
    CHECK(a = parser[0](s, 0));
    if (!keycon(s, "THEN")) return -1;
    if (!strcon(s, "\n")) return -1;
    CHECK(b = parseBlock(s));
    if (keycon(s, "ELSE"))
    {
      if (!strcon(s, "\n")) return -1;
      CHECK(c = parseBlock(s));
    }
    if (!keycon(s, "ENDIF")) return -1;
    if (!strcon(s, "\n")) return -1;
    return makeAst(AST_CMD_IF, a, b, c, -1);
  }

  if (keycon(s, "REM"))
  {
    while (**s != '\n') *s = readChars(1, true);
    return makeAst(AST_CMD_REM, -1, -1, -1, -1);
  }

  return -1;
}

//-----------------------------------------------------------------------------
static int parseBlock(const char** s)
{
  int a = -1;
  int b;

  while (1)
  {
    if (strcon(s, "\n"))
      continue; // Skip empty lines

    if (keycmp(*s, "ELSE") ||
        keycmp(*s, "ENDIF") ||
        keycmp(*s, "NEXT") ||
        keycmp(*s, "LOOP") ||
        (**s == '\0'))
    {
      return (a != -1) ? a : makeAst(AST_CMD_NOP, -1, -1, -1, -1);
    }

    CHECK(b = parseStmt(s));
    a = (a == -1) ? b : makeAst(AST_STMTS, a, b, -1, -1);
  }
}

//=============================================================================
// Debug
//=============================================================================
#define INDENT()      for (int n = 0; n < indent; n++) printf("  ");
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
      if (ast[i].param[1] != -1) { printf(" WHILE "); debugPrintAst(ast[i].param[1], indent+1); }
      printf("\n");
    case AST_CMD_REM:
      INDENT(); printf("REM");
      break;
    case AST_CMD_NOP:
      INDENT(); printf("NOP");
      break;

    case AST_OP_NEQ:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" != "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_LTEQ:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" <= "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_GTEQ:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" >= "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_LT:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" < "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_GT:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" > "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_EQUAL:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" == "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_OR:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" || "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_AND:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" && "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_NOT:
      printf("(!"); debugPrintAst(ast[i].param[0], 0); printf(")");
      break;
    case AST_OP_PLUS:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" + "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_MINUS:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" - "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_MOD:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" MOD "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_MULT:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" * "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_DIV:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" / "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_IDIV:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" \\ "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_POW:
      printf("("); debugPrintAst(ast[i].param[0], 0); printf(" ^ "); debugPrintAst(ast[i].param[1], 0); printf(")");
      break;
    case AST_OP_SIGN:
      printf("(-"); debugPrintAst(ast[i].param[0], 0); printf(")");
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
  printf("\e[1;1H\e[2J\n=[ Start ]==================================================================\n");

  const char* s = readChars(0, true);
  if (parseBlock(&s) < 0)
    printf("ERROR\n");
  printf("Unparsed: '%.*s'\n", 15, readChars(0, false));

  printf("\n============================================================================\n");
  debugPrintAst(maxAst-1, 0); printf("\n");
  return 0;
}
