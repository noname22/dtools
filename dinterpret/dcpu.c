#include "common.h"
#include "dcpu.h"

typedef void (*insp)(Dcpu* me) InsPtr;

struct Dcpu {
	uint16_t* ram;
	uint16_t regs[8];
	uint16_t sp, pc, o;

	void (*ins)(Dcpu* me);

	InsPtr p[256];
};

// Basic instructions
void NonBasic(Dcpu* me)
{
}

void Set(Dcpu* me)
{
}

void Add(Dcpu* me)
{
}

void Sub(Dcpu* me)
{
}

void Mul(Dcpu* me)
{
}

void Div(Dcpu* me)
{
}

void Mod(Dcpu* me)
{
}

void Shl (Dcpu* me)
{
}

void Shr(Dcpu* me)
{
}

void And(Dcpu* me)
{
}

void Bor(Dcpu* me)
{
}

void Xor(Dcpu* me)
{
}

void Ife(Dcpu* me)
{
}

void Ifn(Dcpu* me)
{
}

void Ifg(Dcpu* me)
{
}

void Ifb(Dcpu* me)
{
}


// Extended instructions
//DI_ExtReserved, DI_ExtJsr

Dcpu* Dcpu_Create()
{
	Dcpu* me = calloc(1, sizeof(Dcpu));
	me->ram = calloc(1, sizeof(uint16_t) * 0x10000);

	me->ins[DI_NonBasic] = NonBasic;
	DI_Set
	DI_Set
	DI_Add
	DI_Add
	DI_Sub
	DI_Sub
	DI_Mul
	DI_Mul
	DI_Div
	DI_Div
	DI_Mod
	DI_Mod
	DI_Shl 
	DI_Shl 
	DI_Shr
	DI_Shr
	DI_And
	DI_And
	DI_Bor
	DI_Bor
	DI_Xor
	DI_Xor
	DI_Ife
	DI_Ife
	DI_Ifn
	DI_Ifn
	DI_Ifg
	DI_Ifg
	DI_Ifb
	DI_Ifb

	return me;
}

void Dcpu_Destroy(Dcpu** me)
{
	free((*me)->ram);
	free(*me);
	*me = NULL;
}

void* Dcpu_GetRam(Dcpu* me)
{
	return me->ram;
}

int Dcpu_Execute(Dcpu* me, int cycles)
{
	return 0;
}
