#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "basic_common.h"
#include "basic_bytecode.h"
#include "basic_parser.h"
#include "basic_optimizer.h"
#include "basic_debug.h"
#include "basic_exec.h"

//=============================================================================
// Public variables
//=============================================================================
sCode myCode[CODE_MEM];
int   myNextCode = 0;
char  myStrings[STRING_MEM];
int   myNextStr = 0;
sReg  regs[MAX_REG_NUM];

//=============================================================================
// Private variables
//=============================================================================
static FILE* file;

//=============================================================================
// Registers, functions
//=============================================================================
int test(sCode* args, sCode* mem)
{
  printf("\n--- test called ---\n");
  args[0].op     = VAL_INTEGER;
  args[0].iValue = 123;
  return 0;
}

//-----------------------------------------------------------------------------
int readData(sCode* args, sCode* mem)
{
  // Check if 1st arg is pointer to array with length 8
  if (args[1].op != VAL_PTR || args[1].param2 != 8)
    return -1;

  mem[args[1].param + 1].op     = VAL_INTEGER;
  mem[args[1].param + 1].iValue = 12345;
  return 0;
}

//=============================================================================
// Private functions
//=============================================================================
static char getNextChar(void)
{
  int c = fgetc(file);
  return (c == EOF) ? '\0' : c;
}

//-----------------------------------------------------------------------------
static int addCode(const sCode* code)
{
  int idx;

  ENSURE(code, ERR_MEM_CODE);
  ENSURE(myNextCode < CODE_MEM, ERR_MEM_CODE);
  idx = myNextCode++;
  memcpy(&myCode[idx], code, sizeof(sCode));
  return idx;
}

//-----------------------------------------------------------------------------
static int setCode(const sCodeIdx* code)
{
  if (!code || code->idx >= CODE_MEM)
    return ERR_MEM_CODE;
  memcpy(&myCode[code->idx], &code->code, sizeof(sCode));
}

//-----------------------------------------------------------------------------
static int getCode(sCodeIdx* code, int idx)
{
  if (idx == NEW_CODE)
  {
    if (myNextCode >= CODE_MEM || !code) return ERR_MEM_CODE;
    memset(&code->code, 0x00, sizeof(sCode));
    code->idx = myNextCode++;
    return code->idx;
  }
  if (idx == NEXT_CODE_IDX)
    return myNextCode;

  if (idx < 0 || idx >= myNextCode)
    return ERR_MEM_CODE;

  memcpy(&code->code, &myCode[idx], sizeof(sCode));
  code->idx = idx;
  return idx;
}

//-----------------------------------------------------------------------------
static int setString(const char* str, unsigned int len)
{
  int j;

  for (int i = 0; i <= myNextStr; i++)
  {
    for (j = 0; j < len && j < myNextStr - i; j++)
      if (myStrings[i+j] != str[j])
        break;
    if (j == len)
      return i;
    if (j == myNextStr - i)
    {
      ENSURE(myNextStr + len <= STRING_MEM, ERR_STR_MEM);
      memcpy(&myStrings[myNextStr], &str[j], len - j);
      myNextStr += len - j;
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
  *str = &myStrings[start];
  return len;
}

//=============================================================================
// Main
//=============================================================================
int main()
{
  const char* const filename = "C:\\temp\\mcubasic\\demo\\test.bas";
  sSys sys =
  {
    .getNextChar = getNextChar,
    .addCode     = addCode,
    .getCode     = getCode,
    .setCode     = setCode,
    .getString   = getString,
    .setString   = setString,
    .regs        =
    {
      { "$TICK", NULL, NULL, 0},
    },
    .svcs        =
    {
      { "_TEST",    test,     2 },
      { "ReadData", readData, 1 },
    }
  };

  file = fopen(filename, "r");
  if (!file) { printf("Error open file\n"); return -1; }



  printf("=[ Start ]==================================================================\n");

  int line, col, err;

  if ((err = parseAll(&sys, &line, &col)) < 0)
  {
   fclose(file);
    printf("%*s\n", col, "^");
    printf("ERROR %d in Line %d, Col %d: %s\n", err, line, col, errmsg(err));
    return err;
  }
  if ((err = link(&sys)) < 0)
  {
    printf("LINK ERROR %d: %s\n", err, errmsg(err));
    return err;
  }
  if ((err = optimize(&sys)) < 0)
  {
    printf("Optimizer ERROR %d: %s\n", err, errmsg(err));
    return err;
  }

  printf("\n----------------------------------------------------------------------------\n");
  debugPrintRaw(&sys);
  printf("\n============================================================================\n");
  debugPrintString(&sys, 0, myNextStr);
  printf("\n============================================================================\n");

  idxType pc = 0;
  while (pc >= 0)
    pc = exec(&sys, pc);

  if (pc == ERR_EXEC_END)
    printf("--- Done ---\n", pc);
  else
    printf("--- Err: %d ---\n", pc);
  return 0;
}
