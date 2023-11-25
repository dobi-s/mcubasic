# Integration
This document describes how to integragte mcuBASIC into a microcontroller project.

A good starting point are the files in the demo folder.
| File | Description |
| ---- | ----------- |
| main.c | The main task loop, calling the BASIC task and maybe some other tasks written in C |
| basic.c | All the system related functions, which should be implemented depending on the actual system and hardware |
| basic_config.h | Configuration defines for mcuBASIC |

# main.c
Usually the main function will setup the system and has a kind of task loop to call sub tasks in a regular interval.

## Setup
There are several steps needed to setup mcuBASIC.
1. Generate the bytecode

   You can read the BASIC source code from an interface (eg UART) or memory (eg file system) and parse it to bytecode. This way is shown in the demo.
   If you already have a ready to use bytecode in some memory (internal such as flash or EEPROM, or external such as USB memory, µSD card, ...), you can skip this step.
2. Init mcuBASIC

   This is done by calling `BasicInit`. This function (in `basic.c`) can be customized, but usually loads the bytecode and sets the starting point of the execution.
3. Run mcuBASIC

   In your task loop, the function `BasicTask` must be called in regular intervals (eg every 10ms).
   `BasicTask` will run `exec` several times (for 2ms in the demo) to execute the bytecode. It will return `true` until the BASIC program ends or is terminated by an error.

# basic.c
This is the main file for integrating mcuBASIC into your system. Here the system environment for mcuBASIC is implemented, such as
* Registers
* User defined functions
* Access functions for bytecode and strings

## Registers
Registers are used like variables in your BASIC program. However, when accessing them getters or setters are called. This way, the BASIC program can access the hardware in an easy way.

> Note: Registers must always start with a dollar sign (`$`) in order to distinguish them from variables!

### Getter
The getter has the form `int <name>(sCode* val, int cookie)`.
The value of the getter is saved in `val`:
* Integers
  ```C
  val->op     = VAL_INTEGER;
  val->iValue = ...;
  ```
* Floating Point
   ```C
   val->op     = VAL_FLOAT;
   val->fValue = ...;
   ```
The cookie is used to reuse the same getter for different registers. For example, several digital inputs can share the same getter with the cookie serving as pin number.

The return value of the getter function is
* Positive or zero: Success
* Negative: Error code

### Setter
The setter has the form `int <name>(sCode* val, int cookie)`.

It works the same way as the register getter, however the pointer `val` is input instead of output.

## User defined functions
SVCs (Super Visor Calls) on a normal operating system are used to call kernel functions from an user space application. Here the same principle is used: SVCs are used to call functions in the underlying system out of a BASIC application.

This way, a function can be defined which interacts with the hardware (eg. accessing memory). Or it can be used to outsource performance critical tasks from BASIC to C.

### Function
The SVC function in C has the form `int <name>(sCode* args, sCode* mem)`.

`args` points to an array of arguments of the function. `args[0]` is the return value of the function (initialized with 0), `args[1]` is the first argument (if any), and so on.

`mem` is a pointer to the whole stack. It is used with pointer arguments only.

```C
// Example of accessing array arguments (first argument is array)
if (args[1].op != VAL_PTR)            // arg[1] must be pointer to
an array
  return ERR_EXEC_VAR_INV;

int offset = args[1].param;           // Offset of 1st array element in mem
int dim    = args[1].param2;          // Dimension of array

if (dim < 2)                          // Check array bounds
  return ERR_EXEC_OUT_BOUND;
mem[offset+1].op     = VAL_INTERGER;  // Change 2nd array element
mem[offset+1].iValue = 123;           // to 123 (integer)
```

## System struct
The system environment is packed into the system struct of type `sSys` (typedefed in `basic_bytecode.h`).

Most of the times, only the following struct members must be adapted:
* getNextChar
* regs
* svcs

### getNextChar
`char getNextChar(void)` is called during parsing and should provide the next character of the BASIC source code. The end of file is marked by a NUL byte (`\0`).

You should change this function depending on the source of the BASIC source code (UART, memory, ...).

### getCodeLen
`int getCodeLen(eOp op)` returns the length of an instruction in memory. Depending how you save the bytecode, it must be adjusted.

> If this function is changed, the bytecode must be recompiled!

### addCode
`int addCode(const sCode* code)` adds an instruction to the bytecode.

### setCode
`int setCode(const sCodeIdx* code)` saves an instruction (usually created by `newCode`) over an existing instruction (of the same operator).

### newCode
`int newCode(sCodeIdx* code, eOp op)` creates a new instrution of the bytecode. It is used to create an instruction, which is changed later (e.g. `CMD_GOTO` where the destination will be set later).

It basically does the same as `addCode`, but it returns a sCodeIdx object which can be used with `setCode`.

### getCode
`int getCode(sCodeIdx* code, int idx)` reads an instruction from memory at index `idx` and returns it in `code`.

### getCodeNextIndex
`int getCodeNextIndex(void)` returns the index of the next (not yet existing) instruction. It is often used to get the branch destination.

### setString
`int setString(const char* str, unsigned int len)` saves a string in the string memory and returns the offset of the first character.

In the demo implementation, deduplication is used. So if the string is already present in the string memory, this substring is reused to save memory.

### getString
`int getString(const char** str, int start, unsigned int len)` returns a pointer to the string, which is saved at offset `start` in the string memory.

In the demo implementation, it returns a pointer to the string memory, avoiding an unnecessary copy.

### regs
Registers are defined by
* Register name (starting with `$`)
* Getter function (or `NULL`)
* Setter function (or `NULL`)
* Cookie

If only a getter function is defined (setter = `NULL`), the register is read only. If only a setter function is defined (getter = `NULL`), the register is write only.

The cookie is passed to the setter or getter function, so the same function can be used for several registers. An example is a digital output, where the cookie serves as the pin number.

> If registers or their order in the list change, the BASIC program must be recompiled!
> There are only 2 exceptions:
> * New registers were added at the end of the list
> * A register name changed, but it is still the "same" register (in the bytecode only the register index is used, the name is used only during parsing).

### svcs
SVCs are defined by
* Function name
* Function pointer
* Number of arguments

The function pointer is called when the SVC is called in BASIC. The number of arguments is used for checks during parsing and for stack management during execution.

> If SVCs or their order in the list change, the BASIC program must be recompiled!
> There are only 2 exceptions:
> * New SVCs were added at the end of the list
> * An SVC name changed, but it is still the "same" function (in the bytecode only the SVC index is used, the name is used only during parsing).
