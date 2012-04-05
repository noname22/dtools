#ifndef DCPU_H
#define DCPU_H

typedef struct Dcpu Dcpu;

Dcpu* Dcpu_Create();
void Dcpu_Destroy(Dcpu** me);
uint16_t* Dcpu_GetRam(Dcpu* me);
int Dcpu_Execute(Dcpu* me, int cycles);

void Dcpu_SetSysCall(Dcpu* me, void (*sc)(Dcpu* me, void* data), int id, void* data);

uint16_t Dcpu_Pop(Dcpu* me);
void Dcpu_Push(Dcpu* me, uint16_t v);
void Dcpu_DumpState(Dcpu* me);

#endif
