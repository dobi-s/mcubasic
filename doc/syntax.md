# Syntax

The syntax is based on QBasic or Visual Basic, with modifications and limitations to keep the parser small.

BASIC is case insensitive, so all statements, variables, labels, ... can be written in upper case, lower case or mixed case.

# Memory

## Variable
Variable names can consist of letters (`A-Z`), digits (`0-9`) and underscores (`_`). Digits are not allowed for the first character. The maximum length of variables is implementation dependend (default: 10 characters).

Variables have an automatic datatype and can be either INTEGER (32bit signed) or FLOAT (32bit). Variables are initialized as INTEGER 0, but can become FLOAT on assignments.

Variables are scoped. Variables declared on top level are global, whereas variables declared within sub functions or loop are local and live from their declaration until the end of the block they are declared in.
Variables can be declared by `Dim` or by their first assignment. A variable can't be declared twice (assigment before `Dim` - "before" in terms of code line, not in execution time), however 2 variables can have the same name: the more local variable will shadow the other one.

**Example**
```basic
Dim var1            ' var1 is a global variable

Sub Func(a, b)      ' func (= return value), a and b are local variables
  x = 1             ' x is a local variable
  For i = 0 To 10   ' i is a local variable
    Dim y           ' y is a local variable (created every round)
    ...
  Next              ' y is destroyed (after every round)
                    ' i is destroyed (after the loop)
End Sub             ' func, a, b and x are destroyed

Dim var2            ' var2 is a global variable (but not visible in Func!)
...
```

## Arrays
Arrays are a group of variables. Each individual variable is accessed by the array name and an index.
For arrays apply the mainly the same rules as for variables, with the following exceptions:
* Arrays must be explicitely declared (see `Dim`).
* Arrays are assigned to sub functions by reference, whereas variables are assigned by value.

**Example**
```basic
Sub Test(x(), y)
  x(0) = 10
  y    = 20
End Sub

Dim a(3)
a(0) = 1
b    = 2
Test a, b
Print a(0); ", "; b   ' Output: "10, 2"
```

## Registers
Registers can be used like variables, but their read value or the effect of writing to them depend on the underlying system. Be aware some of them can be read only or write only.
In order to differentiate them from variables, registers always start with a dollar sign (`$`).

**Example**
All registers here are implementation dependend (implemented in the `sys` struct).

```basic
' Blinky
Do
  start = $tick                   '$tick is the systick in milliseconds
  Do
  Loop While $tick - start < 500
  $led = Not $led                 '$led is a GPIO (read and write)
Loop
```

# Statements

## Dim
The `Dim` statement declares a variable or array.

`Dim <var> [= <expr>]`

`Dim <array>(<dimension>)`

| Expression | Description |
| --- | --- |
| < var > | Variable name |
| < array > | Array name |
| < dim > | Dimension of array |
| < expr > | Expression |

Arrays must be declared with `Dim`. Variables can be automatically declared on their first assignment (see `Option Explicit`). However, declaring variables can be useful for explicit scoping or shadowing.

When `Option Explicit` is set, variables must be declared with `Dim`. This way spelling errors in variable names can be detected.

When no expression is assigned as an initial value, the variable is initialized with 0. Arrays are always initialized with all elements 0.

**Main differences to QBasic / Visual Basic**
* No datatype for variables
* No variable lists (only one variable per `Dim` command is allowed)
* Lower bound of array dimensions can't be set (always 0)

**Example**
```basic
Dim abc(8)    ' Array of 8 variables: abc(0) .. abc(7)
Dim xyz       ' Variable (initialized with 0)
Dim a1 = 1+2  ' Variable with initial value
```


## End
Ends the program

`End`

The program ends at the last line. Optionally it can be terminated by the statement `End`.

**Main differences to QBasic / Visual Basic**
* (none)

**Example**
```basic
Print "Hello"
End             ' Program ends here
Print "World"   ' Will never print "World"
```

## GoTo
Jumps to a label. `GoTo` is propably the most iconic BASIC command. However, most of the time it should be replaced by `For`..`Next` or `Do`..`Loop` for better readability, variable scoping, ...

`GoTo <label>`

| Expression | Description |
| --- | --- |
| < label > | Label |

> Note:
> * Never jump across scopes (`Sub`..`End Sub`, `Do`..`Loop`, `For`..`Next`) or variable/array declarations. This is not detected during parsing and will definitely corrupt the stack.

### Labels
Labels are used as a jump target for `GoTo`. Labels can consist of letters (`A-Z`), digits (`0-9`), underscores (`_`) and dollor (`$`). Digits are not allowed for the first character, whereas dollar is only allowed for the first character (but not encouraged). The maximum length of variables is implementation dependend (default: 10 characters). Labels are followed by a colon (`:`).

The label can be a seperate line or it can share a line with a statement

**Main differences to QBasic / Visual Basic**
* Even harder branch restrictions

**Example**
```basic
start:            ' Label without statement
a = 1
GoTo skip
a = 2
skip: Print a     ' Label with statement; Output: "1"
```

## Let
Assigns a value to a variable, array or register.

`[Let] <var> = <expr>`

| Expression | Description |
| --- | --- |
| < var > | Name of variable, register or array (with index) |
| < expr > | Expression |

`Let` is a historic remnant and therefore optional.

**Main differences to QBasic**
(`Let` is not part of Visual Basic)
* (none)

**Example**
```basic
Let a = 1 + 2   ' Assignment with Let
a = a + 1       ' Assignment without Let
Print a         ' Output: "4"
```

## Option
Options will not change the behaviour the program, they set options for the compiler.

Options can be changed during source code, so it is possible to turn on an option for parts of the code only.

### Option Explicit
Defines, whether variables must be declared with `Dim` (`Option Explicit On`) or variables are also created at assignments (`Option Explicit Off`). The only exceptions are loop variables of a `For`..`Next` loop, which can always be created in the header of a `For`..`Next` loop.

`Option Explicit [{On|Off}]`

By default, option explicit is off. If `Option Explicit` is used without `On` or `Off`, it is turned on.
It is good practice to turn on `Option Explicit` in order to detect misspelled variable names.

**Main differences to Visual Basic**
(`Option Explicit` is not part of QBasic)
* (none)

**Example**
```basic
a = 1             ' Variable "a" is created by assignment

Option Explicit   ' Below here, variables must be declared
Dim var1
var_1 = a + 1     ' Error, "var_1" can't be created by assignment
                  ' If "var1" should be "var_1", the error wouldn't
                  ' be detected without "Option Explicit"
```


## Print
Output data on `stdout`

`Print <expr>[; [<expr> [; [<expr> ...]]]]`

| Expression | Description |
| --- | --- |
| < expr > | Expression (numeric or string) |

After each `Print` command, an end-of-line (defined by `BASIC_OUT_EOL` in `basic_config.h`) is sent to `stdout`. A trailing semicolon (`;`) will suppress a the end-of-line character(s), so the next `Print` will continue the line.
> Notes:
> * Depending on your system settings, the output could be buffered until a newline-character is sent.
> * Splitting lines to several `Print` commands will be a bit slower but will consume less stack

**Main differences to QBasic**
(`Print` is not part of Visual Basic)
* No tab support (`,`)

**Example**
```basic
a = 5
Print "a = "; a     ' Output: "a = 5"
```

## Remarks
There are 2 ways of comments: the statement `Rem` or the single quote (`'`).

`Rem [<comment>]`

`' <comment>`

| Expression | Description |
| --- | --- |
| < comment > | Any text until end-of-line |

`Rem` is normal statement, so the same rules as for all other statements apply. The single quote comment can follow any code and is considered as end-of-line.

```basic
Rem A comment with Rem
Print 1+2   ' Comment in same line as another statement
Rem         ' Comment in comment ;-)
```

# Control structures

## If (single-line)
Executes statement depending on condition

`If <cond> Then <statement>`

| Expression | Description |
| --- | --- |
| < cond > | Condition |
| < statement > | Any (single line) statement |

The condition can by any numeric expression. It is considered False when it evaluates to 0, otherwise it is considered True.

**Main differences to QBasic / Visual Basic**
* No `Else` supported in single-line `If`
* Only 1 single statement (as no `:` statement separator is implemented)

**Example**
```basic
a = 1
If a < 0 Then Print "a is negative"
```

## If (multi-line)
Executes statement depending on condition

```
If <cond> Then
  <statements>
[Else
  <statements>]
End If
```

| Expression | Description |
| --- | --- |
| < cond > | Condition |
| < statements > | Any number of statements |

The condition can by any numeric expression. It is considered false when it evaluates to 0, otherwise it is considered true.

**Main differences to QBasic / Visual Basic**
* No `ElseIf` supported (use nested `If`)

**Example**
```basic
a = 1
If a < 0 Then
  Print "a is negative"
Else
  If a = 0 Then
    Print "a is zero"
  Else
    Print "a is positive"
  End If
End If
```

## Do .. Loop
Repeates a block of statements

```
Do [{While|Until} <cond>]
  <statements>
Loop [{While|Until} <cond>]
```

| Expression | Description |
| --- | --- |
| < cond > | Condition |
| < statements > | Any number of statements |

The statements are repeated, until / while the condition is true. If no condition is given, the loop is repeated forever ("infinite loop").
The loop can additionally be exited with the statement `Exit Do`.
If the condition is provided at `Do`, it is checked __before__ each loop. If the condition is provided at `Loop`, it is checked __after__ each loop (so the statements are executed at least once).

**Main differences to QBasic / Visual Basic**
* Conditions are allowed at both `Do` and `Loop`

**Example**
```basic
a = 1
Do
  Print a
  If a > 5 Then Exit Do
Loop Until a >= 10
```
Loop would run until a = 10 (`Until a >= 10`), but is exited prematurely at a = 6 (`If a > 5 Then Exit Do`).

## For .. Next
Repeates a block of statements a defined number of times

```
For <var> = <init> To <end> [Step <step>]
  <statements>
Next
```

| Expression | Description |
| --- | --- |
| < var > | Variable or register |
| < init > | Initial value of variable |
| < end > | Last value of variable |
| < step > | Step size (default: 1) |
| < statements > | Any number of statements |

The variable < var > is initialized with < init >. While the variable is <= < end > the statements are executed and the variable is increased by < step >.
The loop can be exited with the statement `Exit For`.

A `For` loop can be replaced by the following `Do` loop:
```
<var> = <init>
Do While <var> <= <end>
  <statements>
  <var> = <var> + <step>
Loop
```

**Main differences to QBasic / Visual Basic**
* < end > and < step > are recalculated at each use.
* A negative step doesn't invert the condition (`For x = 10 To 0 Step -1` will never execute the statements as 10 > 0).
* `Next` is always used without a variable name.
* Multiple `Next` can't be joined (as in `Next i, j`; QBasic only).

**Example**
```basic
For i = 0 To 10 Step 2
  Print i                 ' Output: 0 2 4 6 8 10
Next
```

# Sub functions
A sub function is a block of code, which can be called on several places and which can optionally return a value.

```
Sub <name>([<arg> [, <arg> [, ...]]])
  <statements>
End Sub
```

| Expression | Description |
| --- | --- |
| < name > | Function name |
| < arg > | Arguments |
| < statements > | Any number of statements |

The statements inside a function can access global variables and the arguments (as local variables). The function name is also available as local variable and serves as the return value of the function.
A function can be exited by `Exit Sub` or with the `Return` statement.

If a function is called inside an expression (i.e. its return value is used), it is called with brackets. If it is called as statement (i.e. its return value is not used), it is called without brackets.

Arguments are passed by value, i.e. changing its value doesn't change the variable used to call the function. An exception is an array argument, which is passed by reference.
Array arguments are noted in the form `<name>()`. The array inherits the dimension of the passed array.

**Main differences to QBasic / Visual Basic**
* No datatype for arguments and return value
* No differentiation between `Sub` (no return value) and `Function` (with return value)

**Example**
```basic
Sub Add(a, b)
  add = a + b
End Sub

x = Add(1, 2) + 3 ' Use in expression
Add 3, 4          ' Use as statement
```

# Operators
The following operators can be used in expressions

| Operator | Description      | Result                                        |
| :------: | ---------------- | --------------------------------------------- |
| `+`      | Addition         | INTEGER (both operants are INTERGER) or FLOAT |
| `-`      | Subtraction      | INTEGER (both operants are INTERGER) or FLOAT |
| `*`      | Multiplication   | INTEGER (both operants are INTERGER) or FLOAT |
| `/`      | Division         | FLOAT                                         |
| `\`      | Integer division | INTEGER (both operants casted to INTERGER)    |
| `Mod`    | Modulo           | INTEGER (both operants casted to INTERGER)    |
| `^`      | Power            | INTEGER (both operants casted to INTERGER)    |
| `-`      | Sign             | INTEGER or FLOAT (same as operant)            |
| `Shr`    | Shift right      | INTEGER (both operants casted to INTERGER)    |
| `Shl`    | Shift left       | INTEGER (both operants casted to INTERGER)    |
| `<>`     | Not equal        | INTEGER (true: -1; false: 0)                  |
| `=`      | Equal            | INTEGER (true: -1; false: 0)                  |
| `>`      | Greater than     | INTEGER (true: -1; false: 0)                  |
| `<`      | Less than        | INTEGER (true: -1; false: 0)                  |
| `<=`     | Less or equal    | INTEGER (true: -1; false: 0)                  |
| `>=`     | Greater or equal | INTEGER (true: -1; false: 0)                  |
| `Or`     | Binary OR*       | INTEGER                                       |
| `And`    | Binary AND*      | INTEGER                                       |
| `Not`    | Binary NOT*      | INTEGER                                       |

*) True and false values are choosen in a way that binary operators can also be used as logical operators when used in conjunction with comparators:
   * `a > 1 And a < 5` will work as intended
   * `a > 1 And 5`     will work as binary (`-1 And 5` is 5, however 5 is not 0 and therefore considered true)
   * `2 And 5`         will work as binary `And` (like `2 & 5` and not like `2 && 5` in C/C++)

## Operator precedance
| Group       | Operators                  |
| ----------- | :------------------------: |
| Or          | `Or`                       |
| And         | `And`                      |
| Not         | `Not`                      |
| Comparators | `=` `<` `<=` `>` `>=` `<>` |
| Shift       | `Shr` `Shl`                |
| Add/SUb     | `+` `-`                    |
| Mult/Div    | `*` `/` `\` `Mod`          |
| Power       | `^`                        |
| Sign        | `-`                        |
