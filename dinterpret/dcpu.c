#include "common.h"
#include "dcpu.h"

typedef void (*InsPtr)(Dcpu* me, uint16_t* v1, uint16_t* v2);
typedef void (*SysCallPtr)(Dcpu* me, void* data);

typedef struct {
	SysCallPtr fun;
	int id;
	void* data;
} SysCall;

static const char* dinsNames[] = DINSNAMES;

typedef Vector(SysCall) SysCallVector;

struct Dcpu {
	uint16_t* ram;
	uint16_t regs[8];
	uint16_t sp, pc, o;

	bool performNextIns;
	bool exit;

	SysCallVector sysCalls;

	InsPtr ins[DINS_NUM];
};

uint16_t Dcpu_Pop(Dcpu* me) { 
	return me->ram[me->sp++]; 
}

void Dcpu_Push(Dcpu* me, uint16_t v){
	me->ram[--me->sp] = v;
}

// Basic instructions
void NonBasic(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	if(*v1 == DI_ExtJsr - DINS_EXT_BASE){
		Dcpu_Push(me, me->pc);
		me->pc = *v2;
	}

	else if(*v1 == DI_ExtSys - DINS_EXT_BASE){
		if(*v2 == 0){
			me->exit = true;
			return;
		}

		SysCall* s;
		Vector_ForEach(me->sysCalls, s){
			bool found = false;
			if(s->id == *v2){
				Dcpu_DumpState(me);
				s->fun(me, s->data);
				Dcpu_DumpState(me);
				found = true;
				break;
			}
			if(!found) LogW("Invalid syscall: %d", *v2);
		}
	}
}

void Set(Dcpu* me, uint16_t* v1, uint16_t* v2){ *v1 = *v2; }

void Add(Dcpu* me, uint16_t* v1, uint16_t* v2){
	uint16_t tmp = *v1;
	*v1 += *v2;
	me->o = *v1 < tmp;
}

void Sub(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	uint16_t tmp = *v1;
	*v1 -= *v2;
	me->o = *v1 > tmp;
}

void Mul(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	me->o = ((uint32_t)*v1 * (uint32_t)*v2 >> 16) & 0xffff;
	*v1 *= *v2;
}

void Div(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	if(!*v2) me->o = *v1 = 0;
	me->o = (((uint32_t)*v1 << 16) / ((uint32_t)*v2)) & 0xffff;
	*v1 /= *v2;
}

void Mod(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	if(!*v2) me->o = *v1 = 0;
	*v1 %= *v2;
}

void Shl(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	me->o = (((uint32_t)*v1 << (uint32_t)*v2) >> 16) & 0xffff;
	*v1 = *v1 << *v2;
}

void Shr(Dcpu* me, uint16_t* v1, uint16_t* v2)
{
	me->o = (((uint32_t)*v1 << 16)>> (uint32_t)*v2) & 0xffff;
}

void And(Dcpu* me, uint16_t* v1, uint16_t* v2){ *v1 &= *v2; }
void Bor(Dcpu* me, uint16_t* v1, uint16_t* v2){ *v1 |= *v2; }
void Xor(Dcpu* me, uint16_t* v1, uint16_t* v2){ *v1 ^= *v2; }

void Ife(Dcpu* me, uint16_t* v1, uint16_t* v2){ me->performNextIns = *v1 == *v2; }
void Ifn(Dcpu* me, uint16_t* v1, uint16_t* v2){ me->performNextIns = *v1 != *v2; }
void Ifg(Dcpu* me, uint16_t* v1, uint16_t* v2){ me->performNextIns = *v1 > *v2; }
void Ifb(Dcpu* me, uint16_t* v1, uint16_t* v2){ me->performNextIns = (*v1 & *v2) != 0; }

Dcpu* Dcpu_Create()
{
	Dcpu* me = calloc(1, sizeof(Dcpu));
	me->ram = calloc(1, sizeof(uint16_t) * 0x10000);

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

void Dcpu_DumpState(Dcpu* me)
{
	LogD(" == Current State ==");
	LogD("Registers:");
	LogD("  SP: 0x%04x", me->sp);
	
	LogD(" ");
	LogD("Stack: ");

	if(me->sp){
		for(int i = 0xffff; i >= me->sp; i--){
			LogD("  0x%04x", me->ram[i]);
		}
	}else LogD("  (empty)");
	LogD(" ");
}

int Dcpu_Execute(Dcpu* me, int cycles)
{
	#define READ (me->ram[me->pc++])

	for(int i = 0; i < cycles; i++){
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
			
			// Extended instruction, first op is the instruction number
			if(i == 0 && ins == DI_NonBasic){
				val[0] = v[0];
				pv[i] = val;
				continue;
			}

			if((hasNextWord[i] = opHasNextWord(v[i]))) val[i] = READ;

			// register
			if(vv >= DV_A && vv <= DV_J) pv[i] = me->regs + vv - DV_A;

			// register reference
			else if(vv >= DV_RefBase && vv <= DV_RefTop){
				pv[i] = me->ram + me->regs[vv - DV_RefBase];
			}

			// nextword + register reference
			else if(vv >= DV_RefRegNextWordBase && vv <= DV_RefRegNextWordTop){
				pv[i] = me->ram + val[i] + me->regs[vv - DV_RefRegNextWordBase];
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
			LogD("%s", dinsNames[ins]);
			me->ins[ins](me, pv[0], pv[1]);
		}
		else me->performNextIns = true;

		//Dcpu_DumpState(me);

		if(me->exit) return 0;
	}

	return 1;
}
