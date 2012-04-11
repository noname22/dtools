#ifndef DINTERPRET_H
#define DINTERPRET_H

#include "dcpu.h"
#include "common.h"

typedef char* CharPtr;

typedef Vector(CharPtr) CharPtrVec;
typedef Vector(char) CharVec;
typedef Vector(int) IntVec;

typedef struct {
	uint16_t addr;
	bool enabled;
} BreakPoint;

typedef Vector(BreakPoint) BreakPointVec;

typedef struct {
	CharVec file;
	IntVec lineIndices;
	char* filename;
} SourceFile;

typedef SourceFile* SourceFilePtr;

typedef Vector(SourceFilePtr) SourceFilePtrVec;

typedef struct {
	uint16_t addr, length;
	int line;
	SourceFile* sourceFile;

	CharPtrVec items;
} DebugSymbol;

typedef Vector(DebugSymbol) DebugSymbolVec;

typedef struct {
	int runInstructions;
	int insExec;
	Dcpu* dcpu;

	SourceFilePtrVec sourceFiles;
	DebugSymbolVec debugSymbols;
	BreakPointVec breakPoints;

	bool showNextIns;
} Debug;

Debug* Debug_Create(Dcpu* dcpu);
void Debug_Destroy(Debug** debug);
void Debug_Inspector(Dcpu* dcpu, void* debug);
bool Debug_LoadSymbols(Debug* debug, const char* filename);

void Debug_AddBreakPointAddr(Debug* debug, uint16_t addr);
bool Debug_AddBreakPointLine(Debug* debug, const char* filename, int line);
bool Debug_AddBreakPointItem(Debug* debug, const char* item);

bool Debug_RemoveBreakPoint(Debug* debug, int index);
bool Debug_EnableBreakPoint(Debug* debug, int index, bool enabled);

#endif
