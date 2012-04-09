================================
DCPU-16 Binary Development Tools
================================

About
=====

This project aims to provide a complete implementaiton of the DCPU-16 VM, which will be the programmable target for Mojang's new space based game 0x10c (0x10c.com).

The project aims to implement
  * An assembler
  * A disassembler
  * A byte code interpreter

Dasm
====

Dasm supports what you would expect of an assembler. Apart from the normal instructions (SET, ADD, SUB, MUL, DIV, MOD, SHL, SHR, AND, BOR, XOR, IFE, IFN, IFG, IFB, JSR) it also supports my suggested extended system call instruction SYS. For information on the instruction set see the specification at http://0x10c.com/doc/dcpu-16.txt .

SYS
***

To work with sys you have to register a system call callback in the interpreter. My interpreter dinterpret handles this with the Dcpu_SetSysCall function, to which you associate a callback function with a syscall ID. When the interpreter sees a SYS instruction it simply calls the function specified with that ID. It is up to the DCPU-16 application and the system call implementation to implement the calling convention (eg. you could pass arguments on the stack, in registers, or on specific memory locations). See dinterpret/dinterpret.c and dinterpret/tests/syshello.dasm for a puts/gets example.

  * SYS n - where n is a syscall id between 0 and 0xffff

Assembler Directives
********************

There are a number of assembler directives in dasm that aims to make the life of the programmer a bit easier. They include:

  * .ORG n - Set the origin of the assemnly. n is an address. Eg. .ORG 0x10 means that the succeeding instructions now start at memory address 0x10.
  * .DEFINE x y - If a token is x, it's replaced by y. Eg. .DEFINE MY_REGISTER A ; SET MY_REGISTER, 3 will set A to 3.
  * .RESERVE n - Reserves a memory area for eg. a text buffer. n is the size of the area.
  * .FILL n x - Same as .RESERVE but fills the area with the word x.
  * .INCBIN "file" LE|BE - Includes a binary file (path is relative to the source file) into the assembly at the current address. LE is Little Endian and BE is Big Endian.
  * .INCLUDE "file" - Includes another assembly file at the current address, path is relative to the source file.
  * .MACRO/.END - TODO
  * .DAT/.DW x, (,x, x, x) - Put specified words on the memory position. Literals and "strings" are allowed.
