#ifndef DASMI_H
#define DASMI_H

#include "common.h"
#include "dasm.h"

#define MAX_STR_SIZE 8192
#define LAssertError(__v, ...) \
	if(!(__v)){ \
		if(me->currentFile) LogI("@ %s:%d", me->currentFile, me->lineNumber); \
		LogF(__VA_ARGS__); \
		exit(1);\
	}

typedef Vector(char) CharVec;

typedef struct 
{
	int sourceLine; // Originating line
	CharVec str;	// The macro
} Macro;

typedef Macro* MacroPtr;
typedef Vector(MacroPtr) MacroVec;

typedef struct { char* searchReplace[2]; } Define;
Vector(Define);

typedef struct
{
	uint16_t addr;
	uint16_t insAddr;
	char* filename;
	int lineNumber;
	bool relative;
} LabelRef;

typedef Vector(LabelRef) LabelRefs;

typedef struct {
	char* label;

	uint16_t addr;
	uint16_t id;
	bool found;

	int lineNumber;
	char* filename;

	LabelRefs references;
} Label;

Vector(Label); 

Labels* Labels_Create();
Label* Labels_Lookup(Labels* me, const char* label);
Label* Labels_Add(Labels* me, const char* label);
void Labels_Define(Labels* me, Dasm* d, const char* label, uint16_t address, const char* filename, int lineNumber);
uint16_t Labels_Get(Labels* me, const char* label, uint16_t current, uint16_t insAddr, const char* filename, int lineNumber);
void Labels_Replace(Labels* me, uint16_t* ram);
bool GetLine(Dasm* me, FILE* f, char* buffer);
char* GetToken(Dasm* me, char* buffer, char* token);
uint16_t Assemble(Dasm* me, const char* ifilename, int addr, int depth);

#endif
