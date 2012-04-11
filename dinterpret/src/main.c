#include "common.h"
#include "dcpu.h"
#include "dinterpret.h"

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
	gets(lbuffer);

	while(*lbuffer) *buffer++ = *lbuffer++;
}

int Start(const char* file, const char* debugFile)
{
	Dcpu* cpu = Dcpu_Create();
	uint16_t* ram = Dcpu_GetRam(cpu);
	
	Dcpu_SetSysCall(cpu, SysRead, 1, NULL);
	Dcpu_SetSysCall(cpu, SysWrite, 2, NULL);

	LoadRam(ram, file);

	Debug* debugger = NULL;

	if(debugFile){
		debugger = Debug_Create(cpu);
		Debug_LoadSymbols(debugger, debugFile);
	}
	
	LogV("Ram before execution:");
	if(logLevel <= 1) DumpRam(ram, GetUsedRam(ram));

	int stillRunning = 0;
	while((stillRunning = Dcpu_Execute(cpu, 10)) || debugger){
		if(debugger && !stillRunning){
			LogI("program finished");
			debugger->runInstructions = 0;
			Debug_Inspector(cpu, debugger);
		}
	}

	int returnValue = Dcpu_GetRegister(cpu, DR_A);
	
	LogV("Ram after execution:");
	if(logLevel <= 1) DumpRam(ram, GetUsedRam(ram));

	Dcpu_Destroy(&cpu);
	if(debugger) Debug_Destroy(&debugger);
	return returnValue;
	
}

int main(int argc, char** argv)
{
	const char* usage = "usage: %s (-vX | -d) [dcpu-16 binary]";
	LAssert(argc >= 2, usage, argv[0]);
	bool debugging = false;

	logLevel = 2;
	
	const char* file = NULL;
	int numFiles = 0;

	for(int i = 1; i < argc; i++){
		char* v = argv[i];
		if(v[0] == '-'){
			if(!strcmp(v, "-h")){
				LogI(usage, argv[0]);
				LogI(" ");
				LogI("Available flags:");
				LogI("  -vX   set log level, where X is [0-5] - default: 2");
				LogI("  -d    start with debugger");
				return 0;
			}
			else if(sscanf(v, "-v%d", &logLevel) == 1){}
			else if(!strcmp(v, "-d")){ debugging = true; }
			else{
				LogF("No such flag: %s", v);
				return 1;
			}
		}else{
			numFiles++;
			file = v;
		}
	}
	
	LAssert(numFiles == 1, "Please specify one file to run");

	const char* debugFile = NULL;
	if(debugging){	
		char tmp[512] = {0};
		strcat(tmp, file);
		strcat(tmp, ".dbg");
		debugFile = tmp;
	}

	return Start(file, debugFile);
}
