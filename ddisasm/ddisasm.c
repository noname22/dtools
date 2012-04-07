#include "common.h"

static const char* dinsNames[] = DINSNAMES;
static const char* valNames[] = VALNAMES;

int logLevel;

void Disasm(uint16_t* ram)
{
	int pc = 0;
	
	int end = 0xffff;
	while(ram[--end] == 0);
	
	while(pc <= end){
		int hasRead = 0;
		uint16_t read(){ hasRead++; return ram[pc++]; }

		bool hasNextWord[2];
		uint16_t nextWord[2];
		uint16_t pIns = read();
		DVals v[2];

		int numVals = 2;

		DIns ins = pIns & 0xf;
		v[0] = (pIns >> 4) & 0x3f;
		v[1] = (pIns >> 10) & 0x3f;

		for(int i = 0; i < 2; i++){
			if((hasNextWord[i] = opHasNextWord(v[i]))) nextWord[i] = read();
		}
		
		if(ins == DI_NonBasic){
			numVals = 1;
			ins = v[0] + DINS_EXT_BASE;
			v[0] = v[1];
			hasNextWord[0] = hasNextWord[1];
			nextWord[0] = nextWord[1];

			hasNextWord[1] = false;
		}
		
		int n = printf("\t%s ", dinsNames[ins]);

		for(int i = 0; i < numVals; i++){
			char numStr[64] = {0};
			if(hasNextWord[i]) sprintf(numStr, "0x%x", nextWord[i]);

			char str[64];
			n += printf("%s", StrReplace(str, valNames[v[i]], "NW", numStr));
			if(i == 0 && numVals == 2) n += printf(", ");
		}
		
		printf("%*s", 40 - n, "; ");
		printf("0x%04x | ", pc - hasRead);
		for(int i = 0; i < hasRead; i++) printf("%04x ", ram[pc - hasRead + i]);

		printf("\n");
	}
}

int main(int argc, char** argv)
{
	logLevel = 1;
	
	LAssert(argc == 2, "usage: %s [dcpu-16 binary]", argv[0]);

	// Allocate 64 kword RAM file
	uint16_t* ram = malloc(sizeof(uint16_t) * 0x10000);

	LoadRam(ram, argv[1]);
	
	if(logLevel == 0) DumpRam(ram);

	Disasm(ram);

	free(ram);
}
