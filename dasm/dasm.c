#include "cvector.h"
#include "common.h"

static const char* dinsNames[] = DINSNAMES;
static const char* valNames[] = VALNAMES;

int logLevel;
static int lineNumber = 0;

#define ENDSWITH(__str, __c) (__str)[strlen(__str) - 1] == (__c)
#define STARTSWITH(__str, __c) ((__str)[0] == (__c))
#define LAssertError(__v, ...) if(!(__v)){ LogI("line: %d", lineNumber); LogF(__VA_ARGS__); exit(1); }
typedef Vector(uint16_t) U16Vec;

typedef struct {
	char* label;
	uint16_t addr;
	uint16_t id;
	bool found;
	U16Vec positions;
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
	
void Labels_Replace(Labels* me, uint16_t* ram)
{
	LogD("replacing labels");

	LogD("%d", me->count);

	Label* l;
	Vector_ForEach(*me, l){
		LAssert(l->found, "No such label: %s", l->label);
		LogD("label: %s", l->label);

		uint16_t* pos;
		Vector_ForEach(l->positions, pos){
			ram[*pos] = l->addr;
			LogD("replaced label %s @ 0x%04x with 0x%04x", l->label, *pos, l->addr);
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
		if(*buffer == '\"' || *buffer == '\''){
			if(expecting && expecting == *buffer) expecting = 0;
			else expecting = *buffer;
		}

		token[at++] = *(buffer++);
	}

	LAssertError(!expecting, "unterminated quotation, expected: '%c'", expecting);	
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
		LAssertError(!fail, "No such register: %c", c);
		return -1;
	}

	*label = NULL;

	// POP / [SP++], PEEK / [SP], PUSH / [--SP]
	if(!strcmp(token, "pop") || !strcmp(token, "[sp++]")) return DV_Pop;
	if(!strcmp(token, "peek") || !strcmp(token, "[sp]")) return DV_Peek;
	if(!strcmp(token, "push") || !strcmp(token, "[--sp]")) return DV_Push;

	// SP, PC
	if(!strcmp(token, "sp")) return DV_SP;
	if(!strcmp(token, "pc")) return DV_PC;
	if(!strcmp(token, "o")) return DV_O;
	
	// [next word + register]
	if(sscanf(token, "[0x%x+%c]", nextWord, &c) == 2) return DV_RefNextWordBase + lookUpReg(c, true);
	
	// [NextWord]
	if(sscanf(token, "[0x%x]", nextWord) || sscanf(token, "[%u]", nextWord) == 1) return DV_RefNextWord;

	// Literal or NextWord
	if(sscanf(token, "0x%x", nextWord) == 1 || sscanf(token, "%u", nextWord) == 1){
		LAssertError(*nextWord < 0x10000, "Literal number must be in range 0 - 65535 (0xFFFF)");
		if(*nextWord < 0x20) return DV_LiteralBase + *nextWord;
		return DV_NextWord;
	}

	// [register]
	if(sscanf(token, "[%c]", &c) == 1) return DV_RefBase + lookUpReg(c, true);
	
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

void Assemble(const char* ifilename, uint16_t* ram)
{
	FILE* in = fopen(ifilename, "r");
	LAssert(in, "could not open file: %s", ifilename);

	char buffer[512];
	char token[512];

	bool done = false;
				
	uint16_t addr = 0;
	Labels* labels = Labels_Create();

	do{
		int wrote = 0;
		void Write(uint16_t val){ ram[addr++] = val; wrote++; }

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

			#if 0
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
			else 
			#endif

			// An instruction
			if(toknum == 0){
				insnum = -1;

				#if 0
				// Pseudo instructions
				if(!strcmp(token, "db")){
					LogD("db pseudo");
					insnum = -2;
				}

				// Actual instructions
				else{
				#endif
					for(int i = 0; i < DINS_NUM; i++){
						if(!strcmp(dinsNames[i], token)) {
							insnum = i;
							break;
						}
					}

					LAssertError(insnum != -1, "no such instruction: %s", token);
				//}
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
					Labels_Get(labels, opLabels[i], addr);
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
}

int main(int argc, char** argv)
{
	logLevel = 2;

	int atFile = 0;
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
				LogI("  -h    show this help message");
				return 0;
			}
			else if(sscanf(v, "-v%d", &logLevel) == 1){}
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
	
	// Allocate 64 kword RAM file
	uint16_t* ram = malloc(sizeof(uint16_t) * 0x10000);

	LogV("Assembling: %s", files[0]);
	Assemble(files[0], ram);	

	if(logLevel == 0) DumpRam(ram);

	LogV("Writing to: %s", files[1]);
	WriteRam(ram, files[1]);

	free(ram);
	return 0;
}
