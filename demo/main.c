#include "basic_bytecode.h"
#include "basic_common.h"
#include "basic_debug.h"
#include "basic_exec.h"
#include "basic_optimizer.h"
#include "basic_parser.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PC specific (for GetTickCount())
// Implement your own tickCount() depending on your hardware
#include <sysinfoapi.h>

//=============================================================================
// Private variables
//=============================================================================
static char codeMem[CODE_MEM];
static char strings[STRING_MEM];

// loading
static idxType codeLen = 0;
static idxType strLen  = 0;
static FILE*   file;

// executing
static idxType pc      = 0;
static int     sleepMs = 0;

// hardware - implement your own functions
#define IO_LED 0
static bool led;
static int  sysTickMs()
{
  return GetTickCount();
};

//=============================================================================
// Registers
//=============================================================================
static int getTick(sCode* val, int cookie)
{
  val->op     = VAL_INTEGER;
  val->iValue = sysTickMs();
  return 0;
}

//-----------------------------------------------------------------------------
static int getDout(sCode* val, int cookie)
{
  val->op     = VAL_INTEGER;
  val->iValue = led ? -1 : 0;  // TODO: read HW
  return 0;
}

//-----------------------------------------------------------------------------
static int setDout(sCode* val, int cookie)
{
  if (val->op != VAL_INTEGER && val->op != VAL_FLOAT)
    return ERR_EXEC_VAR_INV;

  led = (val->op == VAL_INTEGER) ? val->iValue : val->fValue;  // TODO: set HW
  return 0;
}

//=============================================================================
// Buildin functions
//=============================================================================
static int iif(sCode* args, sCode* mem)
{
  bool cond;
  if (args[1].op == VAL_INTEGER)
    cond = (args[1].iValue != 0);
  else if (args[1].op == VAL_FLOAT)
    cond = (args[1].fValue != 0.0f);
  else
    return ERR_EXEC_VAR_INV;

  memcpy(&args[0], &args[cond ? 2 : 3], sizeof(args[0]));
  return 0;
}

//-----------------------------------------------------------------------------
static int sleep(sCode* args, sCode* mem)
{
  if (args[1].op == VAL_INTEGER)
    sleepMs = args[1].iValue * 1000;
  else if (args[1].op == VAL_FLOAT)
    sleepMs = args[1].fValue * 1000.0f;
  else
    return ERR_EXEC_VAR_INV;

  if (sleepMs <= 0)
    sleepMs = 1;

  // No return value
  return 0;
}

//=============================================================================
// Private system functions
//=============================================================================
static char getNextChar(void)
{
  if (!file)
    return '\0';
  int c = fgetc(file);
  return (c == EOF) ? '\0' : c;
}

//-----------------------------------------------------------------------------
static int getCodeLen(eOp op)
{
  switch (op)
  {
    case CMD_INVALID:
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
    case OP_NOT:
    case OP_SHL:
    case OP_SHR:
    case OP_PLUS:
    case OP_MINUS:
    case OP_MOD:
    case OP_MULT:
    case OP_DIV:
    case OP_IDIV:
    case OP_POW:
    case OP_SIGN:
    case VAL_ZERO:
      return 1;
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
      return 3;
    case CMD_LET_GLOBAL:
    case CMD_LET_LOCAL:
    case CMD_GET_GLOBAL:
    case CMD_GET_LOCAL:
    case CMD_CREATE_PTR:
    case VAL_INTEGER:
    case VAL_FLOAT:
    case VAL_STRING:
    case VAL_PTR:
      return 5;
    default:
      return ERR_EXEC_CMD_INV;
  }
}

//-----------------------------------------------------------------------------
static int addCode(const sCode* code)
{
  int idx = codeLen;
  int len = getCodeLen(code->op);

  ENSURE(code, ERR_MEM_CODE);
  ENSURE(codeLen + len <= CODE_MEM, ERR_MEM_CODE);
  codeMem[idx] = code->op;
  if (len > 1)
    memcpy(&codeMem[idx + 1], &code->param, len - 1);
  codeLen += len;
  return idx;
}

//-----------------------------------------------------------------------------
static int setCode(const sCodeIdx* code)
{
  if (!code || code->idx >= CODE_MEM)
    return ERR_MEM_CODE;
  int len            = getCodeLen(code->code.op);
  codeMem[code->idx] = code->code.op;
  if (len > 1)
    memcpy(&codeMem[code->idx + 1], &code->code.param, len - 1);
  return 0;
}

//-----------------------------------------------------------------------------
static int newCode(sCodeIdx* code, eOp op)
{
  if (!code)
    return ERR_MEM_CODE;
  code->code.op = op;
  code->idx     = addCode(&code->code);
  return code->idx;
}

//-----------------------------------------------------------------------------
static int getCode(sCodeIdx* code, int idx)
{
  if (idx < 0 || idx >= CODE_MEM)
    return ERR_MEM_CODE;

  code->idx     = idx;
  code->code.op = (eOp)codeMem[idx];
  int len       = getCodeLen(code->code.op);
  if (len > 1)
    memcpy(&code->code.param, &codeMem[code->idx + 1], len - 1);
  return idx;
}

//-----------------------------------------------------------------------------
static int getCodeNextIndex(void)
{
  return codeLen;
}

//-----------------------------------------------------------------------------
static int setString(const char* str, unsigned int len)
{
  int j;

  for (int i = 0; i <= strLen; i++)
  {
    for (j = 0; j < len && j < strLen - i; j++)
      if (strings[i + j] != str[j])
        break;
    if (j == len)
      return i;
    if (j == strLen - i)
    {
      ENSURE(strLen + len <= STRING_MEM, ERR_STR_MEM);
      memcpy(&strings[strLen], &str[j], len - j);
      strLen += len - j;
      return i;
    }
  }
  return ERR_STR_MEM;  // Never comes here
}

//-----------------------------------------------------------------------------
static int getString(const char** str, int start, unsigned int len)
{
  if (start + len > STRING_MEM)
    return ERR_STR_MEM;
  *str = &strings[start];
  return len;
}

//=============================================================================
// System
//=============================================================================
// clang-format off
static sSys sys =
{
  .getNextChar      = getNextChar,
  .addCode          = addCode,
  .newCode          = newCode,
  .getCode          = getCode,
  .setCode          = setCode,
  .getCodeNextIndex = getCodeNextIndex,
  .getCodeLen       = getCodeLen,
  .getString        = getString,
  .setString        = setString,
  .regs             =
  {
    { "$TICK",      getTick, NULL,    0      },
    { "$LED",       getDout, setDout, IO_LED },
  },
  .svcs        =
  {
    { "Iif",      iif,     3 },  // Iif(cond, t, f)     -> (cond ? t : f)
    { "Sleep",    sleep,   1 },  // Sleep(sec)          -> void
  }
};
// clang-format on

//=============================================================================
// Private functions
//=============================================================================
static void clear(void)
{
  pc      = ERR_EXEC_END;
  codeLen = 0;
  strLen  = 0;
  memset(codeMem, 0, sizeof(codeMem));
  memset(strings, 0, sizeof(strings));
}

//-----------------------------------------------------------------------------
static bool save(void)
{
  // Mandadory data to save:
  // - codeMem  (at least the first codeLen bytes)
  // - strings  (at least the first strLen bytes)
  // Recommended data to save:
  // - codeLen  (helps to save/read only needed data)
  // - strLen   (helps to save/read only needed data)
  // - CRC      (ensure data integrity)
  // - Version  (ensure compatibility: registers, buildin functions, ...)
  return true;
}

//-----------------------------------------------------------------------------
static bool load(void)
{
  // load data -> must match save()
  return true;
}

//=============================================================================
// Public functions
//=============================================================================
void BasicInit(void)
{
  // TODO: prepare system hardware

  if (!load())
  {
    printf("BASIC: Load error" BASIC_OUT_EOL);
    clear();
  }

  // Autostart
  pc = ((eOp)codeMem[0] != CMD_INVALID) ? 0 : ERR_EXEC_END;
}

//-----------------------------------------------------------------------------
bool BasicTask(int interval)
{
  int start = sysTickMs();

  if (sleepMs > 0)
    sleepMs -= interval;

  while (sleepMs <= 0 && pc >= 0 && sysTickMs() - start < 2)
  {
    pc = exec(&sys, pc);
    if (pc == ERR_EXEC_END)
      printf("BASIC: done" BASIC_OUT_EOL);
    else if (pc < 0)
      printf("BASIC: Runtime error %d" BASIC_OUT_EOL, pc);
  }
  return (pc >= 0);
}

//=============================================================================
// Main
//=============================================================================
int main()
{
  printf(BASIC_OUT_EOL
         "=[ Load ]==========================================================="
         "=" BASIC_OUT_EOL);
  {
    const char* const filename = "demo\\test.bas";
    int               line, col, err;

    file = fopen(filename, "r");
    if (!file)
    {
      printf("Error open file" BASIC_OUT_EOL);
      return -1;
    }

    clear();

    if ((err = parseAll(&sys, &line, &col)) < 0)
    {
      printf("%*s" BASIC_OUT_EOL, col, "^");
      printf("ERROR %d in Line %d, Col %d: %s" BASIC_OUT_EOL, err, line, col,
             errmsg(err));
      return 1;
    }
    if ((err = link(&sys)) < 0)
    {
      printf("LINK ERROR %d: %s" BASIC_OUT_EOL, err, errmsg(err));
      return 1;
    }
    if ((err = optimize(&sys)) < 0)
    {
      printf("Optimizer ERROR %d: %s" BASIC_OUT_EOL, err, errmsg(err));
      return 1;
    }
    parseStat(codeLen, strLen);
    save();
  }

  printf(BASIC_OUT_EOL
         "=[ List ]==========================================================="
         "=" BASIC_OUT_EOL);
  {
    debugPrintRaw(&sys);
    printf("Str = '");
    for (int i = 0; i < strLen; i++)
    {
      if (strings[i] >= ' ' && strings[i] < 0x7F)
        putchar(strings[i]);
      else
        printf("[%02x]", strings[i]);
    }
    printf("'" BASIC_OUT_EOL);
  }

  printf(BASIC_OUT_EOL
         "=[ Exec ]==========================================================="
         "=" BASIC_OUT_EOL);
  {
    const int interval = 10;
    int       lastCall = sysTickMs();

    BasicInit();

    // Run task every <interval> ms
    while (1)
    {
      if (!BasicTask(interval))
        break;  // embedded systems usually don't end the task loop

      // wait until interval expired (to some other tasks)
      while (sysTickMs() - lastCall < interval)
        ;
    }
  }
}
