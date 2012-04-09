#include "dasmi.h"

static const char* dinsNames[] = DINSNAMES;
static const char* valNames[] = VALNAMES;

// Assembler directives
#define AD_NUM (AD_Dw + 1)
#define AD2INS(_n) (-2 - (_n))
#define INS2AD(_n) (-(_n) - 2)

typedef enum                           { AD_Org, AD_Define, AD_Reserve, AD_Fill, AD_IncBin, AD_Include, AD_Macro, AD_End, AD_Dat, AD_Dw } AsmDir;
static const char* adNames[AD_NUM] =   { ".ORG", ".DEFINE", ".RESERVE", ".FILL", ".INCBIN", ".INCLUDE",  "MACRO", "END", "DAT",  ".DW"  };
int                adNumArgs[AD_NUM] = {    1,       2,         1,         2,        2,          1,         -1,     0,     -1,    -1    };

bool StrEmpty(const char* str)
{
	for(int i = 0; i < strlen(str); i++) if(str[i] > 32) return false;
	return true;
}

char* LStrip(char* str)
{
	while(*str <= 32 && *str != 0) str++;
	return str;
}

uint16_t ParseLiteral(Dasm* me, const char* str, bool* success, bool failOnError)
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

DVals ParseOperand(Dasm* me, const char* tok, unsigned int* nextWord, char** label)
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
	char buffer2[MAX_STR_SIZE];

	char* b1 = buffer, *b2 = buffer2;
	
	// [nextword + register] or [register + nextword]
	if(sscanf(token, "[%[^+]+%[^]]s]", buffer, buffer2) == 2){
		// if it's on the format [register + nextword], flip it
		bool isLiteral = false;
		*nextWord = ParseLiteral(me, b2, &isLiteral, false);
		if(isLiteral){
			b2 = buffer;
			b1 = buffer2;
		}

		LogD("b1 '%s' b2 '%s'", b1, b2);
		LogD("nw: %x", *nextWord);

		// 0x1 or 1...
		isLiteral = false;
		*nextWord = ParseLiteral(me, b1, &isLiteral, false);
		if(isLiteral) goto done_parsing;

		// label
		*nextWord = 0;
		*label = strdup(b1);

		done_parsing:
		b2 = LStrip(b2);
		LogD("looking for reg '%c' (%s)", b2[0], b2);
		return DV_RefRegNextWordBase + lookUpReg(b2[0], true);
	}

	if(sscanf(token, "[%[^]]]", buffer)){
		// [register]
		int reg = lookUpReg(*buffer, false);
		if(buffer[1] == 0 && reg != -1) return DV_RefBase + reg;

		// [nextword]
		bool isLiteral = false;
		*nextWord = ParseLiteral(me, buffer, &isLiteral, false);
		if(isLiteral) return DV_RefNextWord;
	
		// [label]
		*nextWord = 0;
		*label = strdup(buffer);
		return DV_RefNextWord;
	}

	// literal or nextword
	bool isLiteral = false;
	*nextWord = ParseLiteral(me, tok, &isLiteral, false);
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

char* UnquoteStr(Dasm* me, char* target, const char* str)
{
	LAssertError(
		(STARTSWITH(str, '"') && ENDSWITH(str, '"')) ||
		(STARTSWITH(str, '\'') && ENDSWITH(str, '\'')), 
		"Error parsing quoted string: %s", str);

	return strncpy(target, str + 1, strlen(str) - 2);
}

uint16_t Assemble(Dasm* me, const char* ifilename, int addr, int depth)
{
	LogV("Assembling: %s", ifilename);
	LogD("at address: 0x%x", addr);

	FILE* in = fopen(ifilename, "r");
	LAssertError(in, "could not open file: %s", ifilename);

	const char* saveFile = me->currentFile;
	int saveLineNumber = me->lineNumber;

	me->currentFile = ifilename;
	me->lineNumber = 0;

	char buffer[MAX_STR_SIZE];
	char token[MAX_STR_SIZE];

	bool done = false;

	do{
		int wrote = 0;
		void Write(uint16_t val){
			LAssertError(addr <= me->endAddr, "Out of space in binary, at last address %x", me->endAddr);
			me->ram[addr++] = val;
			wrote++;
		}

		char* line = buffer;

		done = GetLine(me, in, line);

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
			line = GetToken(me, line, token);
			
			if(StrEmpty(token)) break;

			char tokenUpper[MAX_STR_SIZE];
			for(int i = 0; i < strlen(token) + 1; i++) tokenUpper[i] = toupper(token[i]);

			// A label, add it and continue	
			if(toknum == 0 && STARTSWITH(token, ':')) {
				Labels_Define(me->labels, token + 1, addr);
				continue;
			}

			// Handle arguments to directives
			if(toknum > 0 && insnum < -1){
				AsmDir ad = INS2AD(insnum);

				LAssertError(adNumArgs[ad] == -1 || 
					(toknum <= adNumArgs[ad] && toknum + 1 >= adNumArgs[ad]),
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
						Write(ParseLiteral(me, token, NULL, true));
					}
				}

				// .ORG
				else if(ad == AD_Org) addr = ParseLiteral(me, token, NULL, true);
		
				// .DEFINE
				else if(ad == AD_Define){	
					def.searchReplace[toknum - 1] = strdup(token);
					if(toknum == 2) Vector_Add(*me->defines, def); 
				}	

				// .FILL
				else if(ad == AD_Fill){
					if(toknum == 1) tmp = ParseLiteral(me, token, NULL, true);
					else{
						uint16_t c = ParseLiteral(me, token, NULL, true);
						for(int i = 0; i < tmp; i++) Write(c);
					}
				}

				// .RESERVE
				else if(ad == AD_Reserve) addr += ParseLiteral(me, token, NULL, true);

				// .INCBIN
				else if(ad == AD_IncBin){
					if(toknum == 1) UnquoteStr(me, ibFile, token);
					if(toknum == 2){
						char buffer[MAX_STR_SIZE];
						sprintf(buffer, "%s%s", me->baseDir, ibFile);
						DByteOrder bo = (!strcmp(tokenUpper, "BE")) ? DBO_BigEndian : DBO_LittleEndian;
						addr += LoadRamMax(me->ram + addr, buffer, me->endAddr - addr, bo);
					}
				}

				// .INCLUDE
				else if(ad == AD_Include){
					char buffer[MAX_STR_SIZE];
					sprintf(buffer, "%s%s", me->baseDir, UnquoteStr(me, ibFile, token));
					addr = Assemble(me, buffer, addr, depth + 1);
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
				operands[toknum - 1] = ParseOperand(me, token, nextWord + toknum - 1, opLabels + toknum - 1);
				numOperands++;
			}
				
			toknum++;
		}

		// Pseudo instruction handled (-2) or no instruction found 
		// (probably a label but no instruction), continue with next line
		if(insnum <= -1) continue;
		
		if(insnum < DINS_EXT_BASE){ LAssertError(toknum == 3, "baisc instructions expect 2 operands (not %d)", toknum - 1);}
		else { LAssertError(toknum == 2, "extended instructions expect 1 operand (not %d)", toknum - 1); }

		// Line parsed, write parsed stuff

		bool hasNw[] = {opHasNextWord(operands[0]), opHasNextWord(operands[1])};
		
		if(insnum >= DINS_EXT_BASE){
			operands[1] = operands[0];
			operands[0] = insnum - DINS_EXT_BASE;
			opLabels[1] = opLabels[0];

			hasNw[1] = hasNw[0]; 
			hasNw[0] = false;
			nextWord[1] = nextWord[0];

			numOperands = 2;
			insnum = DI_NonBasic;
		}

		LogD("Line: '%s'", buffer);
		LogD("  Instruction: %s (0x%02x)", dinsNames[insnum], insnum);
		if(insnum == DI_NonBasic) LogD("  Extended Instruction: %s (0x%02x)", dinsNames[DINS_EXT_BASE + operands[0]], operands[0]);

		for(int i = 0; i < numOperands; i++){
			int v = operands[i];
			LogD("  Operand %d: %s (0x%02x)", i + 1, valNames[v], v);
			if(hasNw[i]){
				LogD("  NextWord: 0x%04x", nextWord[i]);
			}
		}

		// Write to output
	
		// Write the instruction and its operands
		uint16_t ins = (insnum & 0xf) | ((operands[0] & 0x3f) << 4) | ((operands[1] & 0x3f) << 10);
		Write(ins);
		
		// Write "nextwords"
		for(int i = 0; i < numOperands; i++){
			if(hasNw[i]){
				// This refers to a label
				if(opLabels[i]){
					Labels_Get(me->labels, opLabels[i], addr, me->lineNumber);
					free(opLabels[i]);
				}

				Write(nextWord[i]);
			}
		}

		char dump[64];
		memset(dump, 64, 0);
		char* d = dump;

		for(int i = 0; i < wrote; i++) d += sprintf(d, "%04x ", me->ram[addr - wrote + i]);
		LogD("  Output: %s", dump);
		
		// Debug file
		if(me->debugFile){
			fprintf(me->debugFile, "%04x %04x %d %s\n", addr - wrote, wrote, me->lineNumber, me->currentFile);
		}

	} while (done);

	Labels_Replace(me->labels, me->ram);

	fclose(in);
	
	me->currentFile = saveFile;
	me->lineNumber = saveLineNumber;

	return addr;
}


