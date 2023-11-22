#pragma once

#include "basic_config.h"
#include <stdint.h>

//=============================================================================
// Typedefs
//=============================================================================
// clang-format off
typedef enum
{                   // Code    Linker    Stack    Arg                 Stack
  CMD_INVALID = 0,  //  X                         -                   -
  CMD_PRINT,        //  X                         <cnt>               -1-<cnt>
  CMD_LET_GLOBAL,   //  X                         <abs>, <dim>        -1/-2
  CMD_LET_LOCAL,    //  X                         <rel>, <dim>        -1/-2
  CMD_LET_PTR,      //  X                         <rel>               -2
  CMD_LET_REG,      //  X                         <reg>               -1
  CMD_IF,           //  X                         <lbl>               -1
  CMD_GOTO,         //  X                         <lbl>               -
  LNK_GOTO,         //          X                 <lbl>               -
  CMD_GOSUB,        //  X                         <lbl>               -
  LNK_GOSUB,        //          X                 <lbl>               -
  CMD_RETURN,       //  X                         <cnt>               -1-<cnt>
  CMD_POP,          //  X                         <cnt>               -1-<cnt>
  CMD_NOP,          //  X                         -                   -
  CMD_END,          //  X                         -                   -
  CMD_SVC,          //  X                         <func>              -<func>.argc
  CMD_GET_GLOBAL,   //  X                         <abs>, <dim>        +1/-
  CMD_GET_LOCAL,    //  X                         <rel>, <dim>        +1/-
  CMD_GET_PTR,      //  X                         <rel>               -
  CMD_GET_REG,      //  X                         <reg>               +1
  CMD_CREATE_PTR,   //  X                         <rel>, <dim>        +1        Becomes VAL_PTR on stack
  OP_NEQ,           //  X                         -                   -2+1
  OP_LTEQ,          //  X                         -                   -2+1
  OP_GTEQ,          //  X                         -                   -2+1
  OP_LT,            //  X                         -                   -2+1
  OP_GT,            //  X                         -                   -2+1
  OP_EQUAL,         //  X                         -                   -2+1
  OP_XOR,           //  X                         -                   -2+1
  OP_OR,            //  X                         -                   -2+1
  OP_AND,           //  X                         -                   -2+1
  OP_NOT,           //  X                         -                   -1+1
  OP_SHL,           //  X                         -                   -2+1
  OP_SHR,           //  X                         -                   -2+1
  OP_PLUS,          //  X                         -                   -2+1
  OP_MINUS,         //  X                         -                   -2+1
  OP_MOD,           //  X                         -                   -2+1
  OP_MULT,          //  X                         -                   -2+1
  OP_DIV,           //  X                         -                   -2+1
  OP_IDIV,          //  X                         -                   -2+1
  OP_POW,           //  X                         -                   -2+1
  OP_SIGN,          //  X                         -                   -1+1
  VAL_ZERO,         //  X                         -                   +1
  VAL_INTEGER,      //  X                X        <int>               +1
  VAL_FLOAT,        //  X                X        <float>             +1
  VAL_STRING,       //  X                X        <str>               +1
  VAL_PTR,          //  X                X        <abs>, <dim>        +1
  VAL_LABEL,        //                   X        <lbl>
} eOp;
// clang-format on

//-----------------------------------------------------------------------------
typedef int32_t iType;     // Integer value
typedef float   fType;     // Float value
typedef int16_t idxType;   // Code/var/reg/str index
typedef int16_t sLenType;  // String length

//-----------------------------------------------------------------------------
typedef struct sCode
{
  eOp op;
  union
  {
    iType iValue;      // VAL_INTEGER
    fType fValue;      // VAL_FLOAT
    struct             //
    {                  //
      idxType param;   // var (VAR, LET), reg (REG, SET), lbl (IF, GOTO, GOSUB)
      idxType param2;  // dim (VAR, LET)
    };                 //
    struct             // VAL_STRING
    {                  //
      idxType  start;  // - start address
      sLenType len;    // - length
    } str;             //
    struct             // VAL_LABEL
    {                  //
      idxType lbl;     // - return address
      idxType fp;      // - frame pointer
    } lbl;             //
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
  const char* name;
  fGetter     getter;
  fSetter     setter;
  int         cookie;
} sReg;

//-----------------------------------------------------------------------------
typedef int (*fSvc)(sCode* args, sCode* mem);
typedef struct
{
  const char* name;
  fSvc        func;
  int         argc;
} sSvc;

//-----------------------------------------------------------------------------
typedef struct
{
  char (*getNextChar)(void);
  int (*newCode)(sCodeIdx* code, eOp op);
  int (*addCode)(const sCode* code);
  int (*setCode)(const sCodeIdx* code);
  int (*getCode)(sCodeIdx* code, int idx);
  int (*getCodeNextIndex)(void);
  int (*getCodeLen)(eOp op);
  int (*setString)(const char* str, unsigned int len);
  int (*getString)(const char** str, int start, unsigned int len);
  sReg regs[MAX_REG_NUM];
  sSvc svcs[MAX_SVC_NUM];
} sSys;
