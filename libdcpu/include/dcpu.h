#ifndef DCPU_H
#define DCPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Dcpu Dcpu;

Dcpu* Dcpu_Create();
void Dcpu_Destroy(Dcpu** me);
uint16_t* Dcpu_GetRam(Dcpu* me);
int Dcpu_Execute(Dcpu* me, int cycles);

void Dcpu_SetSysCall(Dcpu* me, void (*sc)(Dcpu* me, void* data), int id, void* data);

uint16_t Dcpu_Pop(Dcpu* me);
void Dcpu_Push(Dcpu* me, uint16_t v);
void Dcpu_DumpState(Dcpu* me);

typedef enum { DR_A, DR_B, DR_C, DR_X, DR_Y, DR_Z, DR_I, DR_J, DR_SP, DR_PC, DR_O } Dcpu_Register;

uint16_t Dcpu_GetRegister(Dcpu* me, Dcpu_Register reg);
void Dcpu_SetRegister(Dcpu* me, Dcpu_Register reg, uint16_t val);

void Dcpu_SetExit(Dcpu* me, bool e);
bool Dcpu_GetExit(Dcpu* me);

/* Something to be called before executing each instruction */
void Dcpu_SetInspector(Dcpu* me, void (*ins)(Dcpu* dcpu, void* data), void* data);

#endif
