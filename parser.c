#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bytecode.h"
#include "debug.h"
#include "parser.h"
#include "common.h"

//=============================================================================
// Defines
//=============================================================================
#define UPPER_CASE(x)  (((x) >= 'a' && (x) <= 'z') ? ((x) & 0xDF) : (x))
#define parseExpr(l)   (parser[l](l))

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
static int parseStmt(void);
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
static char    varname[MAX_VAR_NUM][MAX_NAME];
static char    labels[MAX_LABELS][MAX_NAME];
static idxType labelDst[MAX_LABELS];

static const sSys* sys;
static const char* s = NULL;
int lineNum = 1;
int lineCol = 1;


//=============================================================================
// Private functions
//=============================================================================
static void readChars(int cnt, bool skipSpace)
{
  static char buffer[MAX_NAME + 2];

  if (!s)
  {
    buffer[0] = '\n';
    for (int i = 1; i < sizeof(buffer); i++)
      buffer[i] = sys->getNextChar();
    s = &buffer[1];
    putchar(*s);
  }

  while ((cnt > 0 && cnt--) || (skipSpace && *s == ' '))
  {
    lineCol = (*s == '\n') ? (++lineNum, 1) : lineCol + 1;
    memmove(buffer, buffer+1, sizeof(buffer) - 1);
    buffer[sizeof(buffer)-1] = sys->getNextChar();
    putchar(*s);
  }
}

//-----------------------------------------------------------------------------
static int keycmp(const char* kw)
{
  if (s[-1] != ' ' && s[-1] != '\n')
    return 0;

  int l = 0;
  while (kw[l] && ((s[l] & 0xDF) == kw[l])) l++;
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
  while (str[l] && (UPPER_CASE(s[l]) == str[l])) l++;
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
    if ((sys->regs[idx].name[len] == '\0'            ) &&
        (memcmp(name, sys->regs[idx].name, len) == 0))
      return idx;
  }
  return ERR_REG_NOT_FOUND;
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
static int newCode(sCodeIdx* code, eOp op)
{
  CHECK(sys->getCode(code, NEW_CODE));
  code->code.op = op;
  return code->idx;
}

//-----------------------------------------------------------------------------
static int makeCode(eOp op)
{
  sCodeIdx code;
  CHECK(sys->getCode(&code, NEW_CODE));
  code.code.op = op;
  CHECK(sys->setCode(&code));
  return code.idx;
}

//-----------------------------------------------------------------------------
static int makeCodeExpr(eOp op, int expr)
{
  sCodeIdx code;
  CHECK(sys->getCode(&code, NEW_CODE));
  code.code.op = op;
  CHECK(code.code.cmd.expr = expr);
  CHECK(sys->setCode(&code));
  return code.idx;
}

//-----------------------------------------------------------------------------
static int makeCodeExprParam(eOp op, int expr, int param)
{
  CHECK(expr);
  CHECK(param);

  sCode code =
  {
    .op        = op,
    .cmd.expr  = expr,
    .cmd.param = param
  };
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int makeExpr(eOp op, int lhs, int rhs)
{
  CHECK(lhs);
  CHECK(rhs);

  sCode code =
  {
    .op       = op,
    .expr.lhs = lhs,
    .expr.rhs = rhs
  };
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int makeVar(const char* name, int len, bool add)
{
  sCode code =
  {
    .op    = VAL_VAR,
    .param = varIndex(name, len, add)
  };
  CHECK(code.param);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int makeReg(const char* name, int len)
{
  sCode code =
  {
    .op    = VAL_REG,
    .param = regIndex(name, len)
  };
  CHECK(code.param);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int makeStr(const char* str, sLenType len)
{
  sCode code =
  {
    .op        = VAL_STRING,
    .str.start = sys->setString(str, len),
    .str.len   = len
  };
  CHECK(code.str.start);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int makeFloat(float value)
{
  sCode code =
  {
    .op     = VAL_FLOAT,
    .fValue = value
  };
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int makeInt(int value)
{
  sCode code =
  {
    .op     = VAL_INTEGER,
    .iValue = value
  };
  return sys->addCode(&code);
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
    char str[MAX_STRING];
    int  len = 0;
    readChars(1, false);
    while (*s != '"')
    {
      ENSURE(*s >= ' ', ERR_STR_INV);
      ENSURE(len < sizeof(str), ERR_STR_LENGTH);
      str[len++] = *s;
      readChars(1, false);
    }
    readChars(1, true);
    return makeStr(str, len);
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
  sCodeIdx code;
  idxType  lhs, rhs;
  CHECK(newCode(&code, CMD_PRINT));
  CHECK(lhs = parseExpr(0));
  while (strcon(";"))
  {
    if (strcon("\n"))
    {
      code.code.cmd.expr = lhs;
      return sys->setCode(&code);
    }
    CHECK(lhs = makeExpr(OP_CONCAT, lhs, parseExpr(0)));
  }
  ENSURE(strcon("\n"), ERR_NEWLINE);
  CHECK(lhs = makeExpr(OP_CONCAT, lhs, makeStr("\n", 1)));
  code.code.cmd.expr = lhs;
  return sys->setCode(&code);
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
  sCodeIdx cond;

  CHECK(newCode(&cond, CMD_IF));
  CHECK(cond.code.cmd.expr = parseExpr(0));
  ENSURE(keycon("THEN"), ERR_IF_THEN);
  if (strcon("\n"))  // multi line IF
  {
    CHECK(parseBlock());
    if (keycon("ELSE"))
    {
      sCodeIdx eob;
      ENSURE(strcon("\n"), ERR_NEWLINE);
      CHECK(newCode(&eob, CMD_GOTO));
      cond.code.cmd.param = sys->getCode(NULL, NEXT_CODE_IDX);
      CHECK(parseBlock());
      eob.code.cmd.param = sys->getCode(NULL, NEXT_CODE_IDX);
      CHECK(sys->setCode(&eob));
    }
    else
    {
      cond.code.cmd.param = sys->getCode(NULL, NEXT_CODE_IDX);
    }
    ENSURE(keycon("ENDIF"), ERR_IF_ENDIF);
    ENSURE(strcon("\n"), ERR_NEWLINE);
  }
  else  // single line IF (no ELSE allowed)
  {
    CHECK (parseStmt());
    cond.code.cmd.param = sys->getCode(NULL, NEXT_CODE_IDX);
  }
  CHECK(sys->setCode(&cond));
  return 0;
}

//-----------------------------------------------------------------------------
static int parseDo()
{
  idxType hdr = sys->getCode(NULL, NEXT_CODE_IDX);
  sCodeIdx top = { .idx = -1 };

  if (keycon("WHILE"))
  {
    CHECK(newCode(&top, CMD_IF));
    CHECK(top.code.cmd.expr = parseExpr(0));
  }
  else if (keycon("UNTIL"))
  {
    CHECK(newCode(&top, CMD_IF));
    CHECK(top.code.cmd.expr = makeExpr(OP_NOT, parseExpr(0), 0));
  }
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
  if (top.idx != -1)
  {
    top.code.cmd.param = sys->getCode(NULL, NEXT_CODE_IDX);
    CHECK(sys->setCode(&top));
  }
  return 0;
}

//-----------------------------------------------------------------------------
static int parseFor()
{
  int varExpr, varIdx, step;
  sCodeIdx code;

  CHECK(varExpr = parseVar(true));
  CHECK(sys->getCode(&code, varExpr));
  varIdx = code.code.param;
  ENSURE(strcon("="), ERR_ASSIGN);
  CHECK(makeCodeExprParam(CMD_LET, parseExpr(0), varIdx));
  ENSURE(keycon("TO"), ERR_FOR_TO);
  CHECK(newCode(&code, CMD_IF));
  CHECK(code.code.cmd.expr = makeExpr(OP_LTEQ, varExpr, parseExpr(0)));
  CHECK(step = keycon("STEP") ? parseExpr(0) : makeInt(1));
  ENSURE(strcon("\n"), ERR_NEWLINE);

  CHECK(parseBlock());

  ENSURE(keycon("NEXT"), ERR_FOR_NEXT);
  ENSURE(strcon("\n"), ERR_NEWLINE);
  CHECK(makeCodeExprParam(CMD_LET, makeExpr(OP_PLUS, varExpr, step), varIdx));
  CHECK(makeCodeExprParam(CMD_GOTO, 0, code.idx));
  code.code.cmd.param = sys->getCode(NULL, NEXT_CODE_IDX);
  CHECK(sys->setCode(&code));
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
static int parseEnd()
{
  CHECK(makeCode(CMD_END));
  ENSURE(strcon("\n"), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseLabel(const char* name, int len)
{
  CHECK(lblIndex(name, len, sys->getCode(NULL, NEXT_CODE_IDX)));
  if (!strcon("\n"));
  return parseStmt();
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
  if (keycon("END"))
    return parseEnd();
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
        keycmp("EOF") ||
        (*s == '\0'))
    {
      return 0;
    }

    CHECK(parseStmt());
  }
}

//=============================================================================
// Public functions
//=============================================================================
int parseAll(const sSys* system, int* errline, int* errcol)
{
  sys = system;
  readChars(0, true);

  int err;
  if (((err = parseBlock()                 ) >= 0) &&
      ((err = (keycon("EOF") ? 0 : ERR_EOF)) >= 0) &&
      ((err = makeCode(CMD_END)            ) >= 0))
    return 0;

  if (errline)
    *errline = lineNum;
  if (errcol)
    *errcol = lineCol;
  return err;
}

//-----------------------------------------------------------------------------
int link(const sSys* system)
{
  sCodeIdx code;

  for (idxType idx = 0; system->getCode(&code, idx) >= 0; idx++)
  {
    if (code.code.op == LNK_GOTO || code.code.op == LNK_GOSUB)
    {
      ENSURE(code.code.cmd.param < ARRAY_SIZE(labelDst), ERR_LABEL_INV);
      ENSURE(labelDst[code.code.cmd.param] != (idxType)-1, ERR_LABEL_MISSING);
      code.code.op = (code.code.op == LNK_GOTO) ? CMD_GOTO : CMD_GOSUB;
      code.code.cmd.param = labelDst[code.code.cmd.param];
      system->setCode(&code);
    }
  }
  return 0;
}
