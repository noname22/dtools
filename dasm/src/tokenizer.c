#include "dasmi.h"

bool GetLine(Dasm* me, FILE* f, char* buffer)
{
	me->lineNumber++;
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

char* GetToken(Dasm* me, char* line, char* token)
{
	// skip spaces etc.
	while((*line <= 32 || *line == ',') && *line != 0) line++;

	// read into token until the next space or end of string
	int at = 0;
	
	char expecting = 0;

	char start[] = "\"'[";
	char end[] = "\"']";


	while((expecting || (*line > 32 && *line != ',')) && *line != 0){
		if(expecting){
			if(*line == expecting) expecting = 0;
		}else{
			for(int i = 0; i < sizeof(start); i++){
				if(*line == start[i]) expecting = end[i];
			}
		}
		token[at++] = *(line++);
	}

	LAssertError(!expecting, "unterminated quotation, expected: '%c'", expecting);	
	token[at] = '\0';

	// Handle defines	
	Define* it;
	Vector_ForEach(*me->defines, it){
		if(!strcmp(token, it->searchReplace[0])){
			strcpy(token, it->searchReplace[1]);
		}
	}

	return line;
}


