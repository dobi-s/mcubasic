#include "basic_parser.h"
#include "basic_bytecode.h"
#include "basic_common.h"
#include "basic_config.h"
#include "basic_debug.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// Defines
//=============================================================================
#define READ_AHEAD_BUF_SIZE (MAX_NAME + 2)

#define UPPER_CASE(x)       (((x) >= 'a' && (x) <= 'z') ? ((x)&0xDF) : (x))
#define parseExpr(l)        (parser[l](l))

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
static int addInt(int value);
static int parseDual(int level);
static int parsePrefix(int level);
static int parseVal(int level);
static int parseStmt(void);
static int parseBlock(void);

//=============================================================================
// Constants
//=============================================================================
// clang-format off
static const sOperators operators[] =
{
  { 0, " XOR",  OP_XOR   },
  { 1, " OR",   OP_OR    },
  { 2, " AND",  OP_AND   },
  { 3, "NOT ",  OP_NOT   },
  { 4, "<>",    OP_NEQ   },
  { 4, "<=",    OP_LTEQ  },
  { 4, ">=",    OP_GTEQ  },
  { 4, "<",     OP_LT    },
  { 4, ">",     OP_GT    },
  { 4, "=",     OP_EQUAL },
  { 5, " SHR",  OP_SHR   },
  { 5, " SHL",  OP_SHL   },
  { 6, "+",     OP_PLUS  },
  { 6, "-",     OP_MINUS },
  { 7, " MOD",  OP_MOD   },
  { 7, "*",     OP_MULT  },
  { 7, "/",     OP_DIV   },
  { 7, "\\",    OP_IDIV  },
  { 8, "^",     OP_POW   },
  { 9, "-",     OP_SIGN  },
};

//-----------------------------------------------------------------------------
static const fParser parser[] =
{
  parseDual,      //  0: XOR
  parseDual,      //  1: OR
  parseDual,      //  2: AND
  parsePrefix,    //  3: NOT
  parseDual,      //  4: <>, <=, >=, <, >, =
  parseDual,      //  5: SHR, SHL, <<, >>
  parseDual,      //  6: +, -
  parseDual,      //  7: MOD, \, *, /
  parseDual,      //  8: ^
  parsePrefix,    //  9: -
  parseVal,       // 10: values
};
// clang-format on

//=============================================================================
// Private variables
//=============================================================================
static char    varName[MAX_VAR_NUM][MAX_NAME];
static idxType varIndex[MAX_VAR_NUM];
static idxType varLevel[MAX_VAR_NUM];
static idxType varDim[MAX_VAR_NUM];
static char    labels[MAX_LABELS][MAX_NAME];
static idxType labelDst[MAX_LABELS];
static char    subName[MAX_SUB_NUM][MAX_NAME];
static idxType subLabel[MAX_SUB_NUM];
static idxType subArgc[MAX_SUB_NUM];
#if STAT
static idxType maxVarNum;
#endif
static bool    optionExplicit = false;

static char    exitLabel[2]   = "!";
static idxType spAtBeginOfDo  = -1;
static idxType spAtBeginOfFor = -1;
static idxType exitDo         = -1;
static idxType exitFor        = -1;

static int curArgc = -1;

static int sp = 0;  // Stack pointer tracking
static int level;

static const sSys* sys;
static const char* s       = NULL;
static int         lineNum = 1;
static int         lineCol = 1;

//=============================================================================
// Private functions
//=============================================================================
static int fillBuffer(char* buffer, int len)
{
  static bool quote = false;
  char        c     = ' ';

  while (len < READ_AHEAD_BUF_SIZE - 1 && c != '\n')
  {
    c = sys->getNextChar();

    switch (c)
    {
      case '"':
        quote = !quote;
        break;
      case '\n':
        quote = false;
        break;
      case '\t':
        c = ' ';
        break;
      case '\'':
        if (!quote)
          while (c != '\n')
          {
            putchar(c);
            c = sys->getNextChar();
          }
        break;
    }

    if (c != '\r')
      buffer[++len] = c;

    if (c == '\0')
      return len;
    putchar(c);
  }
  return len;
}

//-----------------------------------------------------------------------------
static void readChars(int cnt, bool skipSpace)
{
  static char buffer[READ_AHEAD_BUF_SIZE];
  static int  buflen = 0;

  if (!s)
  {
    buffer[0] = '\n';
    buflen    = fillBuffer(buffer, 0);
    s         = &buffer[1];
  }

  while ((cnt > 0 && cnt--) || (skipSpace && *s == ' '))
  {
    lineCol = (*s == '\n') ? (++lineNum, 1) : lineCol + 1;
    memmove(buffer, buffer + 1, sizeof(buffer) - 1);
    buflen--;
    if (buflen == 0 || buffer[buflen] != '\n')
      buflen = fillBuffer(buffer, buflen);
  }
}

//-----------------------------------------------------------------------------
static bool isKeyword(const char* str, int len)
{
  // clang-format off
  const struct
  {
    const char* name;
    int         len;
  } keywords[] =
  {
    { "AND",    3 },
    { "DIM",    3 },
    { "DO",     2 },
    { "ELSE",   4 },
    { "ELSEIF", 6 },
    { "END",    3 },
    { "EXIT",   4 },
    { "FALSE",  5 },
    { "FOR",    3 },
    { "GOTO",   4 },
    { "IF",     2 },
    { "LET",    3 },
    { "LOOP",   4 },
    { "MOD",    3 },
    { "NEXT",   4 },
    { "NOT",    3 },
    { "OPTION", 6 },
    { "OR",     2 },
    { "PRINT",  5 },
    { "REM",    3 },
    { "RETURN", 6 },
    { "STEP",   4 },
    { "SUB",    3 },
    { "THEN",   4 },
    { "TO",     2 },
    { "TRUE",   4 },
    { "UNTIL",  5 },
    { "WHILE",  5 },
  };
  // clang-format on

  for (int i = 0; i < ARRAY_SIZE(keywords); i++)
  {
    if (len == keywords[i].len)
    {
      int j;
      for (j = 0; j < len && keywords[i].name[j] == (str[j] & 0xDF); j++)
        ;
      if (j == len)
        return true;
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
  while (kw[l] && ((s[l] & 0xDF) == kw[l]))
    l++;
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
  while (str[l] && (UPPER_CASE(s[l]) == str[l]))
    l++;
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

//----------------------------------------------------------------------------
static bool namecmp(const char* ref, const char* name, int len)
{
  if (len < MAX_NAME && ref[len] != '\0')  // Check len
    return false;

  for (int i = 0; i < len; i++)
    if ((ref[i] ^ name[i]) & 0xDF)
      return false;
  return true;
}

//----------------------------------------------------------------------------
static int namecon(const char** name)
{
  static char buf[MAX_NAME];

  // clang-format off
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
  // clang-format on

  ENSURE(!isKeyword(s, l), ERR_NAME_KEYWORD);

  memcpy(buf, s, l);
  readChars(l, true);
  if (name)
    *name = buf;
  return l;
}

//-----------------------------------------------------------------------------
static int getVar(const char* name, int len)
{
  for (int idx = MAX_VAR_NUM - 1; idx >= 0; idx--)
  {
    if (namecmp(varName[idx], name, len))
      return idx;
  }
  return ERR_VAR_UNDEF;
}

//-----------------------------------------------------------------------------
static int addVar(const char* name, int len, int level)
{
  int idx = getVar(name, len);
  ENSURE(idx < 0 || varLevel[idx] != level, ERR_VAR_NAME);

  for (idx = 0; idx < MAX_VAR_NUM; idx++)
  {
    if (varName[idx][0] == '\0')
    {
      memcpy(varName[idx], name, len);
      if (len < MAX_NAME)
        varName[idx][len] = '\0';
      varLevel[idx] = level;
      varDim[idx]   = 0;
#if STAT
      if (maxVarNum < idx + 1)
        maxVarNum = idx + 1;
#endif
      return idx;
    }
  }
  return ERR_VAR_COUNT;
}

//-----------------------------------------------------------------------------
static int getOrAddVar(const char* name, int len, bool allowAdd)
{
  int idx = getVar(name, len);
  if (idx < 0 && allowAdd)
  {
    CHECK(idx = addVar(name, len, level));
    varIndex[idx] = sp;
    CHECK(addInt(0));
  }
  return idx;
}

//-----------------------------------------------------------------------------
static int clrVar(int level)
{
  int cnt = 0;
  for (int idx = 0; idx < ARRAY_SIZE(varName); idx++)
  {
    if (varName[idx][0] == '\0' || varLevel[idx] < level)
      continue;
    varName[idx][0] = '\0';
    if (varDim[idx] > 0)
      cnt += varDim[idx];
    else
      cnt++;
  }
  return cnt;
}

//-----------------------------------------------------------------------------
static int regIndex(const char* name, int len)
{
  for (int idx = 0; idx < MAX_REG_NUM; idx++)
  {
    if (sys->regs[idx].name && namecmp(sys->regs[idx].name, name, len))
      return idx;
  }
  return ERR_REG_NOT_FOUND;
}

//-----------------------------------------------------------------------------
static int svcIndex(const char* name, int len)
{
  for (int idx = 0; idx < MAX_SVC_NUM; idx++)
  {
    if (sys->svcs[idx].name && namecmp(sys->svcs[idx].name, name, len))
      return idx;
  }
  return ERR_SUB_NOT_FOUND;
}

//-----------------------------------------------------------------------------
static int subIndex(const char* name, int len)
{
  int idx;
  for (idx = 0; idx < ARRAY_SIZE(subName); idx++)
  {
    if (namecmp(subName[idx], name, len))
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
static int lblIndex(const char* name, int len, int dst)
{
  int idx;
  for (idx = 0; idx < ARRAY_SIZE(labels); idx++)
  {
    if (namecmp(labels[idx], name, len))
    {
      ENSURE(dst == -1 || labelDst[idx] == (idxType)-1, ERR_LABEL_DUPL);
      if (dst != -1)
        labelDst[idx] = dst;
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
static void trackStack(eOp op, int param, int param2)
{
  switch (op)
  {
    case CMD_PRINT:
    case CMD_POP:
      sp -= param + 1;
      break;
    case CMD_RETURN:
    case CMD_GOSUB:
    case LNK_GOSUB:
    case CMD_SVC:
      // manual
      break;
    case CMD_LET_GLOBAL:
    case CMD_LET_LOCAL:
      sp -= (param2 > 0) ? 2 : 1;
      break;
    case CMD_LET_PTR:
      sp -= 2;
      break;
    case CMD_LET_REG:
    case CMD_IF:
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
      sp--;
      break;
    case VAL_ZERO:
    case VAL_INTEGER:
    case VAL_FLOAT:
    case VAL_STRING:
    case VAL_PTR:
    case CMD_GET_REG:
    case CMD_CREATE_PTR:
      sp++;
      break;
    case CMD_GET_GLOBAL:
    case CMD_GET_LOCAL:
      if (param2 == 0)
        sp++;
      break;
  }
}

//-----------------------------------------------------------------------------
static int newCode(sCodeIdx* code, eOp op)
{
  CHECK(sys->newCode(code, op));
  trackStack(op, 0, 0);
  return code->idx;
}

//-----------------------------------------------------------------------------
static int addCode(eOp op, int param)
{
  sCode code = {.op = op, .param = param, .param2 = 0};
  // Local var and ptr can have neg param
  if (op != CMD_GET_LOCAL && op != CMD_LET_LOCAL && op != CMD_GET_PTR &&
      op != CMD_LET_PTR)
    CHECK(param);
  trackStack(op, param, 0);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int addCode2(eOp op, int param, int param2)
{
  sCode code = {.op = op, .param = param, .param2 = param2};
  // Local var and ptr can have neg param / param 2
  if (op != CMD_GET_LOCAL && op != CMD_LET_LOCAL && op != CMD_GET_PTR &&
      op != CMD_LET_PTR && op != CMD_CREATE_PTR)
    CHECK(param);
  if (op != CMD_CREATE_PTR)
    CHECK(param2);
  trackStack(op, param, param2);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int addStr(const char* str, sLenType len)
{
  sCode code = {
      .op = VAL_STRING, .str.start = sys->setString(str, len), .str.len = len};
  CHECK(code.str.start);
  trackStack(code.op, 0, 0);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int addFloat(float value)
{
  sCode code = {.op = VAL_FLOAT, .fValue = value};
  trackStack(code.op, 0, 0);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int addInt(int value)
{
  sCode code = {.op = (value == 0) ? VAL_ZERO : VAL_INTEGER, .iValue = value};
  trackStack(code.op, 0, 0);
  return sys->addCode(&code);
}

//-----------------------------------------------------------------------------
static int parseArray(idxType idx)
{
  ENSURE(varDim[idx] != 0, ERR_NOT_ARRAY);
  CHECK(parseExpr(0));
  ENSURE(chrcon(')'), ERR_BRACKETS_MISS);
  if (varDim[idx] > 0)
    return addCode2(varLevel[idx] ? CMD_GET_LOCAL : CMD_GET_GLOBAL,
                    varIndex[idx], varDim[idx]);
  ENSURE(varLevel[idx] > 0, ERR_NOT_IMPL);  // Ptr can't exist globally
  return addCode(CMD_GET_PTR, varIndex[idx]);
}

//-----------------------------------------------------------------------------
static int parseFunc(const char* name, int len, bool sub)
{
  int     argc = 0;
  idxType idx  = svcIndex(name, len);
  bool    svc  = (idx >= 0);
  char    end  = sub ? '\n' : ')';

  if (!svc)
    CHECK(idx = subIndex(name, len));

  CHECK(addInt(0));  // Return value
  if (*s != end)
    do
    {
      CHECK(parseExpr(0));
      argc++;
    } while (chrcon(','));
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
    CHECK(addCode(CMD_POP, 0));  // Return value not used -> consume
  return 0;
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

  // Constants
  if (keycon("TRUE"))
    return addInt(-1);
  if (keycon("FALSE"))
    return addInt(0);

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
    int   res = strtoul(s, &end, 16);
    ENSURE(end > s, ERR_NUM_INV);
    readChars(end - s, true);
    return addInt(res);
  }

  // Decimal
  if (*s >= '0' && *s <= '9')
  {
    char* end;
    int   res = strtoul(s, &end, 10);
    ENSURE(end > s, ERR_NUM_INV);
    if (*end == '.' || *end == 'E' || *end == 'e')  // float
    {
      float val = strtof(s, &end);
      readChars(end - s, true);
      return addFloat(val);
    }
    readChars(end - s, true);
    return addInt(res);
  }

  // Registers
  if (*s == '$')
  {
    CHECK(len = namecon(&name));
    return addCode(CMD_GET_REG, regIndex(name, len));
  }

  // Functions / Arrays
  CHECK(len = namecon(&name));
  if (chrcon('('))
  {
    idxType idx = getVar(name, len);
    if (idx >= 0)
      return parseArray(idx);
    return parseFunc(name, len, false);
  }

  // Variables
  idxType idx;
  CHECK(idx = getVar(name, len));
  if (varDim[idx] != 0)  // Array without index
    return addCode2(varLevel[idx] ? CMD_CREATE_PTR : VAL_PTR, varIndex[idx],
                    varDim[idx]);
  return addCode(varLevel[idx] ? CMD_GET_LOCAL : CMD_GET_GLOBAL, varIndex[idx]);
}

//-----------------------------------------------------------------------------
static int parseDual(int level)
{
  int op;

  CHECK(parseExpr(level + 1));

  while (1)
  {
    for (op = 0; op < ARRAY_SIZE(operators); op++)
    {
      if (operators[op].level != level)
        continue;

      if ((operators[op].str[0] == ' ' && keycon(operators[op].str + 1)) ||
          (operators[op].str[0] != ' ' && strcon(operators[op].str)))
        break;
    }
    if (op >= ARRAY_SIZE(operators))
      return 0;

    CHECK(parseExpr(level + 1));
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
    return parseExpr(level + 1);

  CHECK(parseExpr(level));
  return addCode(operators[op].operator, 0);
}

//-----------------------------------------------------------------------------
static int parseDim()
{
  const char* name;
  int         len;
  idxType     idx;
  int         dim = 0;
  ENSURE(*s != '$', ERR_VAR_NAME);
  CHECK(len = namecon(&name));
  if (chrcon('('))  // Array
  {
    char* end;
    dim = strtoul(s, &end, 10);
    ENSURE(end > s, ERR_NUM_INV);
    ENSURE(dim > 0, ERR_DIM_INV);
    readChars(end - s, true);
    ENSURE(chrcon(')'), ERR_BRACKETS_MISS);
  }
  CHECK(idx = addVar(name, len, level));
  varIndex[idx] = sp;
  varDim[idx]   = dim;
  if (dim > 0)         // No assignment allowed for array
    for (int i = 0; i < dim; i++)
      CHECK(addInt(0));
  else if (chrcon('=')) // Dim with assignment
    CHECK(parseExpr(0));
  else                  // No assignment -> default = 0
    CHECK(addInt(0));
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return 0;
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
  CHECK(addStr(BASIC_OUT_EOL, strlen(BASIC_OUT_EOL)));
  return addCode(CMD_PRINT, cnt + 1);
}

//-----------------------------------------------------------------------------
static int parseAssign(const char* name, int len)
{
  bool    reg = (name[0] == '$');
  idxType idx;
  CHECK(idx = reg ? regIndex(name, len) : getOrAddVar(name, len, !optionExplicit));
  ENSURE(reg || varDim[idx] == 0, ERR_ARRAY);
  CHECK(parseExpr(0));
  ENSURE(chrcon('\n'), ERR_NEWLINE);

  if (reg)
    return addCode(CMD_LET_REG, idx);
  return addCode(varLevel[idx] ? CMD_LET_LOCAL : CMD_LET_GLOBAL, varIndex[idx]);
}

//-----------------------------------------------------------------------------
static int parseArrayAssign(const char* name, int len)
{
  idxType idx = getVar(name, len);
  ENSURE(name[0] != '$', ERR_NOT_IMPL);  // TODO: Implement array registers
  ENSURE(idx >= 0, ERR_ARRAY_NOT_FOUND);
  ENSURE(varDim[idx] != 0, ERR_NOT_ARRAY);
  CHECK(parseExpr(0));
  ENSURE(chrcon(')'), ERR_BRACKETS_MISS);
  ENSURE(chrcon('='), ERR_ASSIGN);
  CHECK(parseExpr(0));
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  if (varDim[idx] > 0)
    return addCode2(varLevel[idx] ? CMD_LET_LOCAL : CMD_LET_GLOBAL,
                    varIndex[idx], varDim[idx]);
  ENSURE(varLevel[idx] > 0, ERR_NOT_IMPL);  // Ptr can't exist globally
  return addCode(CMD_LET_PTR, varIndex[idx]);
}

//-----------------------------------------------------------------------------
static int parseExit()
{
  if (keycon("SUB"))
  {
    ENSURE(chrcon('\n'), ERR_NEWLINE);
    ENSURE(curArgc >= 0, ERR_EXIT_SUB);
    return addCode(CMD_RETURN, curArgc);
  }

  idxType* exit;
  if (keycon("DO"))
    exit = &exitDo;
  else if (keycon("FOR"))
    exit = &exitFor;
  else
    return ERR_NOT_IMPL;

  idxType spAtBegin = (exit == &exitDo) ? spAtBeginOfDo : spAtBeginOfFor;
  ENSURE(spAtBegin >= 0, (exit == &exitDo) ? ERR_EXIT_DO : ERR_EXIT_FOR);
  if (*exit < 0)
  {
    CHECK(*exit = lblIndex(exitLabel, 2, -1));
    exitLabel[1]++;
  }
  if (sp > spAtBegin)
    CHECK(addCode(CMD_POP, sp - spAtBegin - 1));
  CHECK(addCode(LNK_GOTO, *exit));
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseReturn()
{
  ENSURE(curArgc >= 0, ERR_EXIT_SUB);
  CHECK(parseExpr(0));
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  CHECK(addCode(CMD_LET_LOCAL, -curArgc - 1));
  return addCode(CMD_RETURN, curArgc);
}

//-----------------------------------------------------------------------------
static int parseGoto()
{
  const char* name;
  int         len;
  CHECK(len = namecon(&name));
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return addCode(LNK_GOTO, lblIndex(name, len, -1));
}

//-----------------------------------------------------------------------------
static int parseIf()
{
  sCodeIdx cond;
  sCodeIdx eob = {.idx = -1};

  CHECK(parseExpr(0));
  CHECK(newCode(&cond, CMD_IF));
  ENSURE(keycon("THEN"), ERR_IF_THEN);
  if (chrcon('\n'))  // multi line IF
  {
    CHECK(parseBlock());
    while (keycon("ELSEIF"))
    {
      if (eob.idx >= 0)
      {
        CHECK(eob.code.param = sys->getCodeNextIndex());
        CHECK(sys->setCode(&eob));
      }
      CHECK(newCode(&eob, CMD_GOTO));

      CHECK(cond.code.param = sys->getCodeNextIndex());
      CHECK(sys->setCode(&cond));
      CHECK(parseExpr(0));
      CHECK(newCode(&cond, CMD_IF));
      ENSURE(keycon("THEN"), ERR_IF_THEN);
      ENSURE(chrcon('\n'), ERR_NEWLINE);
      CHECK(parseBlock());
    }
    if (eob.idx >= 0)
    {
      CHECK(eob.code.param = sys->getCodeNextIndex());
      CHECK(sys->setCode(&eob));
    }

    if (keycon("ELSE"))
    {
      ENSURE(chrcon('\n'), ERR_NEWLINE);
      CHECK(newCode(&eob, CMD_GOTO));
      CHECK(cond.code.param = sys->getCodeNextIndex());
      CHECK(sys->setCode(&cond));
      CHECK(parseBlock());
      CHECK(eob.code.param = sys->getCodeNextIndex());
      CHECK(sys->setCode(&eob));
    }
    else  // End If
    {
      CHECK(cond.code.param = sys->getCodeNextIndex());
      CHECK(sys->setCode(&cond));
    }
    ENSURE(keycon("IF"), ERR_IF_ENDIF);
    ENSURE(chrcon('\n'), ERR_NEWLINE);
  }
  else  // single line IF (no ELSE allowed)
  {
    CHECK(parseStmt());
    CHECK(cond.code.param = sys->getCodeNextIndex());
    CHECK(sys->setCode(&cond));
  }
  return 0;
}

//-----------------------------------------------------------------------------
static int parseDo()
{
  sCodeIdx top = {.idx = -1};
  idxType  hdr;
  idxType  oldSp = spAtBeginOfDo;

  spAtBeginOfDo = sp;
  CHECK(hdr = sys->getCodeNextIndex());
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
    CHECK(top.code.param = sys->getCodeNextIndex());
    CHECK(sys->setCode(&top));
  }
  if (exitDo >= 0)
  {
    CHECK(labelDst[exitDo] = sys->getCodeNextIndex());
    exitDo = -1;
  }
  spAtBeginOfDo = oldSp;
  return 0;
}

//-----------------------------------------------------------------------------
static int parseFor()
{
  idxType     hdr;
  sCodeIdx    cond;
  int         varIdx;
  eOp         varSet;
  eOp         varGet;
  const char* name;
  int         len;
  idxType     oldSp = spAtBeginOfFor;
  spAtBeginOfFor    = sp;

  level++;
  ENSURE(*s != '$', ERR_VAR_NAME);
  CHECK(len = namecon(&name));
  varIdx = getOrAddVar(name, len, true);
  varGet = varLevel[varIdx] ? CMD_GET_LOCAL : CMD_GET_GLOBAL;
  varSet = varLevel[varIdx] ? CMD_LET_LOCAL : CMD_LET_GLOBAL;
  varIdx = varIndex[varIdx];

  ENSURE(chrcon('='), ERR_ASSIGN);
  CHECK(parseExpr(0));
  CHECK(addCode(varSet, varIdx));
  ENSURE(keycon("TO"), ERR_FOR_TO);
  CHECK(hdr = sys->getCodeNextIndex());
  CHECK(addCode(varGet, varIdx));
  CHECK(parseExpr(0));
  CHECK(addCode(OP_LTEQ, 0));
  CHECK(newCode(&cond, CMD_IF));
  CHECK(keycon("STEP") ? parseExpr(0) : addInt(1));
  ENSURE(chrcon('\n'), ERR_NEWLINE);

  CHECK(parseBlock());

  ENSURE(keycon("NEXT"), ERR_FOR_NEXT);
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  CHECK(addCode(varGet, varIdx));
  CHECK(addCode(OP_PLUS, 0));
  CHECK(addCode(varSet, varIdx));
  CHECK(addCode(CMD_GOTO, hdr));
  cond.code.param = sys->getCodeNextIndex();
  CHECK(sys->setCode(&cond));

  idxType cnt = clrVar(level--);
  if (cnt > 0)
    addCode(CMD_POP, cnt - 1);
  if (exitFor >= 0)
  {
    CHECK(labelDst[exitFor] = sys->getCodeNextIndex());
    exitFor = -1;
  }
  spAtBeginOfFor = oldSp;
  return 0;
}

//-----------------------------------------------------------------------------
static int parseRem()
{
  while (*s != '\n')
    readChars(1, true);
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return 0;
}

//-----------------------------------------------------------------------------
static int parseSub()
{
  sCodeIdx    skip;
  idxType     subIdx;
  idxType     varIdx;
  idxType     oldSp = sp;
  const char* name;
  int         len;

  ENSURE(level == 0, ERR_NESTED_SUB);
  curArgc = 0;

  CHECK(len = namecon(&name));
  ENSURE(svcIndex(name, len) < 0, ERR_SUB_CONFLICT);
  subIdx = subIndex(name, len);
  ENSURE(subLabel[subIdx] == -1, ERR_SUB_REDEF);

  CHECK(newCode(&skip, CMD_GOTO));

  subLabel[subIdx] = sys->getCodeNextIndex();
  CHECK(varIdx = addVar(name, len, 1));

  ENSURE(chrcon('('), ERR_BRACKETS_MISS);
  if (*s != ')')
    do
    {
      CHECK(len = namecon(&name));
      curArgc++;
      CHECK(varIdx = addVar(name, len, 1));
      if (chrcon('('))
      {
        varDim[varIdx] = -1;
        ENSURE(chrcon(')'), ERR_BRACKETS_MISS);
      }
    } while (chrcon(','));
  ENSURE(chrcon(')'), ERR_BRACKETS_MISS);
  ENSURE(chrcon('\n'), ERR_NEWLINE);

  for (int i = -curArgc; i <= 0; i++)
    varIndex[varIdx + i] = i - 1;
  subArgc[subIdx] = curArgc;

  sp = 1;
  level++;
  CHECK(parseBlock());
  CHECK(clrVar(level--));  // don't pop, this is done by return
  sp = oldSp;

  ENSURE(keycon("SUB"), ERR_END_SUB_EXP);
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  CHECK(addCode(CMD_RETURN, curArgc));
  curArgc = -1;

  CHECK(skip.code.param = sys->getCodeNextIndex());
  return sys->setCode(&skip);
}

//-----------------------------------------------------------------------------
static int parseEnd()
{
  ENSURE(chrcon('\n'), ERR_NEWLINE);
  return addCode(CMD_END, 0);
}

//-----------------------------------------------------------------------------
static int parseOption()
{
  if (keycon("EXPLICIT"))
  {
    if (keycon("OFF"))
      optionExplicit = false;
    else if (keycon("ON"))
      optionExplicit = true;
    else
      optionExplicit = true;
    ENSURE(chrcon('\n'), ERR_NEWLINE);
    return 0;
  }
  return ERR_EXPR_MISSING;
}

//-----------------------------------------------------------------------------
static int parseLabel(const char* name, int len)
{
  CHECK(lblIndex(name, len, sys->getCodeNextIndex()));
  if (!chrcon('\n'))
    return parseStmt();
  return 0;
}

//-----------------------------------------------------------------------------
static int parseStmt(void)
{
  const char* name;
  int         len;

  while (chrcon('\n'))
    ;

  if (keycon("DIM"))
    return parseDim();
  if (keycon("PRINT"))
    return parsePrint();
  if (keycon("EXIT"))
    return parseExit();
  if (keycon("RETURN"))
    return parseReturn();
  if (keycon("GOTO"))
    return parseGoto();
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
  if (keycon("OPTION"))
    return parseOption();
  if (keycon("LET"))
  {
    CHECK(len = namecon(&name));
    if (chrcon('='))  // Variable assignment
      return parseAssign(name, len);
    if (chrcon('('))  // Array assignment
      return parseArrayAssign(name, len);
    return ERR_ASSIGN;
  }

  // Commands without keyword
  CHECK(len = namecon(&name));
  if (chrcon(':'))  // Label
    return parseLabel(name, len);
  if (chrcon('='))  // Variable assignment
    return parseAssign(name, len);
  if (chrcon('('))  // Array assignment
    return parseArrayAssign(name, len);
  return parseFunc(name, len, true);
}

//-----------------------------------------------------------------------------
static int parseBlock(void)
{
  level++;
  while (1)
  {
    if (chrcon('\n'))
      continue;  // Skip empty lines

    // clang-format off
    if (keycon("END"))
    {
      if (keycmp("IF") ||
          keycmp("SUB"))
        break;
      CHECK(parseEnd());
      continue;
    }
    else if (keycmp("ELSEIF") ||
             keycmp("ELSE")   ||
             keycmp("NEXT")   ||
             keycmp("LOOP")   ||
             (*s == '\0'))
    {
      break;
    }
    // clang-format on

    CHECK(parseStmt());
  }

  // Exit block
  idxType cnt = clrVar(level--);
  if (cnt > 0)
    addCode(CMD_POP, cnt - 1);
  return 0;
}

//=============================================================================
// Public functions
//=============================================================================
int parseAll(const sSys* system, int* errline, int* errcol)
{
  // Init variables
  memset(varName, 0, sizeof(varName));
  memset(labels, 0, sizeof(labels));
  memset(subName, 0, sizeof(subName));
  exitLabel[1]   = 0;
  spAtBeginOfDo  = -1;
  spAtBeginOfFor = -1;
  exitDo         = -1;
  exitFor        = -1;
  optionExplicit = false;
#if STAT
  maxVarNum = 0;
#endif

  curArgc = -1;

  sp    = 0;
  level = -1;

  sys     = system;
  s       = NULL;
  lineNum = 1;
  lineCol = 1;

  // Start reading
  readChars(0, true);

  int err;
  if (((err = parseBlock()) >= 0) &&
      ((err = (chrcon('\0') ? 0 : ERR_EOF)) >= 0) &&
      ((err = addCode(CMD_END, 0)) >= 0))
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

  for (idxType idx = 0; sys->getCode(&code, idx) >= 0;
       idx += sys->getCodeLen(code.code.op))
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

//-----------------------------------------------------------------------------
void parseStat(int codeSize, int strSize)
{
#if STAT
  int maxSubNum = 0;
  int maxLblNum = 0;

  for (int i = 0; i < MAX_SUB_NUM; i++)
    if (subName[i][0])
      maxSubNum++;
  for (int i = 0; i < MAX_LABELS; i++)
    if (labels[i][0])
      maxLblNum++;

  // clang-format off
  printf(BASIC_OUT_EOL);
  printf("+-----------------------------------------+" BASIC_OUT_EOL);
  printf("| Code   %5.1f%% - %4d/%4d bytes         |"  BASIC_OUT_EOL, (100.0f * codeSize ) / CODE_MEM,    codeSize,  CODE_MEM);
  printf("| Sub    %5.1f%% - %4d/%4d sub functions |"  BASIC_OUT_EOL, (100.0f * maxSubNum) / MAX_SUB_NUM, maxSubNum, MAX_SUB_NUM);
  printf("| Var    %5.1f%% - %4d/%4d variables     |"  BASIC_OUT_EOL, (100.0f * maxVarNum) / MAX_VAR_NUM, maxVarNum, MAX_VAR_NUM);
  printf("| Label  %5.1f%% - %4d/%4d labels        |"  BASIC_OUT_EOL, (100.0f * maxLblNum) / MAX_LABELS,  maxLblNum, MAX_LABELS);
  printf("| Str    %5.1f%% - %4d/%4d bytes         |"  BASIC_OUT_EOL, (100.0f * strSize)   / STRING_MEM,  strSize,   STRING_MEM);
  printf("+-----------------------------------------+" BASIC_OUT_EOL);
  // clang-format on
#endif
}
