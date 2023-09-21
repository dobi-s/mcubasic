#pragma once

#include <stdint.h>
#include "config.h"

//=============================================================================
// Defines
//=============================================================================
#define NEW_CODE        -1
#define NEW_DATA        -2
#define NEXT_CODE_IDX   -3
#define NEXT_DATA_IDX   -4

//=============================================================================
// Typedefs
//=============================================================================
typedef enum
{                   // Code    Linker    Data
  CMD_INVALID = 0,  //  X
  CMD_PRINT,        //  X
  CMD_LET,          //  X
  CMD_SET,          //  X
  CMD_IF,           //  X
  CMD_GOTO,         //  X
  CMD_GOSUB,        //  X
  CMD_RETURN,       //  X
  CMD_NOP,          //  X
  CMD_END,          //  X
  LNK_GOTO,         //          X
  LNK_GOSUB,        //          X
  OP_NEQ,           //                    X
  OP_LTEQ,          //                    X
  OP_GTEQ,          //                    X
  OP_LT,            //                    X
  OP_GT,            //                    X
  OP_EQUAL,         //                    X
  OP_OR,            //                    X
  OP_AND,           //                    X
  OP_NOT,           //                    X
  OP_PLUS,          //                    X
  OP_MINUS,         //                    X
  OP_MOD,           //                    X
  OP_MULT,          //                    X
  OP_DIV,           //                    X
  OP_IDIV,          //                    X
  OP_POW,           //                    X
  OP_SIGN,          //                    X
  VAL_STRING,       //                    X
  VAL_INTEGER,      //                    X
  VAL_FLOAT,        //                    X
  VAL_VAR,          //                    X
  VAL_REG,          //                    X

  CODE_START = CMD_PRINT,
  CODE_END   = LNK_GOSUB,
  DATA_START = OP_NEQ,
  DATA_END   = VAL_REG,
} eOp;

//-----------------------------------------------------------------------------
typedef int32_t   iType;      // Integer value
typedef float     fType;      // Float value
typedef int16_t   idxType;    // Code/data/var/reg/str index
typedef int16_t   sLenType;   // String length

//-----------------------------------------------------------------------------
typedef struct sCode
{
  eOp           op;
  union
  {
    iType       iValue;
    fType       fValue;
    idxType     param;    // var (VAR), reg (REG)
    struct
    {
      idxType   lhs;
      idxType   rhs;
    } expr;
    struct
    {
      idxType   start;
      sLenType  len;
    } str;
    struct
    {
      idxType   expr;     // LET, SET, PRINT: expr; IF: condition
      idxType   param;    // var (LET), reg (SET), false branch (IF), label (GOTO,GOSUB)
    } cmd;
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
typedef struct
{
  char (*getNextChar)(void);
  int  (*setCode)(const sCodeIdx* code);
  int  (*getCode)(sCodeIdx* code, int idx);
  int  (*setString)(const char* str, unsigned int len);
  int  (*getString)(char* str, int start, unsigned int len);
  sReg regs[MAX_REG_NUM];
} sSys;
