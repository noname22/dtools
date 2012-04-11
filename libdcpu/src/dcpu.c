#include "common.h"
#include "dcpu.h"

typedef void (*InsPtr)(Dcpu* me, uint16_t* v1, uint16_t* v2);
typedef void (*SysCallPtr)(Dcpu* me, void* data);

typedef struct {
	SysCallPtr fun;
	int id;
	void* data;
} SysCall;

//static const char* dinsNames[] = DINSNAMES;

typedef Vector(SysCall) SysCallVector;

struct Dcpu {
	uint16_t* ram;
	uint16_t regs[8];
	uint16_t sp, pc, o;

	bool performNextIns;
	bool exit;

	int cycles;

	SysCallVector sysCalls;
	void (*inspector)(Dcpu* dcpu, void* data);
	void* inspectorData;

	InsPtr ins[DINS_NUM];
};

// Cast to uint16_t
#define U16C(__w) ((uint16_t)(__w))

void Dcpu_SetExit(Dcpu* me, bool e)
{
	me->exit = e;
}

bool Dcpu_GetExit(Dcpu* me)
{
	return me->exit;
}

uint16_t Dcpu_Pop(Dcpu* me) { 
	return me->ram[me->sp++];
}

void Dcpu_Push(Dcpu* me, uint16_t v){
	me->ram[--me->sp] = v;
}

void Dcpu_SetInspector(Dcpu* me, void (*ins)(Dcpu* dcpu, void* data), void* data)
{
	me->inspector = ins;
	me->inspectorData = data;
}

// Extended instructions
void NonBasic(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	if(*v1 == DI_ExtJsr - DINS_EXT_BASE){
		Dcpu_Push(me, me->pc);
		me->pc = *v2;
		me->cycles += 2;
	}

	else if(*v1 == DI_ExtSys - DINS_EXT_BASE){
		me->cycles += 1;

		if(*v2 == 0){
			me->exit = true;
			return;
		}

		SysCall* s;
		bool found = false;
		Vector_ForEach(me->sysCalls, s){
			if(s->id == *v2){
				s->fun(me, s->data);
				found = true;
				break;
			}
		}
		if(!found) LogW("Invalid syscall: %d", *v2);
	}

	else
		me->cycles += 1;
}

// Basic instructions
void Set(Dcpu* me, uint16_t* v1, uint16_t* v2){ *v1 = *v2; me->cycles++; }

void Add(Dcpu* me, uint16_t* v1, uint16_t* v2){
	uint16_t tmp = *v1;
	*v1 += *v2;
	me->o = *v1 < tmp;
	me->cycles += 2;
}

void Sub(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	uint16_t tmp = *v1;
	*v1 -= *v2;
	me->o = *v1 > tmp;
	me->cycles += 2;
}

void Mul(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	me->o = ((uint32_t)*v1 * (uint32_t)*v2 >> 16) & 0xffff;
	*v1 *= *v2;
	me->cycles += 2;
}

void Div(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	if(*v2 == 0){ me->o = *v1 = 0; return; }
	me->o = (((uint32_t)*v1 << 16) / ((uint32_t)*v2)) & 0xffff;

	// do this twice because v2 can be o
	if(*v2 == 0){ me->o = *v1 = 0; return; }

	*v1 /= *v2;
	me->cycles += 3;
}

void Mod(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	if(*v2 == 0){ me->o = *v1 = 0; return; }
	*v1 %= *v2;
	me->cycles += 3;
}

void Shl(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	me->o = (((uint32_t)*v1 << (uint32_t)*v2) >> 16) & 0xffff;
	*v1 = *v1 << *v2;
	me->cycles += 2;
}

void Shr(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	me->o = (((uint32_t)*v1 << 16)>> (uint32_t)*v2) & 0xffff;
	me->cycles += 2;
}

void And(Dcpu* me, uint16_t* v1, uint16_t* v2){ *v1 &= *v2; me->cycles++; }
void Bor(Dcpu* me, uint16_t* v1, uint16_t* v2){ *v1 |= *v2; me->cycles++; }
void Xor(Dcpu* me, uint16_t* v1, uint16_t* v2){ *v1 ^= *v2; me->cycles++; }

void Ife(Dcpu* me, uint16_t* v1, uint16_t* v2){ me->performNextIns = *v1 == *v2; me->cycles += 2 + (uint16_t)me->performNextIns; }
void Ifn(Dcpu* me, uint16_t* v1, uint16_t* v2){ me->performNextIns = *v1 != *v2; me->cycles += 2 + (uint16_t)me->performNextIns; }
void Ifg(Dcpu* me, uint16_t* v1, uint16_t* v2){ me->performNextIns = *v1 > *v2; me->cycles += 2 + (uint16_t)me->performNextIns; }
void Ifb(Dcpu* me, uint16_t* v1, uint16_t* v2){ me->performNextIns = (*v1 & *v2) != 0; me->cycles += 2 + (uint16_t)me->performNextIns; }

Dcpu* Dcpu_Create()
{
	Dcpu* me = calloc(1, sizeof(Dcpu));
	me->ram = calloc(1, sizeof(uint16_t) * 0x10000);

	me->performNextIns = true;

	Vector_Init(me->sysCalls, SysCall);

	me->ins[DI_NonBasic] = NonBasic;
	me->ins[DI_Set] = Set;
	me->ins[DI_Add] = Add;
	me->ins[DI_Sub] = Sub;
	me->ins[DI_Mul] = Mul;
	me->ins[DI_Div] = Div;
	me->ins[DI_Mod] = Mod;
	me->ins[DI_Shl] = Shl;
	me->ins[DI_Shr] = Shr;
	me->ins[DI_And] = And;
	me->ins[DI_Bor] = Bor;
	me->ins[DI_Xor] = Xor;
	me->ins[DI_Ife] = Ife;
	me->ins[DI_Ifn] = Ifn;
	me->ins[DI_Ifg] = Ifg;
	me->ins[DI_Ifb] = Ifb;

	return me;
}

void Dcpu_Destroy(Dcpu** me)
{
	Vector_Free((*me)->sysCalls);

	free((*me)->ram);
	free(*me);
	*me = NULL;
}

void Dcpu_SetSysCall(Dcpu* me, void (*sc)(Dcpu* me, void* data), int id, void* data)
{
	SysCall s = {sc, id, data};
	Vector_Add(me->sysCalls, s);
}

uint16_t* Dcpu_GetRam(Dcpu* me)
{
	return me->ram;
}

uint16_t Dcpu_GetRegister(Dcpu* me, Dcpu_Register reg)
{
	if(reg <= DR_J) return me->regs[reg];
	if(reg == DR_O) return me->o;
	if(reg == DR_SP) return me->sp;
	return me->pc;
}

void Dcpu_SetRegister(Dcpu* me, Dcpu_Register reg, uint16_t val)
{
	if(reg <= DR_J) me->regs[reg] = val;
	else if(reg == DR_O) me->o = val;
	else if(reg == DR_SP) me->sp = val;
	else me->pc = val;
}

void Dcpu_DumpState(Dcpu* me)
{
	LogD(" == Current State ==");
	LogD("Regs: A:0x%04x B:0x%04x C:0x%04x X:0x%04x Y:0x%04x Z:0x%4x I:0x%04x J:0x%04x", 
		me->regs[0], me->regs[1], me->regs[2], me->regs[3], me->regs[4], me->regs[5], me->regs[6], me->regs[7]);

	LogD("      SP:0x%04x PC:0x%04x O:0x%04x", me->sp, me->pc, me->o);
	
	LogD(" ");
	LogD("Stack: ");

	if(me->sp){
		for(int i = 0xffff; i >= me->sp; i--){
			LogD("  0x%04x", me->ram[i]);
		}
	}else LogD("  (empty)");
	LogD(" ");
}

int Dcpu_Execute(Dcpu* me, int execCycles)
{
	#define READ me->ram[me->pc++]
	me->cycles = 0;

	while(me->cycles < execCycles){
		if(me->inspector) me->inspector(me, me->inspectorData);

		bool hasNextWord[2];
		uint16_t val[2];
		uint16_t pIns = READ;
		DVals v[2];

		uint16_t* pv[2];

		DIns ins = pIns & 0xf;

		v[0] = (pIns >> 4) & 0x3f;
		v[1] = (pIns >> 10) & 0x3f;

		for(int i = 0; i < 2; i++){
			DVals vv = v[i];
			
			// Extended instruction, first operand is the instruction number
			if(i == 0 && ins == DI_NonBasic){
				val[0] = v[0];
				pv[i] = val;
				continue;
			}

			if((hasNextWord[i] = opHasNextWord(v[i]))) {
				val[i] = READ;
				me->cycles++;
			}

			// register
			if(vv >= DV_A && vv <= DV_J) pv[i] = me->regs + vv - DV_A;

			// register reference
			else if(vv >= DV_RefBase && vv <= DV_RefTop){
				pv[i] = me->ram + me->regs[vv - DV_RefBase];
			}

			// nextword + register reference
			else if(vv >= DV_RefRegNextWordBase && vv <= DV_RefRegNextWordTop){
				pv[i] = me->ram + U16C(val[i] + me->regs[vv - DV_RefRegNextWordBase]);
			}

			else if(vv == DV_Pop)  pv[i] = me->ram + me->sp++;
			else if(vv == DV_Peek) pv[i] = me->ram + me->sp;
			else if(vv == DV_Push) pv[i] = me->ram + --me->sp;

			else if(vv == DV_SP) pv[i] = &me->sp;
			else if(vv == DV_PC) pv[i] = &me->pc;
			else if(vv == DV_O) pv[i] = &me->o; // XXX: O should be one bit?

			else if(vv == DV_RefNextWord) pv[i] = me->ram + val[i];
			else if(vv == DV_NextWord) pv[i] = val + i;
			else if(vv >= DV_LiteralBase && vv <= DV_LiteralTop){
				val[i] = vv - DV_LiteralBase;
				pv[i] = val + i;
			}
		}
		
		if(me->performNextIns){ 
			//LogD("%s", dinsNames[ins]);
			me->ins[ins](me, pv[0], pv[1]);
		}

		else me->performNextIns = true;

		//Dcpu_DumpState(me);

		if(me->exit) return 0;
	}

	return 1;
}
