#include "common.h"
#include "dcpu.h"

int logLevel;

int main(int argc, char** argv)
{
	logLevel = 1;
	
	LAssert(argc == 2, "usage: %s [dcpu-16 binary]", argv[0]);

	Dcpu* cpu = Dcpu_Create();
	LoadRam(Dcpu_GetRam(cpu), argv[1]);

	LogV("Ram before execution:");
	if(logLevel == 1) DumpRam(Dcpu_GetRam(cpu));

	while(Dcpu_Execute(cpu, 10)){}
	
	LogV("Ram after execution:");
	if(logLevel == 1) DumpRam(Dcpu_GetRam(cpu));

	Dcpu_Destroy(&cpu);
}
