#include "common.h"

void WriteRam(uint16_t* ram, const char* filename)
{
	FILE* out = fopen(filename, "w");
	LAssert(out, "could not open new file for writing: %s", filename);

	int end = 0xffff;
	while(ram[--end] == 0);

	for(int i = 0; i < end + 1; i++){
		fputc(ram[i] & 0xff, out);
		fputc(ram[i] >> 8, out);
	}

	fclose(out);
}

void LoadRam(uint16_t* ram, const char* filename)
{
	FILE* f = fopen(filename, "r");
	LAssert(f, "could not open file");
	
	int addr = 0;

	for(;;){
		int c1 = 0, c2 = 0;
		if((c1 = fgetc(f)) == EOF || (c2 = fgetc(f)) == EOF) break;
		ram[addr++] = (c2 & 0xff) << 8 | (c1 & 0xff);
	}
}

void DumpRam(uint16_t* ram)
{
	int end = 0xffff;
	while(ram[--end] == 0);

	for(int i = 0; i < end + 1; i++){
		if(i % 8 == 0) printf("\n%04x: ", i);
		printf("%04x ", ram[i]);
	}

	printf("\n\n");
}

bool opHasNextWord(uint16_t v){ 
	return (v > DV_RefRegNextWordBase && v <= DV_RefRegNextWordTop) 
		|| v == DV_RefNextWord || v == DV_NextWord; 
}
