# mcuBASIC

mcuBASIC is a small BASIC compiler and virtual machine, designed to run on microcontrollers. It is written in a way that allows to run it in parallel to the MCU's main task loop without the need for a multitasking OS like FreeRTOS.

More information on
* [Syntax](doc/syntax.md)
  * [Memory](doc/syntax.md#memory)
    * [Variable](doc/syntax.md#variable)
    * [Array](doc/syntax.md#arrays)
    * [Register](doc/syntax.md#registers)
  * [Statements](doc/syntax.md#statements)
    * [`Dim`](doc/syntax.md#dim)
    * [`End`](doc/syntax.md#end)
    * [`GoTo`](doc/syntax.md#goto)
    * [`Let`](doc/syntax.md#let)
    * [`Option`](doc/syntax.md#option)
    * [`Print`](doc/syntax.md#print)
    * [`Rem`](doc/syntax.md#remarks)
  * [Control structures](doc/syntax.md#control-structures)
    * [`If` (single-line)](doc/syntax.md#if-single-line)
    * [`If` (multi-line)](doc/syntax.md#if-multi-line)
    * [`Do`..`Loop`](doc/syntax.md#do--loop)
    * [`For`..`Next`](doc/syntax.md#for--next)
  * [Sub functions](doc/syntax.md#sub-functions)
  * [Operators](doc/syntax.md#operators)
    * [Operator precedance](doc/syntax.md#operator-precedance)
* [Integration](doc/integration.md)
  * [main.c](doc/integration.md#mainc)
    * [Setup](doc/integration.md#setup)
  * [basic.c](doc/integration.md#basicc)
    * [Registers](doc/integration.md#registers)
    * [User defined functions](doc/integration.md#user-defined-functions)
    * [System struct](doc/integration.md#system-struct)
* [Technical details](doc/tech_details.md)
  * TBD
