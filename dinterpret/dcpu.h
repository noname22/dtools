#ifndef DCPU_H
#define DCPU_H

typedef struct Dcpu Dcpu;

Dcpu* Dcpu_Create();
void Dcpu_Destroy(Dcpu** me);
void* Dcpu_GetRam(Dcpu* me);
int Dcpu_Execute(Dcpu* me, int cycles);

#endif
