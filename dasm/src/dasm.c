#include "dasmi.h"

int GetDir(const char* filename, char* buffer)
{
	int last = 0;
	for(int i = 0; i < strlen(filename); i++){
		if(filename[i] == '\\' || filename[i] == '/') last = i + 1;
	}
	
	if(buffer) strncpy(buffer, filename, last);

	return last;
}

Dasm* Dasm_Create()
{
	Dasm* me = calloc(1, sizeof(Dasm));
	LAssert(me, "Could not allocate RAM for assembler");
	
	me->labels = Labels_Create();

	me->defines = calloc(1, sizeof(Defines));
	Vector_Init(*me->defines, Define);

	return me;
}

void Dasm_Destroy(Dasm** me)
{
	free(*me);
	*me = NULL;
}

uint16_t Dasm_Assemble(Dasm* me, const char* ifilename, uint16_t* ram, int startAddr, uint16_t endAddr)
{
	me->ram = ram;
	me->endAddr = endAddr;

	if(me->baseDir) free(me->baseDir);
	me->baseDir = calloc(1, GetDir(ifilename, NULL));
	GetDir(ifilename, me->baseDir);

	return Assemble(me, ifilename, startAddr, 0);
}
