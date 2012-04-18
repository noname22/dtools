#include "cvector.h"
#include "common.h"
#include "dasm.h"

int logLevel;

int main(int argc, char** argv)
{
	logLevel = 2;

	int atFile = 0;
	unsigned addr = 0;
	unsigned lastAddr = 0xffff;
	bool debugSymbols = false;
	char c;
	DByteOrder byteOrder = DBO_LittleEndian;

	const char* files[2] = {NULL, NULL};
	const char* usage = "usage: %s (-vX | -h | -sX | -d | -eX) [dasm file] [out binary]";

	for(int i = 1; i < argc; i++){
		char* v = argv[i];
		if(STARTSWITH(v, '-')){
			if(!strcmp(v, "-h")){
				LogI(usage, argv[0]);
				LogI(" ");
				LogI("Available flags:");
				LogI("  -vX   set log level, where X is [0-5] - default: 2");
				LogI("  -sX   set assembly start address [0-FFFF] - default 0");
				LogI("  -h    show this help message");
				LogI("  -d    generate debug symbols");
				LogI("  -eX   set endianness of output, where X is [l | b] default: l");
				return 0;
			}
			else if(sscanf(v, "-v%d", &logLevel) == 1){}
			else if(sscanf(v, "-s%x", &addr) == 1){}
			else if(sscanf(v, "-e%1c", &c) == 1){ byteOrder = c == 'l' ? DBO_LittleEndian : DBO_BigEndian; }
			else if(!strcmp(v, "-d")){ debugSymbols = true; }
			else{
				LogF("No such flag: %s", v);
				return 1;
			}
		}else{
			LAssert(atFile < 2, "Please specify exactly one input file and one output file");
			files[atFile++] = v;
		}
	}

	LAssert(argc >= 3 && files[0] && files[1], usage, argv[0]);
	LAssert(addr <= 0xffff, "Assembly start address must be within range 0 - 0xFFFF (not %x)", addr);
	
	// Allocate 64 kword RAM file
	uint16_t* ram = calloc(1, sizeof(uint16_t) * 0x10000);

	Dasm* d = Dasm_Create();
	
	if(debugSymbols){
		char tmp[4096];
		sprintf(tmp, "%s.dbg", files[1]);
		d->debugFile = fopen(tmp, "w");
		LogV("Opening debug file: %s", tmp);
		LAssert(d->debugFile, "could not open file: %s", tmp);
	}

	uint16_t len = Dasm_Assemble(d, files[0], ram, addr, lastAddr);

	if(d->debugFile) fclose(d->debugFile);

	Dasm_Destroy(&d);

	if(logLevel == 0) DumpRam(ram, len - 1);

	LogV("Writing to: %s", files[1]);
	WriteRam(ram, files[1], len - 1, byteOrder);

	free(ram);
	return 0;
}
