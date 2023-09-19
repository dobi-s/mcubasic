#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bytecode.h"
#include "debug.h"
#include "parser.h"

//=============================================================================
// Defines
//=============================================================================
#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))
#define CHECK(x)       do { int _x = (x); if (_x < 0) return _x; } while(0)
#define parseExpr(l)   (parser[l](l))
#define ENSURE(x,e)    do { if (!(x)) return e; } while(0)

//=============================================================================
// Typedefs
//=============================================================================
typedef struct
{
  int         level;
  char* const str;
  eOp         operator;
} sOperators;

//-----------------------------------------------------------------------------
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
  { 0, "<>",    OP_NEQ   },
  { 0, "<=",    OP_LTEQ  },
  { 0, ">=",    OP_GTEQ  },
  { 0, "<",     OP_LT    },
  { 0, ">",     OP_GT    },
  { 0, "=",     OP_EQUAL },
  { 1, " OR",   OP_OR    },
  { 2, " AND",  OP_AND   },
  { 3, "NOT ",  OP_NOT   },
  { 4, "+",     OP_PLUS  },
  { 4, "-",     OP_MINUS },
  { 5, "MOD ",  OP_MOD   },
  { 5, "*",     OP_MULT  },
  { 5, "/",     OP_DIV   },
  { 5, "\\",    OP_IDIV  },
  { 6, "^",     OP_POW   },
  { 7, "-",     OP_SIGN  },
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
static int     nextCode = 0;
static int     nextData = CODE_MEM - 1;
static int     nextStr = 0;
static char    varname[MAX_VAR_NUM][MAX_NAME];
static char    labels[MAX_LABELS][MAX_NAME];
static idxType labelDst[MAX_LABELS];

static const char* s = NULL;

static FILE* file = NULL;
static int lineNum   = 1;
static int lineStart = 0;
static int filePos   = 0;

//=============================================================================
// Private functions
//=============================================================================
static void readChars(int cnt, bool skipSpace)
{
  static char buffer[MAX_NAME + 2];
  int c;

  if (!s)
  {
    buffer[0] = '\n';
    fread(buffer+1, 1, sizeof(buffer)-1, file);
    for (int i = 1; i < sizeof(buffer); i++)
      if (buffer[i] == '\r' || buffer[i] == '\t') buffer[i] = ' ';
    s = &buffer[1];
  }

  while ((cnt > 0 && cnt--) || (skipSpace && buffer[1] == ' '))
  {
    memmove(buffer, buffer+1, sizeof(buffer) - 1);
    filePos++;
    if (s[-1] == '\n') { lineStart = filePos; lineNum++; }
    if (file && fread(&c, 1, 1, file))
    {
      buffer[sizeof(buffer)-1] = (((char)c == '\r' || (char)c == '\t') ? ' ' : c);
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
static int namecon(const char** name)
{
  static char buf[MAX_NAME];

  ENSURE((*s >= 'A' && *s <= 'Z') ||
         (*s >= 'a' && *s <= 'z') ||
         (*s == '$' || *s == '_'), ERR_NAME_INV);

  int l = 1;
  while ((l < sizeof(buf)              ) &&
         ((s[l] >= 'A' && s[l] <= 'Z') ||
          (s[l] >= 'a' && s[l] <= 'z') ||
          (s[l] >= '0' && s[l] <= '9') ||
          (s[l] == '_'               ) ) )
  {
    l++;
  }
  memcpy(buf, s, l);
  readChars(l, true);
  if (name)
    *name = buf;
  return l;
}

//-----------------------------------------------------------------------------
static int varIndex(const char* name, int len, bool add)
{
  int idx;
  for (idx = 0; idx < ARRAY_SIZE(varname); idx++)
  {
    if ((len == MAX_NAME || varname[idx][len] == '\0') &&
        memcmp(name, varname[idx], len) == 0)
      return idx;
  }

  // not found -> add
  ENSURE(add, ERR_VAR_UNDEF);
  for (idx = 0; idx < ARRAY_SIZE(varname); idx++)
  {
    if (varname[idx][0] == '\0')
    {
      memcpy(varname[idx], name, len);
      if (len < MAX_NAME)
        varname[idx][len] = '\0';
      return idx;
    }
  }

  // no free slot found -> error
  return ERR_VAR_COUNT;
}

//-----------------------------------------------------------------------------
static int regIndex(const char* name, int len)
{
  int idx;
  for (idx = 0; idx < MAX_REG_NUM; idx++)
  {
    if ((len == MAX_NAME || regs[idx].name[len] == '\0') &&
        memcmp(name, regs[idx].name, len) == 0)
      return idx;
  }

  // not found -> add
  for (idx = 0; idx < MAX_REG_NUM; idx++)
  {
    if (regs[idx].name[0] == '\0')
    {
      memcpy(regs[idx].name, name, len);
      if (len < MAX_NAME)
        regs[idx].name[len] = '\0';
      return idx;
    }
  }

  // no free slot found -> error
  return ERR_REG_COUNT;
}

//-----------------------------------------------------------------------------
static int lblIndex(const char* name, int len, int dst)
{
  int idx;
  for (idx = 0; idx < ARRAY_SIZE(labels); idx++)
  {
    if ((len == MAX_NAME || labels[idx][len] == '\0') &&
        memcmp(name, labels[idx], len) == 0)
    {
      ENSURE(dst == -1 || labelDst[idx] == (idxType)-1, ERR_LABEL_DUPL);
      if (dst != -1) labelDst[idx] = dst;
      return idx;
    }
  }

  // not found -> add
  for (idx = 0; idx < ARRAY_SIZE(labels); idx++)
  {
    if (labels[idx][0] == '\0')
    {
      memcpy(labels[idx], name, len);
      if (len < MAX_NAME)
        labels[idx][len] = '\0';
      labelDst[idx] = dst;
      return idx;
    }
  }

  // no free slot found -> error
  return ERR_LABEL_COUNT;
}

//-----------------------------------------------------------------------------
static int makeCode(eOp op)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);
  code[nextCode].op = op;
  return nextCode++;
}

//-----------------------------------------------------------------------------
static int makeCodeExpr(eOp op, int expr)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);
  CHECK(expr);
  code[nextCode].op       = op;
  code[nextCode].cmd.expr = expr;
  return nextCode++;
}

//-----------------------------------------------------------------------------
static int makeCodeExprParam(eOp op, int expr, int param)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);
  CHECK(expr);
  CHECK(param);
  code[nextCode].op = op;
  code[nextCode].cmd.expr  = expr;
  code[nextCode].cmd.param = param;
  return nextCode++;
}

//-----------------------------------------------------------------------------
static int makeExpr(eOp op, int lhs, int rhs)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);
  CHECK(lhs);
  CHECK(rhs);
  code[nextData].op = op;
  code[nextData].expr.lhs = lhs;
  code[nextData].expr.rhs = rhs;
  return nextData--;
}

//-----------------------------------------------------------------------------
static int makeVar(const char* name, int len, bool add)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);

  code[nextData].op = VAL_VAR;
  CHECK(code[nextData].param = varIndex(name, len, add));
  return nextData--;
}

//-----------------------------------------------------------------------------
static int makeReg(const char* name, int len)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);

  code[nextData].op = VAL_REG;
  CHECK(code[nextData].param = regIndex(name, len));
  return nextData--;
}

//-----------------------------------------------------------------------------
static int makeStr(idxType start, sLenType len)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);
  code[nextData].op        = VAL_STRING;
  code[nextData].str.start = start;
  code[nextData].str.len   = len;
  return nextData--;
}

//-----------------------------------------------------------------------------
static int makeFloat(float value)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);
  code[nextData].op     = VAL_FLOAT;
  code[nextData].fValue = value;
  return nextData--;
}

//-----------------------------------------------------------------------------
static int makeInt(int value)
{
  ENSURE(nextCode <= nextData, ERR_MEM_CODE);
  code[nextData].op     = VAL_INTEGER;
  code[nextData].iValue = value;
  return nextData--;
}

//-----------------------------------------------------------------------------
static int parseVar(bool add)
{
  const char* name;
  int         len;
  ENSURE(*s != '$', ERR_VAR_NAME);
  CHECK(len = namecon(&name));
  return makeVar(name, len, add);
}

//-----------------------------------------------------------------------------
static int parseReg()
{
  const char* name;
  int         len;
  ENSURE(*s == '$', ERR_REG_NAME);
  CHECK(len = namecon(&name));
  return makeReg(name, len);
}

//-----------------------------------------------------------------------------
static int parseVal(int level)
{
  (void)level;

  // Empty
  if (*s == '\n')
    return ERR_EXPR_MISSING;

  // String
  if (*s == '"')
  {
    idxType  start = nextStr;
    readChars(1, false);
    while (*s != '"')
    {
      ENSURE(*s >= ' ', ERR_STR_INV);
      ENSURE(nextStr < sizeof(strings), ERR_STR_MEM);
      strings[nextStr++] = *s;
      readChars(1, false);
    }
    readChars(1, true);
    return makeStr(start, nextStr - start);
  }

  // Bracket
  if (strcon("("))
  {
    int a;
    CHECK(a = parseExpr(0));
    ENSURE(strcon(")"), ERR_EXPR_BRACKETS);
    return a;
  }

  // Hex
  if (memcmp(s, "0x", 2) == 0 || memcmp(s, "&H", 2) == 0)
  {
    readChars(2, false);
    char* end;
    int res = strtoul(s, &end, 16);
    ENSURE(end > s, ERR_NUM_INV);
    readChars(end-s, true);
    return makeInt(res);
  }

  // Decimal
  if (*s >= '0' && *s <= '9')
  {
    char* end;
    int res = strtoul(s, &end, 10);
    ENSURE(end > s, ERR_NUM_INV);
    if (*end == '.' || *end == 'E' || *end == 'e') // float
    {
      float val = strtof(s, &end);
      readChars(end-s, true);
      return makeFloat(val);
    }
    readChars(end-s, true);
    return makeInt(res);
  }

  // Registers
  if (*s == '$')
    return parseReg();

  // Variables
  return parseVar(false);
}

//-----------------------------------------------------------------------------
static int parseDual(int level)
{
  int op;
  int a;

  CHECK(a = parseExpr(level+1));

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

    a = makeExpr(operators[op].operator, a, parseExpr(level+1));
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
    return parseExpr(level+1);

  return makeExpr(operators[op].operator, parseExpr(level), 0);
}

//-----------------------------------------------------------------------------
static int parsePrint()
{
  CHECK(makeCodeExpr(CMD_PRINT, parseExpr(0)));
  ENSURE(strcon("\n"), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseAssign(const char* name, int len)
{
  ENSURE(strcon("="), ERR_ASSIGN);
  if (name[0] == '$')
    CHECK(makeCodeExprParam(CMD_SET, parseExpr(0), regIndex(name, len)));
  else
    CHECK(makeCodeExprParam(CMD_LET, parseExpr(0), varIndex(name, len, true)));
  ENSURE(strcon("\n"), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseGoto(eOp op)
{
  const char* name;
  int len;
  CHECK(len = namecon(&name));
  CHECK(makeCodeExprParam(op, 0, lblIndex(name, len, -1)));
  ENSURE(strcon("\n"), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseReturn()
{
  CHECK(makeCode(CMD_RETURN));
  ENSURE(strcon("\n"), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseIf()
{
  int cond;

  CHECK(cond = makeCodeExpr(CMD_IF, parseExpr(0)));
  ENSURE(keycon("THEN"), ERR_IF_THEN);
  if (strcon("\n"))  // multi line IF
  {
    CHECK(parseBlock());
    if (keycon("ELSE"))
    {
      int eob;
      ENSURE(strcon("\n"), ERR_NEWLINE);
      CHECK(eob = makeCode(CMD_GOTO));
      code[cond].cmd.param = nextCode;
      CHECK(parseBlock());
      code[eob].cmd.param = nextCode;
    }
    else
    {
      code[cond].cmd.param = nextCode;
    }
    ENSURE(keycon("ENDIF"), ERR_IF_ENDIF);
    ENSURE(strcon("\n"), ERR_NEWLINE);
    return 0;
  }
  else  // single line IF
  {
    return ERR_NOT_IMPL;
  }
}

//-----------------------------------------------------------------------------
static int parseDo()
{
  int hdr = nextCode;
  int top = -1;

  if (keycon("WHILE"))
    CHECK(top = makeCodeExpr(CMD_IF, parseExpr(0)));
  else if (keycon("UNTIL"))
    CHECK(top = makeCodeExpr(CMD_IF, makeExpr(OP_NOT, parseExpr(0), 0)));
  ENSURE(strcon("\n"), ERR_NEWLINE);

  CHECK(parseBlock());
  ENSURE(keycon("LOOP"), ERR_DO_LOOP);

  if (keycon("UNTIL"))
    CHECK(makeCodeExprParam(CMD_IF, parseExpr(0), hdr));
  else if (keycon("WHILE"))
    CHECK(makeCodeExprParam(CMD_IF, makeExpr(OP_NOT, parseExpr(0), 0), hdr));
  else
    CHECK(makeCodeExprParam(CMD_GOTO, 0, hdr));
  ENSURE(strcon("\n"), ERR_NEWLINE);
  if (top != -1)
    code[top].cmd.param = nextCode;
  return 0;
}

//-----------------------------------------------------------------------------
static int parseFor()
{
  int varExpr, varIdx, step, hdr;

  CHECK(varExpr = parseVar(true));
  varIdx = code[varExpr].param;
  ENSURE(strcon("="), ERR_ASSIGN);
  CHECK(makeCodeExprParam(CMD_LET, parseExpr(0), varIdx));
  ENSURE(keycon("TO"), ERR_FOR_TO);
  CHECK(hdr = makeCodeExpr(CMD_IF, makeExpr(OP_LTEQ, varExpr, parseExpr(0))));
  CHECK(step = keycon("STEP") ? parseExpr(0) : makeInt(1));
  ENSURE(strcon("\n"), ERR_NEWLINE);

  CHECK(parseBlock());

  ENSURE(keycon("NEXT"), ERR_FOR_NEXT);
  ENSURE(strcon("\n"), ERR_NEWLINE);
  CHECK(makeCodeExprParam(CMD_LET, makeExpr(OP_PLUS, varExpr, step), varIdx));
  CHECK(makeCodeExprParam(CMD_GOTO, 0, hdr));
  code[hdr].cmd.param = nextCode;
  return 0;
}

//-----------------------------------------------------------------------------
static int parseRem()
{
  while (*s != '\n') readChars(1, true);
  ENSURE(strcon("\n"), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseLabel(const char* name, int len)
{
  CHECK(lblIndex(name, len, nextCode));
  ENSURE(strcon("\n"), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseStmt(void)
{
  const char* name;
  int len;

  while (strcon("\n"));

  if (keycon("PRINT"))
    return parsePrint();
  if (keycon("GOTO"))
    return parseGoto(LNK_GOTO);
  if (keycon("GOSUB"))
    return parseGoto(LNK_GOSUB);
  if (keycon("RETURN"))
    return parseReturn();
  if (keycon("IF"))
    return parseIf();
  if (keycon("DO"))
    return parseDo();
  if (keycon("FOR"))
    return parseFor();
  if (keycon("REM"))
    return parseRem();
  if (keycon("LET"))
  {
    CHECK(len = namecon(&name));
    return parseAssign(name, len);
  }

  // Commands without keyword
  CHECK(len = namecon(&name));
  if (strcon(":"))  // Label
    return parseLabel(name, len);
  return parseAssign(name, len);
}

//-----------------------------------------------------------------------------
static int parseBlock(void)
{
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
      return 0;
    }

    CHECK(parseStmt());
  }
}

//-----------------------------------------------------------------------------
static const char* errmsg(int err)
{
  switch (err)
  {
    case ERR_NAME_INV:      return "Invalid name";
    case ERR_MEM_CODE:      return "Not enough code memory";
    case ERR_VAR_UNDEF:     return "Variable undefined";
    case ERR_VAR_COUNT:     return "Too many variables";
    case ERR_REG_COUNT:     return "Too many registers";
    case ERR_VAR_NAME:      return "Invalid variable name";
    case ERR_REG_NAME:      return "Invalid register name";
    case ERR_STR_INV:       return "Invalid register name";
    case ERR_STR_MEM:       return "Not enough string memory";
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
    case ERR_NOT_IMPL:      return "Not implemented yet";
    case ERR_LABEL_INV:     return "Label invalid";
    case ERR_LABEL_MISSING: return "Label missing";
    default:                return "(unknown)";
  }
}

//=============================================================================
// Public functions
//=============================================================================
int parseAll(const char* filename)
{
  int err;

  file = fopen(filename, "rb");
  if (!file) { printf("Error open file\n"); return -1; }

  readChars(0, true);

  if (((err = parseBlock()     ) >= 0) &&
      ((err = makeCode(CMD_END)) >= 0))
  {
    fclose(file);
    return 0;
  }

  printf("ERROR %d (%s) in Line %d, Col %d\n", err, errmsg(err), lineNum, filePos - lineStart + 1);
  fseek(file, lineStart, SEEK_SET);
  char line[256];
  fgets(line, sizeof(line), file);
  fclose(file);
  printf("%.*s", sizeof(line), line);
  printf("%*s\n", filePos - lineStart + 1, "^");
  return -1;
}

//-----------------------------------------------------------------------------
int link(void)
{
  for (idxType i = 0; i < nextCode; i++)
  {
    if (code[i].op == LNK_GOTO || code[i].op == LNK_GOSUB)
    {
      ENSURE(code[i].cmd.param < ARRAY_SIZE(labelDst), ERR_LABEL_INV);
      ENSURE(labelDst[code[i].cmd.param] != (idxType)-1, ERR_LABEL_MISSING);
      code[i].op = (code[i].op == LNK_GOTO) ? CMD_GOTO : CMD_GOSUB;
      code[i].cmd.param = labelDst[code[i].cmd.param];
    }
  }
  return 0;
}
