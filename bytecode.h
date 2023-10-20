#pragma once

#include <stdint.h>
#include "config.h"

//=============================================================================
// Defines
//=============================================================================
#define NEW_CODE        -1
#define NEXT_CODE_IDX   -2

//=============================================================================
// Typedefs
//=============================================================================
typedef enum
{                   // Code    Linker    Arg     Stack
  CMD_INVALID = 0,  //  X                -       -
  CMD_PRINT,        //  X                <cnt>   -1-<cnt>
  CMD_LET,          //  X                <var>   -1
  CMD_SET,          //  X                <reg>   -1
  CMD_IF,           //  X                <lbl>   -1
  CMD_GOTO,         //  X                <lbl>   -
  LNK_GOTO,         //          X        <lbl>   -
  CMD_GOSUB,        //  X                <lbl>   -
  LNK_GOSUB,        //          X        <lbl>   -
  CMD_RETURN,       //  X                <cnt>   -1-<cnt>
  CMD_POP,          //  X                -       -1
  CMD_NOP,          //  X                -       -
  CMD_END,          //  X                -       -
  CMD_SVC,          //  X                <func>  -<func>.argc
  OP_NEQ,           //  X                -       -2+1
  OP_LTEQ,          //  X                -       -2+1
  OP_GTEQ,          //  X                -       -2+1
  OP_LT,            //  X                -       -2+1
  OP_GT,            //  X                -       -2+1
  OP_EQUAL,         //  X                -       -2+1
  OP_OR,            //  X                -       -2+1
  OP_AND,           //  X                -       -2+1
  OP_NOT,           //  X                -       -1+1
  OP_PLUS,          //  X                -       -2+1
  OP_MINUS,         //  X                -       -2+1
  OP_MOD,           //  X                -       -2+1
  OP_MULT,          //  X                -       -2+1
  OP_DIV,           //  X                -       -2+1
  OP_IDIV,          //  X                -       -2+1
  OP_POW,           //  X                -       -2+1
  OP_SIGN,          //  X                -       -1+1
  VAL_STRING,       //  X                <str>   +1
  VAL_INTEGER,      //  X                <int>   +1
  VAL_FLOAT,        //  X                <float> +1
  VAL_VAR,          //  X                <var>   +1
  VAL_REG,          //  X                <reg>   +1
  VAL_LABEL,        //                   <lbl>
} eOp;

//-----------------------------------------------------------------------------
typedef int32_t   iType;      // Integer value
typedef float     fType;      // Float value
typedef int16_t   idxType;    // Code/var/reg/str index
typedef int16_t   sLenType;   // String length

//-----------------------------------------------------------------------------
typedef struct sCode
{
  eOp           op;
  union
  {
    iType       iValue;
    fType       fValue;
    idxType     param;    // var (VAR, LET), reg (REG, SET), lbl (IF, GOTO, GOSUB)
    struct
    {
      idxType   start;
      sLenType  len;
    } str;
  };
} sCode;

//-----------------------------------------------------------------------------
typedef struct
{
  sCode   code;
  idxType idx;
} sCodeIdx;

//-----------------------------------------------------------------------------
typedef int (*fGetter)(sCode* code, int cookie);
typedef int (*fSetter)(sCode* code, int cookie);
typedef struct
{
  char*   name;
  fGetter getter;
  fSetter setter;
  int     cookie;
} sReg;

//-----------------------------------------------------------------------------
typedef int (*fSvc)(sCode* stack);
typedef struct
{
  char* name;
  fSvc  func;
  int   argc;
} sSvc;


//-----------------------------------------------------------------------------
typedef struct
{
  char  (*getNextChar)(void);
  int   (*addCode)(const sCode* code);
  int   (*setCode)(const sCodeIdx* code);
  int   (*getCode)(sCodeIdx* code, int idx);
  int   (*setString)(const char* str, unsigned int len);
  int   (*getString)(const char** str, int start, unsigned int len);
  sReg  regs[MAX_REG_NUM];
  sSvc  svcs[MAX_SVC_NUM];
} sSys;