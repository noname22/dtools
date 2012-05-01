#include "common.h"
#include "dcpu.h"
#include "dinterpret.h"
#include <SDL.h>

int logLevel;

typedef enum {
	MT_None, MT_Notch, MT_Noname
} MachineType;

typedef struct {
	MachineType machine;
	int freq;
	const char* file;
	const char* debugFile;
} Settings;

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
	fgets(lbuffer, sizeof(lb), stdin);

	while(*lbuffer) *buffer++ = *lbuffer++;
}

int Start(Settings* settings)
{
	Dcpu* cpu = Dcpu_Create();
	uint16_t* ram = Dcpu_GetRam(cpu);
	
	Dcpu_SetSysCall(cpu, SysRead, 1, NULL);
	Dcpu_SetSysCall(cpu, SysWrite, 2, NULL);

	LoadRam(ram, settings->file);

	Debug* debugger = NULL;

	if(settings->debugFile){
		debugger = Debug_Create(cpu);
		Debug_LoadSymbols(debugger, settings->debugFile);
	}
	
	LogV("Ram before execution:");
	if(logLevel <= 1) DumpRam(ram, GetUsedRam(ram));

	LogV("sleeping every %d instructions", settings->freq);

	int stillRunning = 0;
	while((stillRunning = Dcpu_Execute(cpu, settings->freq ? settings->freq : 1000)) || debugger){
		if(debugger && !stillRunning){
			LogI("program finished");
			debugger->runInstructions = 0;
			Debug_Inspector(cpu, debugger);
		}

		// Sleep 1 millisecond for every ~freq cycles executed (default 7000)
		if(settings->freq) SDL_Delay(1);
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

	Settings settings;
	memset(&settings, 0, sizeof(Settings));

	settings.freq = 7000;

	bool debugging = false;

	logLevel = 2;
	
	int numFiles = 0;
	float fFreq;

	char machineStr[64] = {0};
	strcat(machineStr, "none");

	for(int i = 1; i < argc; i++){
		char* v = argv[i];
		if(v[0] == '-'){
			if(!strcmp(v, "-h")){
				LogI(usage, argv[0]);
				LogI(" ");
				LogI("Available flags:");
				LogI("  -vX   set log level, where X is [0-5] - default: 2");
				LogI("  -d    start with debugger");
				LogI("  -fF   interpret at frequency F in MHz - default 7.0 MHz, 0.0 = as fast as possible");
				LogI("  -mM   start with machine M - none (default, only cpu), notch (speculative), noname (my own awesome machine)");
				return 0;
			}
			else if(sscanf(v, "-f%f", &fFreq) == 1){ settings.freq = (int)(fFreq * 1000.0f); }
			else if(sscanf(v, "-v%d", &logLevel) == 1){}
			else if(!strcmp(v, "-d")){ debugging = true; }
			else if(sscanf(v, "-m%s", machineStr)){}
			else{
				LogF("No such flag: %s", v);
				return 1;
			}
		}else{
			numFiles++;
			settings.file = v;
		}
	}

	if(!strcmp(machineStr, "noname")){ settings.machine = MT_Noname; }
	else if(!strcmp(machineStr, "notch")){ settings.machine = MT_Notch; }
	else{ settings.machine = MT_None; }
	
	LAssert(numFiles == 1, "Please specify one file to run");

	if(debugging){	
		char tmp[512] = {0};
		strcat(tmp, settings.file);
		strcat(tmp, ".dbg");
		settings.debugFile = tmp;
	}

	return Start(&settings);}
