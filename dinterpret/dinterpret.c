#include "common.h"
#include "dcpu.h"

int logLevel;

void SysWrite(Dcpu* me, void* data)
{
	uint16_t* msg = Dcpu_GetRam(me) + Dcpu_Pop(me);
	while(*msg) fputc(*msg++, stdout);
}

void SysRead(Dcpu* me, void* data)
{
	char lb[512];
	char* lbuffer = lb;

	uint16_t* buffer = Dcpu_GetRam(me) + Dcpu_Pop(me);
	fgets(lbuffer,sizeof(lb),stdin);

	while(*lbuffer) *buffer++ = *lbuffer++;
}

int main(int argc, char** argv)
{
	logLevel = 2;
	
	LAssert(argc == 2, "usage: %s [dcpu-16 binary]", argv[0]);

	Dcpu* cpu = Dcpu_Create();
	uint16_t* ram = Dcpu_GetRam(cpu);
	
	Dcpu_SetSysCall(cpu, SysRead, 1, NULL);
	Dcpu_SetSysCall(cpu, SysWrite, 2, NULL);

	LoadRam(ram, argv[1]);

	LogV("Ram before execution:");
	if(logLevel <= 1) DumpRam(ram, GetUsedRam(ram));

	while(Dcpu_Execute(cpu, 10)){}
	int returnValue = Dcpu_GetRegister(cpu, DR_A);
	
	LogV("Ram after execution:");
	if(logLevel <= 1) DumpRam(ram, GetUsedRam(ram));

	Dcpu_Destroy(&cpu);
	return returnValue;
}
