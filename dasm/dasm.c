#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "dcpu16ins.h"
#include "log.h"
#include "cvector.h"

static const char* dinsNames[] = DINSNAMES;
static const char* valNames[] = VALNAMES;

#define ENDSWITH(__str, __c) (__str)[strlen(__str) - 1] == (__c)
#define STARTSWITH(__str, __c) ((__str)[0] == (__c))

typedef Vector(uint16_t) U16Vec;

typedef struct {
	char* label;
	uint16_t addr;
	uint16_t id;
	bool found;
	U16Vec positions;
} Label;

typedef Vector(Label) Labels; 

int WriteInt(FILE* out, uint32_t val, int size){
	for(int i = 0; i < size; i++){
		uint8_t w = (val >> ((size - 1 - i) * 8)) & 0xff;
		fputc(w, out);
		//LogD("wrote: 0x%02x", w);
	}
	return size;
}

Labels* Labels_Create()
{
	Labels* me = calloc(1, sizeof(Labels));
	Vector_Init(*me, Label);
	return me;
}

// XXX: use a hash map or something
Label* Labels_Lookup(Labels* me, const char* label)
{
	Label* it;
	Vector_ForEach(*me, it){
		if(!strcmp(it->label, label)) return it;
	}

	return NULL;
}

Label* Labels_Add(Labels* me, const char* label)
{
	LAssert(Labels_Lookup(me, label) == NULL, "duplicate label: %s", label);

	Label l;
	memset(&l, 0, sizeof(Label));
	l.label = strdup(label);
	l.id = me->count;
	Vector_Init(l.positions, uint16_t);

	Vector_Add(*me, l);

	return &me->elems[me->count - 1];
}

void Labels_Define(Labels* me, const char* label, uint16_t address)
{
	Label* l = Labels_Lookup(me, label);
	if(!l) l = Labels_Add(me, label);
	l->addr = address;
	l->found = true;
} 

uint16_t Labels_Get(Labels* me, const char* label, uint16_t current)
{
	Label* l = Labels_Lookup(me, label);
	if(!l) l = Labels_Add(me, label);

	Vector_Add(l->positions, current);

	return l->id;
}
	
void Labels_Replace(Labels* me, FILE* f)
{
	LogD("replacing labels");

	LogD("%d", me->count);

	Label* l;
	Vector_ForEach(*me, l){
		LAssert(l->found, "No such label: %s", l->label);
		LogD("label: %s", l->label);

		uint16_t* pos;
		Vector_ForEach(l->positions, pos){
			fseek(f, (long)*pos, SEEK_SET);
			WriteInt(f, l->addr, 2);
			LogD("replaced label %s @ 0x%04x with 0x%04x", l->label, *pos, l->addr);
		}
	}
}

bool StrEmpty(const char* str)
{
	for(int i = 0; i < strlen(str); i++){
		if(str[i] >= 32) return false;
	}

	return true;
}

bool GetLine(FILE* f, char* buffer)
{
	int at = 0;

	for(;;) {
		int c = fgetc(f);
		if(c == '\r' || c == '\n' || c == EOF || c == ';'){
			bool ret = c != EOF;
			while(c != '\n' && c != EOF){ c = fgetc(f); }
			buffer[at] = '\0';
			return ret;
		}

		buffer[at++] = c;
	}
}

// XXX: Handle spaces inside []
char* GetToken(char* buffer, char* token)
{
	// skip spaces etc.
	while((*buffer <= 32 || *buffer == ',') && *buffer != 0) buffer++;

	// read into token until the next space or end of string
	int at = 0;
	
	char expecting = 0;

	while((expecting || (*buffer > 32 && *buffer != ',')) && *buffer != 0){
		if(*buffer == '\"' || *buffer == '\''){
			if(expecting && expecting == *buffer) expecting = 0;
			else expecting = *buffer;
		}

		token[at++] = *(buffer++);
	}

	LAssert(!expecting, "unterminated quotation, expected: '%c'", expecting);	
	token[at] = '\0';

	return buffer;
}

DVals ParseOperand(const char* tok, unsigned int* nextWord, char** label)
{
	char c;
	char token[512];
	strcpy(token, tok);

	for(int i = 0; i < strlen(token); i++) token[i] = tolower(token[i]);

	int lookUpReg(char c, bool fail)
	{
		char tab[] = "abcxyzij";
		for(int i = 0; i < 8; i++) if(tab[i] == c) return i;
		LAssert(!fail, "No such register: %c", c);
		return -1;
	}

	*label = NULL;
	
	// [NextWord]
	if(sscanf(token, "[0x%x]", nextWord) || sscanf(token, "[%u]", nextWord) == 1) return DV_RefNextWord;

	// Literal or NextWord
	if(sscanf(token, "0x%x", nextWord) == 1 || sscanf(token, "%u", nextWord) == 1){
		LAssert(*nextWord < 0x10000, "Literal number must be in range 0 - 65535 (0xFFFF)");
		if(*nextWord < 0x20) return DV_LiteralBase + *nextWord;
		return DV_NextWord;
	}

	// [next word + register]
	if(sscanf(token, "[0x%x+%c]", nextWord, &c) == 2) return DV_RefNextWordBase + lookUpReg(c, true);

	// [register]
	if(sscanf(token, "[%c]", &c) == 1) return DV_RefBase + lookUpReg(c, true);

	// POP / [SP++], PEEK / [SP], PUSH / [--SP]
	if(!strcmp(token, "pop") || !strcmp(token, "[sp++]")) return DV_Pop;
	if(!strcmp(token, "peek") || !strcmp(token, "[sp]")) return DV_Peek;
	if(!strcmp(token, "push") || !strcmp(token, "[--SP]")) return DV_Push;

	// SP, PC
	if(!strcmp(token, "sp")) return DV_SP;
	if(!strcmp(token, "pc")) return DV_PC;
	if(!strcmp(token, "o")) return DV_O;
	
	// register
	if(strlen(token) == 1 && sscanf(token, "%c", &c) == 1){
		int reg = lookUpReg(c, false);
		if(reg != -1) return DV_A + reg;
	}

	// Label
	*nextWord = 0;
	*label = strdup(tok);
	
	return DV_NextWord;
}

void Assemble(const char* ifilename, const char* ofilename)
{
	FILE* in = fopen(ifilename, "r");
	FILE* out = fopen(ofilename, "w");

	char buffer[512];
	char token[512];

	bool done = false;
				
	uint16_t addr = 0;
	Labels* labels = Labels_Create();

	void Write(uint32_t val, int size){
		//LogD("starting at addr: 0x%04x", (unsigned int)addr);
		addr += WriteInt(out, val, size);
	}

	do{
		char* line = buffer;
		done = GetLine(in, line);
		int insnum = -1;

		int numOperands = 0;
		uint8_t operands[2] = {0, 0};
		unsigned int nextWord[2] = {0, 0};
		char* opLabels[2] = {NULL, NULL};
		
		if(StrEmpty(line)) continue;

		int toknum = 0;

		for(;;){
			line = GetToken(line, token);
			
			if(StrEmpty(token)) break;

			// A label, add it and continue	
			if(toknum == 0 && STARTSWITH(token, ':')) {
				Labels_Define(labels, token + 1, addr);
				continue;
			}

			// Handle db pseudo instruction arguments
			if(toknum > 0 && insnum == -2){
				LogD("db data");
				// characters on 'c' format
				if(token[0] == '\''){
					LAssert(strlen(token) == 3, "syntax error");
					LAssert(ENDSWITH(token, '\''), "expected \'");
					Write(token[1], 1);
				}

				// List of characters on the "string" format
				if(token[0] == '"'){
					LAssert(ENDSWITH(token, '"'), "expected \"");
					int len = strlen(token) - 2;
					for(int i = 0; i < len - 1; i++){
						Write(token[i + 1], 1);
						LogD("%c", token[i + 1]);
					}
				}
			}

			// An instruction
			else if(toknum == 0){
				insnum = -1;

				// Pseudo instructions
				if(!strcmp(token, "db")){
					LogD("db pseudo");
					insnum = -2;
				}

				// Actual instructions
				else{
					for(int i = 0; i < DINS_NUM; i++){
						if(!strcmp(dinsNames[i], token)) {
							insnum = i;
							break;
						}
					}

					LAssert(insnum != -1, "no such instruction: %s", token);
				}
			}

			else if( toknum == 1 || toknum == 2 ){
				operands[toknum - 1] = ParseOperand(token, nextWord + toknum - 1, opLabels + toknum - 1);
				numOperands++;
			}

			else {
				LogF("Error parsing");
				exit(1);
			}
				
			toknum++;
		}

		// Line parsed, write parsed stuff

		int eins = -1;
		if(insnum >= DINS_EXT_BASE){
			eins = insnum - DINS_EXT_BASE;
			insnum = DI_NonBasic;
		}

		LogD("Line: %s", buffer);
		LogD("  Instruction: %s (0x%02x)", dinsNames[insnum], insnum);
		if(eins != -1) LogD("  Extended Instruction: %s (0x%02x)", dinsNames[DINS_EXT_BASE + eins], eins);

		for(int i = 0; i < numOperands; i++){
			int v = operands[i];
			LogD("  Operand %d: %s (0x%02x)", i + 1, valNames[v], v);
			if((v > DV_RefNextWordBase && v <= DV_RefNextWordTop) || 
				v == DV_RefNextWord || v == DV_NextWord){
				LogD("  NextWord: 0x%04x", nextWord[i]);
			}
		}

		// Write to output
	
		// Write the instruction and its operands
		uint16_t ins = (insnum & 0xf) | ((operands[0] & 0x3f) << 4) | ((operands[1] & 0x3f) << 10);
		Write(ins, 2);

		// Write "nextwords"
		for(int i = 0; i < numOperands; i++){
			int v = operands[i];
			if((v > DV_RefNextWordBase && v <= DV_RefNextWordTop) || v == DV_RefNextWord || v == DV_NextWord){
				// This refers to a label
				if(opLabels[i]){
					Labels_Get(labels, opLabels[i], addr);
					free(opLabels[i]);
				}

				Write(nextWord[i], 2);
			}
		}
		
	} while (done);

	Labels_Replace(labels, out);	

	fclose(in);
	fclose(out);
}

int main(int argc, char** argv)
{
	LAssert(argc == 3, "usage: %s [dasm file] [out binary]", argv[0]);
	Assemble(argv[1], argv[2]);	

	return 0;
}
