#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "bytecode.h"
#include "parser.h"
#include "debug.h"
#include "exec.h"
#include "optimizer.h"

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
int test(sCode* stack)
{
  printf("\n--- test called ---\n");
  stack[0].op     = VAL_INTEGER;
  stack[0].iValue = 123;
  return 0;
}

//=============================================================================
// Private functions
//=============================================================================
static char getNextChar(void)
{
  static const char* eofStr = "\nEOF\n";
  int c;

  while(1)
  {
    if ((c = fgetc(file)) == EOF)
      return (*eofStr) ? *(eofStr++) : '\0';

    switch ((char)c)
    {
      case '\r': continue;
      case '\t': return ' ';
      default:   return (char)c;
    }
  }
}

//-----------------------------------------------------------------------------
static int showError(int err, int line, int col)
{
  printf("%*s\n", col, "^");
  printf("ERROR %d in Line %d, Col %d: %s\n", err, line, col, errmsg(err));
  return err;
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

  if (idx < 0 || idx >= CODE_MEM)
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
  const char* const filename = "test4.bas";
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
      { "TEST", test, 2 },
    }
  };

  file = fopen(filename, "rb");
  if (!file) { printf("Error open file\n"); return -1; }



  printf("=[ Start ]==================================================================\n");

  int line, col;
  int err = parseAll(&sys, &line, &col);
  fclose(file);
  if (err < 0)
    return showError(err, line, col);
  link(&sys);

  optimize(&sys);

  printf("\n----------------------------------------------------------------------------\n");
  debugPrintRaw(&sys);
  printf("\n============================================================================\n");
  printf("'%.*s'", myNextStr, myStrings);
  printf("\n============================================================================\n");

  idxType pc = exec_init();
  while (pc >= 0)
    pc = exec(&sys, pc);

  if (pc == ERR_EXEC_END)
    printf("--- Done ---\n", pc);
  else
    printf("--- Err: %d ---\n", pc);
  return 0;
}
