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
#define READ_AHEAD_BUF_SIZE     (MAX_NAME + 2)

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
static char    varName[MAX_VAR_NUM][MAX_NAME];
static char    labels[MAX_LABELS][MAX_NAME];
static idxType labelDst[MAX_LABELS];
static char    subName[MAX_SUB_NUM][MAX_NAME];
static idxType subLabel[MAX_SUB_NUM];
static idxType subArgc[MAX_SUB_NUM];
static char    localName[MAX_LCL_NUM][MAX_NAME];

static int sp = 0;  // Stack pointer tracking

static const sSys* sys;
static const char* s = NULL;
int lineNum = 1;
int lineCol = 1;

//=============================================================================
// Private functions
//=============================================================================
static void readChars(int cnt, bool skipSpace)
{
  static char buffer[READ_AHEAD_BUF_SIZE];

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
static bool isKeyword(const char* str, int len)
{
  const struct
  {
    const char* name;
    int len;
  } keywords[] =
  {
    { "AND",    3 },
    { "DO",     2 },
    { "ELSE",   4 },
    { "END",    3 },
    { "ENDIF",  5 },
    { "ENDSUB", 6 },
    { "EOF",    3 },
    { "EXIT",   4 },
    { "FOR",    3 },
    { "GOTO",   4 },
    { "IF",     2 },
    { "LET",    3 },
    { "LOOP",   4 },
    { "MOD",    3 },
    { "NEXT",   4 },
    { "NOT",    3 },
    { "OR",     2 },
    { "PRINT",  5 },
    { "REM",    3 },
    { "STEP",   4 },
    { "SUB",    3 },
    { "THEN",   4 },
    { "TO",     2 },
    { "UNTIL",  5 },
    { "WHILE",  5 },
  };

  for (int i = 0; i < ARRAY_SIZE(keywords); i++)
  {
    if (len == keywords[i].len)
    {
      int j;
      for (j = 0; j < len && keywords[i].name[j] == (str[j] & 0xDF); j++);
      if (j == len) return true;
    }
  }
  return false;
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
static int chrcon(char c)
{
  if (c != *s)
    return 0;
  readChars(1, true);
  return 1;
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

  ENSURE (!isKeyword(s, l), ERR_NAME_KEYWORD);

  memcpy(buf, s, l);
  readChars(l, true);
  if (name)
    *name = buf;
  return l;
}

//-----------------------------------------------------------------------------
static int varIndex(const char* name, int len)
{
  int idx;
  for (idx = 0; idx < ARRAY_SIZE(varName); idx++)
  {
    if ((len == MAX_NAME || varName[idx][len] == '\0') &&
        memcmp(name, varName[idx], len) == 0)
      return idx;
  }

  // not found -> add
  for (idx = 0; idx < ARRAY_SIZE(varName); idx++)
  {
    if (varName[idx][0] == '\0')
    {
      memcpy(varName[idx], name, len);
      if (len < MAX_NAME)
        varName[idx][len] = '\0';
      return idx;
    }
  }

  // no free slot found -> error
  return ERR_VAR_COUNT;
}

//-----------------------------------------------------------------------------
static int subIndex(const char* name, int len)
{
  int idx;
  for (idx = 0; idx < ARRAY_SIZE(subName); idx++)
  {
    if ((len == MAX_NAME || subName[idx][len] == '\0') &&
        memcmp(name, subName[idx], len) == 0)
      return idx;
  }

  // not found -> add
  for (idx = 0; idx < ARRAY_SIZE(subName); idx++)
  {
    if (subName[idx][0] == '\0')
    {
      memcpy(subName[idx], name, len);
      if (len < MAX_NAME)
        subName[idx][len] = '\0';
      subArgc[idx] = subLabel[idx] = -1;
      return idx;
    }
  }

  // no free slot found -> error
  return ERR_SUB_COUNT;
}

//-----------------------------------------------------------------------------
static int regIndex(const char* name, int len)
{
  for (int idx = 0; idx < MAX_REG_NUM; idx++)
  {
    if ((sys->regs[idx].name                         ) &&
        (sys->regs[idx].name[len] == '\0'            ) &&
        (memcmp(name, sys->regs[idx].name, len) == 0))
      return idx;
  }
  return ERR_REG_NOT_FOUND;
}

//-----------------------------------------------------------------------------
static int svcIndex(const char* name, int len)
{
  for (int idx = 0; idx < MAX_SVC_NUM; idx++)
  {
    if ((sys->svcs[idx].name                        ) &&
        (sys->svcs[idx].name[len] == '\0'           ) &&
        (memcmp(name, sys->svcs[idx].name, len) == 0))
      return idx;
  }
  return ERR_SUB_NOT_FOUND;
}

//-----------------------------------------------------------------------------
static int localIndex(const char* name, int len)
{
  for (int idx = 0; idx < MAX_LCL_NUM; idx++)
  {
    if ((len == MAX_NAME || localName[idx][len] == '\0') &&
        memcmp(name, localName[idx], len) == 0)
      return sp - idx;
  }
  return ERR_LOCAL_NOT_FOUND;
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
static void trackStack(eOp op, int param)
{
  switch (op)
  {
    case CMD_PRINT:
    case CMD_RETURN:
      sp -= 1+param;
      break;
    case CMD_GOSUB:
    case LNK_GOSUB:
    case CMD_SVC:
      // manual
      break;
    case CMD_LET_GBL:
    case CMD_LET_LCL:
    case CMD_LET_REG:
    case CMD_IF:
    case CMD_POP:
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
      sp -= 1;
      break;
    case VAL_STRING:
    case VAL_INTEGER:
    case VAL_FLOAT:
    case VAL_VAR:
    case VAL_REG:
    case VAL_STACK:
      sp += 1;
      break;
  }
}

//-----------------------------------------------------------------------------
static int newCode(sCodeIdx* code, eOp op)
{
  CHECK(sys->getCode(code, NEW_CODE));
  code->code.op = op;
  trackStack(op, 0);
  return code->idx;
}

//-----------------------------------------------------------------------------
static int addCode(eOp op, int param)
{
  sCode code =
  {
    .op    = op,
    .param = param
  };
  CHECK(param);
  trackStack(op, param);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int addStr(const char* str, sLenType len)
{
  sCode code =
  {
    .op        = VAL_STRING,
    .str.start = sys->setString(str, len),
    .str.len   = len
  };
  CHECK(code.str.start);
  trackStack(code.op, 0);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int addFloat(float value)
{
  sCode code =
  {
    .op     = VAL_FLOAT,
    .fValue = value
  };
  trackStack(code.op, 0);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int addInt(int value)
{
  sCode code =
  {
    .op     = VAL_INTEGER,
    .iValue = value
  };
  trackStack(code.op, 0);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int parseFunc(const char* name, int len, bool sub)
{
  int argc = 0;
  idxType idx = svcIndex(name, len);
  bool svc = (idx >= 0);
  char end = sub ? '\n' : ')';

  if (!svc)
    CHECK(idx = subIndex(name, len));

  CHECK(addInt(0));  // Return value
  if (*s != end)
  {
    parseExpr(0);
    argc++;
  }
  while(chrcon(','))
  {
    parseExpr(0);
    argc++;
  }
  ENSURE(chrcon(end), sub ? ERR_NEWLINE : ERR_BRACKETS_MISS);

  if (svc)
  {
    ENSURE(argc == sys->svcs[idx].argc, ERR_ARG_MISMATCH);
    CHECK(addCode(CMD_SVC, idx));
  }
  else
  {
    ENSURE(subArgc[idx] < 0 || subArgc[idx] == argc, ERR_ARG_MISMATCH);
    CHECK(addCode(LNK_GOSUB, idx));
  }
  sp -= argc;

  if (sub)
    CHECK(addCode(CMD_POP, 0));  // Consume return value
  return 0;
}

//-----------------------------------------------------------------------------
static int parseVar(void)
{
  const char* name;
  int         len;
  ENSURE(*s != '$', ERR_VAR_NAME);
  CHECK(len = namecon(&name));
  return varIndex(name, len);
}

//-----------------------------------------------------------------------------
static int parseVal(int level)
{
  const char* name;
  int         len;
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
    return addStr(str, len);
  }

  // Bracket
  if (chrcon('('))
  {
    CHECK(parseExpr(0));
    ENSURE(chrcon(')'), ERR_EXPR_BRACKETS);
    return 0;
  }

  // Hex
  if (memcmp(s, "0x", 2) == 0 || memcmp(s, "&H", 2) == 0)
  {
    readChars(2, false);
    char* end;
    int res = strtoul(s, &end, 16);
    ENSURE(end > s, ERR_NUM_INV);
    readChars(end-s, true);
    return addInt(res);
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
      return addFloat(val);
    }
    readChars(end-s, true);
    return addInt(res);
  }

  // Registers
  if (*s == '$')
  {
    CHECK(len = namecon(&name));
    return addCode(VAL_REG, regIndex(name, len));
  }

  // Functions
  CHECK(len = namecon(&name));
  if (chrcon('('))
    return parseFunc(name, len, false);

  // Local variables
  idxType idx = localIndex(name, len);
  if (idx >= 0)
    return addCode(VAL_STACK, idx);

  // Global variables
  return addCode(VAL_VAR, varIndex(name, len));
}

//-----------------------------------------------------------------------------
static int parseDual(int level)
{
  int op;

  CHECK(parseExpr(level+1));

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
      return 0;

    CHECK(parseExpr(level+1));
    addCode(operators[op].operator, 0);
  }
}

//-----------------------------------------------------------------------------
static int parsePrefix(int level)
{
  int op;

  for (op = 0; op < ARRAY_SIZE(operators); op++)
  {
    if (operators[op].level != level)
      continue;

    if (strcon(operators[op].str))
      break;
  }
  if (op >= ARRAY_SIZE(operators))
    return parseExpr(level+1);

  CHECK(parseExpr(level));
  return addCode(operators[op].operator, 0);
}

//-----------------------------------------------------------------------------
static int parsePrint()
{
  int cnt = 0;
  CHECK(parseExpr(0));
  while (chrcon(';'))
  {
    if (chrcon('\n'))
      return addCode(CMD_PRINT, cnt);
    CHECK(parseExpr(0));
    cnt++;
  }
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  CHECK(addStr("\n", 1));
  return addCode(CMD_PRINT, cnt + 1);
}

//-----------------------------------------------------------------------------
static int parseAssign(const char* name, int len)
{
  idxType idx;
  CHECK(parseExpr(0));
  ENSURE(chrcon('\n'), ERR_NEWLINE);

  if (name[0] == '$')
    return addCode(CMD_LET_REG, regIndex(name, len));
  if ((idx = localIndex(name, len)) >= 0)
    return addCode(CMD_LET_LCL, idx);
  return addCode(CMD_LET_GBL, varIndex(name, len));
}

//-----------------------------------------------------------------------------
static int parseGoto(eOp op)
{
  const char* name;
  int len;
  CHECK(len = namecon(&name));
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return addCode(op, lblIndex(name, len, -1));
}

//-----------------------------------------------------------------------------
static int parseReturn()
{
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return addCode(CMD_RETURN, 0);
}

//-----------------------------------------------------------------------------
static int parseIf()
{
  sCodeIdx cond;

  CHECK(parseExpr(0));
  CHECK(newCode(&cond, CMD_IF));
  ENSURE(keycon("THEN"), ERR_IF_THEN);
  if (chrcon('\n'))  // multi line IF
  {
    CHECK(parseBlock());
    if (keycon("ELSE"))
    {
      sCodeIdx eob;
      ENSURE(chrcon('\n'), ERR_NEWLINE);
      CHECK(newCode(&eob, CMD_GOTO));
      CHECK(cond.code.param = sys->getCode(NULL, NEXT_CODE_IDX));
      CHECK(parseBlock());
      CHECK(eob.code.param = sys->getCode(NULL, NEXT_CODE_IDX));
      CHECK(sys->setCode(&eob));
    }
    else
    {
      CHECK(cond.code.param = sys->getCode(NULL, NEXT_CODE_IDX));
    }
    ENSURE(keycon("ENDIF"), ERR_IF_ENDIF);
    ENSURE(chrcon('\n'), ERR_NEWLINE);
  }
  else  // single line IF (no ELSE allowed)
  {
    CHECK(parseStmt());
    CHECK(cond.code.param = sys->getCode(NULL, NEXT_CODE_IDX));
  }
  CHECK(sys->setCode(&cond));
  return 0;
}

//-----------------------------------------------------------------------------
static int parseDo()
{
  sCodeIdx top = { .idx = -1 };
  idxType hdr;

  CHECK(hdr = sys->getCode(NULL, NEXT_CODE_IDX));
  if (keycon("WHILE"))
  {
    CHECK(parseExpr(0));
    CHECK(newCode(&top, CMD_IF));
  }
  else if (keycon("UNTIL"))
  {
    CHECK(parseExpr(0));
    CHECK(addCode(OP_NOT, 0));
    CHECK(newCode(&top, CMD_IF));
  }
  ENSURE(chrcon('\n'), ERR_NEWLINE);

  CHECK(parseBlock());
  ENSURE(keycon("LOOP"), ERR_DO_LOOP);

  if (keycon("UNTIL"))
  {
    CHECK(parseExpr(0));
    CHECK(addCode(CMD_IF, hdr));
  }
  else if (keycon("WHILE"))
  {
    CHECK(parseExpr(0));
    CHECK(addCode(OP_NOT, 0));
    CHECK(addCode(CMD_IF, hdr));
  }
  else
    CHECK(addCode(CMD_GOTO, hdr));
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  if (top.idx != -1)
  {
    CHECK(top.code.param = sys->getCode(NULL, NEXT_CODE_IDX));
    CHECK(sys->setCode(&top));
  }
  return 0;
}

//-----------------------------------------------------------------------------
static int parseFor()
{
  idxType hdr;
  int varIdx;
  sCodeIdx cond;

  // TODO: allow register or local variables
  CHECK(varIdx = parseVar());
  ENSURE(chrcon('='), ERR_ASSIGN);
  CHECK(parseExpr(0));
  CHECK(addCode(CMD_LET_GBL, varIdx));
  ENSURE(keycon("TO"), ERR_FOR_TO);
  CHECK(hdr = sys->getCode(NULL, NEXT_CODE_IDX));
  CHECK(addCode(VAL_VAR, varIdx));
  CHECK(parseExpr(0));
  CHECK(addCode(OP_LTEQ, 0));
  CHECK(newCode(&cond, CMD_IF));
  CHECK(keycon("STEP") ? parseExpr(0) : addInt(1));
  ENSURE(chrcon('\n'), ERR_NEWLINE);

  CHECK(parseBlock());

  ENSURE(keycon("NEXT"), ERR_FOR_NEXT);
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  CHECK(addCode(VAL_VAR, varIdx));
  CHECK(addCode(OP_PLUS, 0));
  CHECK(addCode(CMD_LET_GBL, varIdx));
  CHECK(addCode(CMD_GOTO, hdr));
  cond.code.param = sys->getCode(NULL, NEXT_CODE_IDX);
  CHECK(sys->setCode(&cond));
  return 0;
}

//-----------------------------------------------------------------------------
static int parseRem()
{
  while (*s != '\n') readChars(1, true);
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseSub()
{
  sCodeIdx skip;
  idxType idx;
  int argc = 0;
  const char* name;
  int len;

  ENSURE(localName[0][0] == '\0', ERR_NESTED_SUB);

  CHECK(len = namecon(&name));
  ENSURE(svcIndex(name, len) < 0, ERR_SUB_CONFLICT);
  idx = subIndex(name, len);
  ENSURE(subLabel[idx] == -1, ERR_SUB_REDEF);

  CHECK(newCode(&skip, CMD_GOTO));

  subLabel[idx] = sys->getCode(NULL, NEXT_CODE_IDX);
  memcpy(localName[0], name, len); if (len < MAX_NAME) localName[0][len] = '\0';

  ENSURE(chrcon('('), ERR_BRACKETS_MISS);
  if (*s != ')')
  {
    CHECK(len = namecon(&name));
    argc++;
    memcpy(localName[argc], name, len); if (len < MAX_NAME) localName[argc][len] = '\0';
  }
  while(chrcon(','))
  {
    CHECK(len = namecon(&name));
    argc++;
    memcpy(localName[argc], name, len); if (len < MAX_NAME) localName[argc][len] = '\0';
  }
  ENSURE(chrcon(')'), ERR_BRACKETS_MISS);
  ENSURE(chrcon('\n'), ERR_NEWLINE);

  subArgc[idx] = argc;
  sp = argc + 2;

  CHECK(parseBlock());

  ENSURE(keycon("ENDSUB"), ERR_END_SUB_EXP);
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  CHECK(addCode(CMD_RETURN, argc));

  CHECK(skip.code.param = sys->getCode(NULL, NEXT_CODE_IDX));
  CHECK(sys->setCode(&skip));

  // Clean up local variables
  for (int i = 0; i < ARRAY_SIZE(localName); i++)
    localName[i][0] = '\0';
  return 0;
}

//-----------------------------------------------------------------------------
static int parseEnd()
{
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return addCode(CMD_END, 0);
}

//-----------------------------------------------------------------------------
static int parseLabel(const char* name, int len)
{
  CHECK(lblIndex(name, len, sys->getCode(NULL, NEXT_CODE_IDX)));
  if (!chrcon('\n'));
  return parseStmt();
}

//-----------------------------------------------------------------------------
static int parseStmt(void)
{
  const char* name;
  int len;

  while (chrcon('\n'));

  if (keycon("PRINT"))
    return parsePrint();
  if (keycon("GOTO"))
    return parseGoto(LNK_GOTO);
  if (keycon("IF"))
    return parseIf();
  if (keycon("DO"))
    return parseDo();
  if (keycon("FOR"))
    return parseFor();
  if (keycon("REM"))
    return parseRem();
  if (keycon("SUB"))
    return parseSub();
  if (keycon("END"))
    return parseEnd();
  if (keycon("LET"))
  {
    CHECK(len = namecon(&name));
    ENSURE(chrcon('='), ERR_ASSIGN);
    return parseAssign(name, len);
  }

  // Commands without keyword
  CHECK(len = namecon(&name));
  if (chrcon(':'))  // Label
    return parseLabel(name, len);
  if (chrcon('='))  // Assignment
    return parseAssign(name, len);
  return parseFunc(name, len, true);
}

//-----------------------------------------------------------------------------
static int parseBlock(void)
{
  while (1)
  {
    if (chrcon('\n'))
      continue; // Skip empty lines

    if (keycmp("ELSE") ||
        keycmp("ENDIF") ||
        keycmp("ENDSUB") ||
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
      ((err = addCode(CMD_END, 0)          ) >= 0))
    return 0;

  for (int i = 1; i < READ_AHEAD_BUF_SIZE && s[i] != '\n'; i++)
    putchar(s[i]);
  putchar('\n');

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
    switch (code.code.op)
    {
      case LNK_GOTO:
        ENSURE(code.code.param < ARRAY_SIZE(labelDst), ERR_LABEL_INV);
        ENSURE(labelDst[code.code.param] != (idxType)-1, ERR_LABEL_MISSING);
        code.code.op    = CMD_GOTO;
        code.code.param = labelDst[code.code.param];
        system->setCode(&code);
        break;
      case LNK_GOSUB:
        ENSURE(code.code.param < ARRAY_SIZE(subLabel), ERR_LABEL_INV);
        ENSURE(subLabel[code.code.param] != (idxType)-1, ERR_SUB_NOT_FOUND);
        code.code.op    = CMD_GOSUB;
        code.code.param = subLabel[code.code.param];
        system->setCode(&code);
        break;
    }
  }
  return 0;
}
