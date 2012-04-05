#include "common.h"
#include "dcpu.h"

int logLevel;

void SysWrite(Dcpu* me, void* data)
{
	uint16_t* msg = Dcpu_GetRam(me) + Dcpu_Pop(me);
	for(int i = 1; i < msg[0] + 2; i++) fputc(msg[i], stdout);
}

int main(int argc, char** argv)
{
	logLevel = 2;
	
	LAssert(argc == 2, "usage: %s [dcpu-16 binary]", argv[0]);

	Dcpu* cpu = Dcpu_Create();
	
	Dcpu_SetSysCall(cpu, SysWrite, 2, NULL);

	LoadRam(Dcpu_GetRam(cpu), argv[1]);

	LogV("Ram before execution:");
	if(logLevel <= 1) DumpRam(Dcpu_GetRam(cpu));

	while(Dcpu_Execute(cpu, 10)){}
	
	LogV("Ram after execution:");
	if(logLevel <= 1) DumpRam(Dcpu_GetRam(cpu));

	Dcpu_Destroy(&cpu);
}
