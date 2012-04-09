#ifndef DASM_H
#define DASM_H

#define ENDSWITH(__str, __c) (__str)[strlen(__str) - 1] == (__c)
#define STARTSWITH(__str, __c) ((__str)[0] == (__c))

#include <stdint.h>
#include <stdio.h>

extern int logLevel;

typedef struct Define_vec_s Defines;
typedef struct Label_vec_s Labels;

typedef struct {
	const char* currentFile;
	char* baseDir;
	int lineNumber;

	uint16_t* ram;

	uint16_t endAddr;

	Defines* defines;
	Labels* labels;

	FILE* debugFile;
} Dasm;

Dasm* Dasm_Create();
void Dasm_Destroy(Dasm** dasm);
uint16_t Dasm_Assemble(Dasm* me, const char* ifilename, uint16_t* ram, int startAddr, uint16_t endAddr);

#endif
