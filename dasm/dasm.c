#include "cvector.h"
#include "common.h"

#define MAX_STR_SIZE 8192

static const char* dinsNames[] = DINSNAMES;
static const char* valNames[] = VALNAMES;

// Assembler directives
#define AD_NUM (AD_Dw + 1)
#define AD2INS(_n) (-2 - (_n))
#define INS2AD(_n) (-(_n) - 2)

typedef enum                           { AD_Org, AD_Define, AD_Reserve, AD_Fill, AD_IncBin, AD_Include, AD_Dat, AD_Dw } AsmDir;
static const char* adNames[AD_NUM] =   { ".ORG", ".DEFINE", ".RESERVE", ".FILL", ".INCBIN", ".INCLUDE",  "DAT",  ".DW" };
int                adNumArgs[AD_NUM] = {    1,       2,         1,         2,        2,       1,          -1,     -1  };

int logLevel;
static int lineNumber = 0;
static const char* currentFile = NULL;
static char baseDir[MAX_STR_SIZE];

#define ENDSWITH(__str, __c) (__str)[strlen(__str) - 1] == (__c)
#define STARTSWITH(__str, __c) ((__str)[0] == (__c))
#define LAssertError(__v, ...) if(!(__v)){ if(currentFile){ LogI("%s: %d", currentFile, lineNumber); } LogF(__VA_ARGS__); exit(1); }

typedef struct { char* searchReplace[2]; } Define;
typedef Vector(Define) Defines;

typedef struct
{
	uint16_t addr;
	int lineNumber;
} LabelRef;

typedef Vector(LabelRef) LabelRefs;

typedef struct {
	char* label;
	uint16_t addr;
	uint16_t id;
	bool found;
	LabelRefs references;
} Label;

typedef Vector(Label) Labels; 

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
	Vector_Init(l.references, LabelRef);

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

uint16_t Labels_Get(Labels* me, const char* label, uint16_t current, int lineNumber)
{
	Label* l = Labels_Lookup(me, label);
	if(!l) l = Labels_Add(me, label);

	LabelRef ref;

	ref.lineNumber = lineNumber;
	ref.addr = current;

	Vector_Add(l->references, ref);

	return l->id;
}
	
void Labels_Replace(Labels* me, uint16_t* ram)
{
	LogD("replacing labels");

	LogD("%d", me->count);

	Label* l;
	Vector_ForEach(*me, l){
		if(!l->found){
			LogF("No such label: %s", l->label);
			LogI("Referenced from:");

			LabelRef* ref;
			Vector_ForEach(l->references, ref){
				LogI("  line: %d", ref->lineNumber);
			}
			exit(1);
		}
		LogD("label: %s", l->label);

		LabelRef* ref;
		Vector_ForEach(l->references, ref){
			ram[ref->addr] = l->addr;
			LogD("replaced label %s @ 0x%04x with 0x%04x", l->label, ref->addr, l->addr);
		}
	}
}

bool StrEmpty(const char* str)
{
	for(int i = 0; i < strlen(str); i++) if(str[i] > 32) return false;
	return true;
}

bool GetLine(FILE* f, char* buffer)
{
	lineNumber++;
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
		if(*buffer == '\"'){
			if(expecting && expecting == *buffer) expecting = 0;
			else expecting = *buffer;
		}

		token[at++] = *(buffer++);
	}

	LAssertError(!expecting, "unterminated quotation, expected: '%c'", expecting);	
	token[at] = '\0';

	return buffer;
}

uint16_t ParseLiteral(const char* str, bool* success, bool failOnError)
{
	unsigned lit = 0xaaaa;

	if(sscanf(str, "0x%x", &lit) == 1 || sscanf(str, "%u", &lit) == 1){
		LAssertError(lit < 0x10000, "Literal number must be in range 0 - 65535 (0xFFFF)");
		if(success) *success = true;
		return lit;
	}

	LAssertError(!failOnError, "could not parse literal: %s", str)
	if(success) *success = false;

	return lit;
}

DVals ParseOperand(const char* tok, unsigned int* nextWord, char** label)
{
	char c;
	char token[MAX_STR_SIZE];
	strcpy(token, tok);

	for(int i = 0; i < strlen(token); i++) token[i] = tolower(token[i]);

	int lookUpReg(char c, bool fail)
	{
		char tab[] = "abcxyzij";
		for(int i = 0; i < 8; i++) if(tab[i] == c) return i;
		LAssertError(!fail, "No such register: %c", c);
		return -1;
	}
		

	LogD("parsing operand: %s", tok);

	*label = NULL;

	// POP / [SP++], PEEK / [SP], PUSH / [--SP]
	if(!strcmp(token, "pop") || !strcmp(token, "[sp++]")) return DV_Pop;
	if(!strcmp(token, "peek") || !strcmp(token, "[sp]")) return DV_Peek;
	if(!strcmp(token, "push") || !strcmp(token, "[--sp]")) return DV_Push;
	
	// SP, PC
	if(!strcmp(token, "sp")) return DV_SP;
	if(!strcmp(token, "pc")) return DV_PC;
	if(!strcmp(token, "o")) return DV_O;

	char buffer[MAX_STR_SIZE];
	
	// [nextword + register]
	if(sscanf(token, "[%[^+]+%c]", buffer, &c) == 2){
		// 0x1 or 1...
		bool isLiteral = false;
		*nextWord = ParseLiteral(buffer, &isLiteral, false);
		if(isLiteral) goto done_parsing;

		// label
		*nextWord = 0;
		*label = strdup(buffer);

		done_parsing:
		return DV_RefRegNextWordBase + lookUpReg(c, true);
	}

	if(sscanf(token, "[%[^]]]", buffer)){
		// [register]
		int reg = lookUpReg(*buffer, false);
		if(buffer[1] == 0 && reg != -1) return DV_RefBase + reg;

		// [nextword]
		bool isLiteral = false;
		*nextWord = ParseLiteral(buffer, &isLiteral, false);
		if(isLiteral) return DV_RefNextWord;
	
		// [label]
		*nextWord = 0;
		*label = strdup(buffer);
		return DV_RefNextWord;
	}

	// literal or nextword
	bool isLiteral = false;
	*nextWord = ParseLiteral(tok, &isLiteral, false);
	if(isLiteral){
		if(*nextWord < 0x20) return DV_LiteralBase + *nextWord;
		return DV_NextWord;
	}

	// register
	if(strlen(token) == 1 && sscanf(token, "%c", &c) == 1){
		int reg = lookUpReg(c, false);
		if(reg != -1) return DV_A + reg;
	}

	// label
	*nextWord = 0;
	*label = strdup(tok);
	
	return DV_NextWord;
}

char* UnquoteStr(char* target, const char* str)
{
	LAssertError(
		(STARTSWITH(str, '"') && ENDSWITH(str, '"')) ||
		(STARTSWITH(str, '\'') && ENDSWITH(str, '\'')), 
		"Error parsing quoted string: %s", str);

	return strncpy(target, str + 1, strlen(str) - 2);
}

void PreProcess(Defines* defines, char* str)
{
	Define* it;
	Vector_ForEach(*defines, it){
		// XXX: optimize this
		char buffer[MAX_STR_SIZE];
		StrReplace(buffer, str, it->searchReplace[0], it->searchReplace[1]);
		strcpy(str, buffer);
	}
}

uint16_t Assemble(const char* ifilename, uint16_t* ram, int addr, Labels* labels, Defines* defines, uint16_t lastAddr)
{
	LogV("Assembling: %s", ifilename);
	LogD("at address: 0x%x", addr);

	FILE* in = fopen(ifilename, "r");
	LAssertError(in, "could not open file: %s", ifilename);

	const char* saveFile = currentFile;
	int saveLineNumber = lineNumber;

	currentFile = ifilename;
	lineNumber = 1;

	char buffer[MAX_STR_SIZE];
	char token[MAX_STR_SIZE];

	bool done = false;

	do{
		int wrote = 0;
		void Write(uint16_t val){
			LAssertError(addr <= lastAddr, "Out of space in binary, at last address %x", lastAddr);
			ram[addr++] = val;
			wrote++;
		}

		char* line = buffer;

		done = GetLine(in, line);
		PreProcess(defines, line);

		int insnum = -1;
		Define def;

		int numOperands = 0;
		uint8_t operands[2] = {0, 0};
		unsigned int nextWord[2] = {0, 0};
		char* opLabels[2] = {NULL, NULL};
		char ibFile[MAX_STR_SIZE]; // file for incbin

		uint16_t tmp = 0;
		
		if(StrEmpty(line)) continue;

		int toknum = 0;

		for(;;){
			line = GetToken(line, token);
			
			if(StrEmpty(token)) break;
	
			char tokenUpper[MAX_STR_SIZE];
			for(int i = 0; i < strlen(token) + 1; i++) tokenUpper[i] = toupper(token[i]);

			// A label, add it and continue	
			if(toknum == 0 && STARTSWITH(token, ':')) {
				Labels_Define(labels, token + 1, addr);
				continue;
			}

			// Handle arguments to directives
			if(toknum > 0 && insnum < -1){
				AsmDir ad = INS2AD(insnum);

				LAssertError(adNumArgs[ad] == -1 || toknum <= adNumArgs[ad],
					"%s expects %d arguments", adNames[ad], adNumArgs[ad]);

				// .DW / DAT

			 	if(ad == AD_Dw || ad == AD_Dat){
					LogD(".dw data");
					// List of characters on the "string" format
					if(token[0] == '"'){
						LAssert(ENDSWITH(token, '"'), "expected \"");
						int len = strlen(token) - 2;
						for(int i = 0; i < len; i++){
							Write(token[i + 1]);
							LogD("%c", token[i + 1]);
						}
					}

					// Literal number (hex or dec)
					else{ 
						LogD("Literal: %s", token);
						Write(ParseLiteral(token, NULL, true));
					}
				}

				// .ORG
				else if(ad == AD_Org) addr = ParseLiteral(token, NULL, true);
		
				// .DEFINE
				else if(ad == AD_Define){	
					def.searchReplace[toknum - 1] = strdup(token);
					if(toknum == 2) Vector_Add(*defines, def); 
				}	

				// .FILL
				else if(ad == AD_Fill){
					if(toknum == 1) tmp = ParseLiteral(token, NULL, true);
					else{
						uint16_t c = ParseLiteral(token, NULL, true);
						for(int i = 0; i < tmp; i++) Write(c);
					}
				}

				// .RESERVE
				else if(ad == AD_Reserve) addr += ParseLiteral(token, NULL, true);

				// .INCBIN
				else if(ad == AD_IncBin){
					if(toknum == 1) UnquoteStr(ibFile, token);
					if(toknum == 2){
						char buffer[MAX_STR_SIZE];
						sprintf(buffer, "%s%s", baseDir, ibFile);
						DByteOrder bo = (!strcmp(tokenUpper, "BE")) ? DBO_BigEndian : DBO_LittleEndian;
						addr += LoadRamMax(ram + addr, buffer, lastAddr - addr, bo);
					}
				}

				// .INCLUDE
				else if(ad == AD_Include){
					char buffer[MAX_STR_SIZE];
					sprintf(buffer, "%s%s", baseDir, UnquoteStr(ibFile, token));
					addr = Assemble(buffer, ram, addr, labels, defines, lastAddr);
				}
			}

			// An instruction or assembly directive
			else if(toknum == 0){
				insnum = -1;

				// Assembly directives
				for(int i = 0; i < AD_NUM; i++){
					if(!strcmp(adNames[i], tokenUpper)){
						LogD("Directive: %s", token);
						insnum = AD2INS(i);
						goto done_searching;
					}
				}

				// Actual instructions
				for(int i = 0; i < DINS_NUM; i++){
					if(!strcmp(dinsNames[i], tokenUpper)) {
						insnum = i;
						goto done_searching;
					}
				}

				LAssertError(insnum != -1, "no such instruction: %s", token);

				done_searching: 
				while(0){} // "NOOP", must have something after a label
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

		// Pseudo instruction handled (-2) or no instruction found 
		// (probably a label but no instruction), continue with next line
		if(insnum <= -1) continue;

		// Line parsed, write parsed stuff

		if(insnum >= DINS_EXT_BASE){
			operands[1] = operands[0];
			operands[0] = insnum - DINS_EXT_BASE;
			opLabels[1] = opLabels[0];
			numOperands = 2;
			insnum = DI_NonBasic;
		}

		LogD("Line: '%s'", buffer);
		LogD("  Instruction: %s (0x%02x)", dinsNames[insnum], insnum);
		if(insnum == DI_NonBasic) LogD("  Extended Instruction: %s (0x%02x)", dinsNames[DINS_EXT_BASE + operands[0]], operands[0]);

		for(int i = 0; i < numOperands; i++){
			int v = operands[i];
			LogD("  Operand %d: %s (0x%02x)", i + 1, valNames[v], v);
			if(opHasNextWord(v)){
				LogD("  NextWord: 0x%04x", nextWord[i]);
			}
		}

		// Write to output
	
		// Write the instruction and its operands
		uint16_t ins = (insnum & 0xf) | ((operands[0] & 0x3f) << 4) | ((operands[1] & 0x3f) << 10);
		Write(ins);

		// Write "nextwords"
		for(int i = 0; i < numOperands; i++){
			int v = operands[i];
			if(opHasNextWord(v)){
				// This refers to a label
				if(opLabels[i]){
					Labels_Get(labels, opLabels[i], addr, lineNumber);
					free(opLabels[i]);
				}

				Write(nextWord[i]);
			}
		}

		char dump[64];
		memset(dump, 64, 0);
		char* d = dump;

		for(int i = 0; i < wrote; i++) d += sprintf(d, "%04x ", ram[addr - wrote + i]);
		LogD("  Output: %s", dump);

	} while (done);

	Labels_Replace(labels, ram);

	fclose(in);
	
	currentFile = saveFile;
	lineNumber = saveLineNumber;

	return addr;
}

char* GetDir(const char* filename, char* buffer)
{
	int last = 0;
	for(int i = 0; i < strlen(filename); i++){
		if(filename[i] == '\\' || filename[i] == '/') last = i + 1;
	}

	return strncpy(buffer, filename, last);	
}

int main(int argc, char** argv)
{
	logLevel = 2;

	int atFile = 0;
	unsigned addr = 0;
	unsigned lastAddr = 0xffff;

	const char* files[2] = {NULL, NULL};
	const char* usage = "usage: %s (-vX | -h) [dasm file] [out binary]";

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
				return 0;
			}
			else if(sscanf(v, "-v%d", &logLevel) == 1){}
			else if(sscanf(v, "-s%x", &addr) == 1){}
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
	
	GetDir(files[0], baseDir);
	
	// Allocate 64 kword RAM file
	uint16_t* ram = malloc(sizeof(uint16_t) * 0x10000);

	Labels* labels = Labels_Create();

	Defines defines;
	Vector_Init(defines, Define);

	uint16_t len = Assemble(files[0], ram, addr, labels, &defines, lastAddr);

	if(logLevel == 0) DumpRam(ram, len - 1);

	LogV("Writing to: %s", files[1]);
	WriteRam(ram, files[1], len - 1);

	free(ram);
	return 0;
}
