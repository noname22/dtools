#include "dinterpret.h"
#include "common.h"
#include <stdlib.h>
	
#define RAssert(__v, ...) if(!(__v)){ printf(__VA_ARGS__); return; }

static const char unknown[] = "(\?\?\?\?)";

// damned signals, there goes my global-less design
static Debug* sDebug = NULL;

void HandleBreak()
{
	if(sDebug->runInstructions == 0) printf("\ntype 'quit' to quit\n");
	else printf("\nbreak\n");
	sDebug->runInstructions = 0;
}

#ifndef WIN32
// Unix signal handler

#include <signal.h>
void signalHandler(int signal){ HandleBreak(); }

#else

// Windows signal handler

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>

BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
	if(CEvent == CTRL_C_EVENT || CEvent == CTRL_BREAK_EVENT) HandleBreak();
}

#endif

SourceFile* Debug_GetSourceFileByName(Debug* me, const char* filename)
{
	SourceFile** sit;
	Vector_ForEach(me->sourceFiles, sit){
		if(!strcmp((*sit)->filename, filename)){
			return *sit;
		}
	}

	return NULL;
}

DebugSymbol* Debug_GetDebugSymbolByItem(Debug* me, const char* item)
{
	DebugSymbol* it;
	Vector_ForEach(me->debugSymbols, it){
		char** iit;
		Vector_ForEach(it->items, iit){
			if(!strcmp(item, *iit)) return it;
		}
	}

	return NULL;
}

const char* SourceFile_GetLine(SourceFile* me, int line)
{
	if(line - 1 >= me->lineIndices.count) return unknown;
	return me->file.elems + me->lineIndices.elems[line - 1];
}

void Debug_AddBreakPointAddr(Debug* me, uint16_t addr)
{
	BreakPoint bp = {addr, true};
	Vector_Add(me->breakPoints, bp);
}

bool Debug_AddBreakPointLine(Debug* me, const char* filename, int line)
{
	SourceFile* sf = Debug_GetSourceFileByName(me, filename);
	if(!sf) return false;

	DebugSymbol* it;
	int lastLine = 0;
	Vector_ForEach(me->debugSymbols, it){
		if(sf == it->sourceFile && line > lastLine && line <= it->line){
			Debug_AddBreakPointAddr(me, it->addr);
			return true;
		}
		lastLine = it->line; 
	}

	return false;
}

bool Debug_AddBreakPointItem(Debug* me, const char* item)
{
	DebugSymbol* it;
	Vector_ForEach(me->debugSymbols, it){
		char** iit;
		Vector_ForEach(it->items, iit){
			if(!strcmp(item, *iit)){
				Debug_AddBreakPointAddr(me, it->addr);
				return true;
			}
		}
	}
	
	return false;
}

bool Debug_RemoveBreakPoint(Debug* me, int index)
{
	if(index < me->breakPoints.count){
		Vector_Remove(me->breakPoints, index);
		return true;
	}
	return false;
}

void Debug_PrintBreakPoint(Debug* me, BreakPoint* bp)
{
	const char* onoff[] = {"disabled", "enabled"};
	printf("0x%04x %s", bp->addr, onoff[bp->enabled]);
}

bool Debug_EnableBreakPoint(Debug* me, int index, bool enabled)
{
	if(index < me->breakPoints.count){
		me->breakPoints.elems[index].enabled = enabled;
		return true;
	}
	return false;
}

DebugSymbol* GetSymbolFromAddress(Debug* me, uint16_t addr)
{
	DebugSymbol* it;
	Vector_ForEach(me->debugSymbols, it){
		if(addr >= it->addr && addr < it->addr + it->length){
			return it;
		}
	}

	return NULL;
}

void Debug_Inspector(Dcpu* dcpu, void* vme)
{
	Debug* me = vme;
	me->insExec++;
	bool done = false;

	
	BreakPoint* bit;
	Vector_ForEach(me->breakPoints, bit){
		if(Dcpu_GetRegister(dcpu, DR_PC) == bit->addr && bit->enabled){
			printf("hit breakpoint: ");
			Debug_PrintBreakPoint(vme, bit);
			printf("\n");
			me->runInstructions = 0;
		}
	}

	if(me->runInstructions != 0){
		if(me->runInstructions > 0) me->runInstructions--;
		return;
	}

	typedef struct {
		const char* cmd;
		void (*fun)(int argc, char** argv);
		const char* desc;
		const char* help;
	} Command;

	void Continue(int argc, char** argv){
		if(argc == 1){
			me->runInstructions = -1;
			done = true;
			return;
		}
		
		unsigned int numIns = 0;
		RAssert(sscanf(argv[1], "%u", &numIns) == 1, "expected literal\n");

		me->runInstructions = numIns;

		done = true;
	}

	void Run(int argc, char** argv)
	{
		if(Dcpu_GetRegister(dcpu, DR_PC) != 0 && !Dcpu_GetExit(dcpu)){
			char buffer[8];
			printf("program already running, restart? (y/n) ");
			fgets(buffer, sizeof(buffer), stdin);

			if(buffer[0] != 'y') return;
		}

		Dcpu_SetExit(dcpu, false);
		Dcpu_SetRegister(dcpu, DR_PC, 0);
		Continue(argc, argv);
	}

	void Where(int argc, char** argv)
	{
		uint16_t addr = Dcpu_GetRegister(dcpu, DR_PC);
		DebugSymbol* s = GetSymbolFromAddress(me, addr);
		if(!s){
			printf("0x%04x: unknown\n", addr);
			return;
		}
		
		printf("%04x (%04x-%04x) %s:%d %s\n", 
			addr, s->addr, s->addr + s->length,s->sourceFile->filename, s->line, 
			SourceFile_GetLine(s->sourceFile, s->line));
	}

	void Step(int argc, char** argv){
		me->runInstructions = 0;
		me->showNextIns = true;
		done = true;
	}

	void Break(int argc, char** argv){
		RAssert(argc >= 2, "break expects at least 1 argument, see help break\n");
		RAssert((!strcmp(argv[1], "list") && argc == 2) || argc == 3, 
			"invalid action or number of arguments, see help break\n");

		void AddBreakPointAddr(uint16_t addr){
			Debug_AddBreakPointAddr(me, addr);
			printf("added breakpoint at address 0x%04x\n", addr);
		}

		void AddBreakPointLine(const char* filename, int line){
			RAssert(Debug_AddBreakPointLine(me, filename, line),
				"could not locate line %d of file '%s' in debug symbols\n", line, filename);
			printf("added breakpoint at %s:%d\n", filename, line);
		}

		if(!strcmp(argv[1], "add")){
			unsigned addr;
			char buffer[1024];

			if(sscanf(argv[2], "+0x%x", &addr) ||  sscanf(argv[2], "+%u", &addr) == 1){
				AddBreakPointAddr(Dcpu_GetRegister(dcpu, DR_PC) + addr);
			}

			else if(sscanf(argv[2], "-0x%x", &addr) ||  sscanf(argv[2], "-%u", &addr) == 1){
				AddBreakPointAddr(Dcpu_GetRegister(dcpu, DR_PC) - addr);
			}

			else if(sscanf(argv[2], "*0x%x", &addr) ||  sscanf(argv[2], "*%u", &addr) == 1){
				AddBreakPointAddr(addr);
			}

			else if(sscanf(argv[2], "%[^:]:%u", buffer, &addr) == 2){
				AddBreakPointLine(buffer, addr);
			}

			else if(sscanf(argv[2], "%u", &addr) == 1){
				DebugSymbol* s = GetSymbolFromAddress(me, Dcpu_GetRegister(dcpu, DR_PC));
				RAssert(s, "when trying to associate line number %d with a source file: " 
					"current address (pc) not associated with a source file, please specify source file\n"
					"(or use 'break add *%d' if you mean an address)\n", addr, addr);
				AddBreakPointLine(s->sourceFile->filename, addr);
			}
		
			else{
				RAssert(Debug_AddBreakPointItem(me, argv[2]), 
					"could not locate function/label '%s' in any of the source files\n", argv[2]);
				printf("added breakpoint at function/label '%s'\n", argv[2]);
			}
		}

		else if(!strcmp(argv[1], "remove")){
			RAssert( Debug_RemoveBreakPoint(me, atoi(argv[2])), "invalid index\n");
			printf("removed breakpoint: %d\n", atoi(argv[2]));
		}

		else if(!strcmp(argv[1], "enable")){
			RAssert( Debug_EnableBreakPoint(me, atoi(argv[2]), true), "invalid index\n");
			printf("enabled breakpoint: %d\n", atoi(argv[2]));
		}

		else if(!strcmp(argv[1], "disable")){
			RAssert( Debug_EnableBreakPoint(me, atoi(argv[2]), false), "invalid index\n");
			printf("disabled breakpoint: %d\n", atoi(argv[2]));
		}

		else if(!strcmp(argv[1], "list")){
			printf("  current breakpoints:\n");
			BreakPoint* it;
			int i = 0;
			Vector_ForEach(me->breakPoints, it){
				printf("    %2d. ", i++);
				Debug_PrintBreakPoint(me, it);
				printf("\n");
			}
		}

		else {
			printf("no such breakpoint action: '%s', see help", argv[1]);
		}
	}

	void Quit(int argc, char** argv){ exit(0); }
	void Print(int argc, char** argv){
		RAssert(argc >= 2, "print requires at least 1 arguments\n");

		typedef struct {const char* what; int v; } Printable;
		Printable printables[] = {
			{"a", DR_A}, {"b", DR_B}, {"c", DR_C}, {"x", DR_X}, {"y", DR_Y}, {"z", DR_Z}, 
			{"i", DR_I}, {"j", DR_J}, {"sp", DR_SP}, {"pc", DR_PC}, {"o", DR_O}, 
			{"regs", -2}, {"stack", -3}
		};
	
		int Lookup(const char* str){
			for(int i = 0; i < sizeof(printables) / sizeof(Printable); i++){
				if(!strcmp(printables[i].what, str)) return printables[i].v;
			}
			return -1;
		}

		for(int i = 1; i < argc; i++){
			int what = Lookup(argv[i]);
		
			if(what == -1){
				unsigned addr = 0;
				if(sscanf(argv[i], "0x%x", &addr) == 1 || sscanf(argv[i], "%u", &addr)){
					printf("[0x%04x]: 0x%04x\n", addr, *Dcpu_GetRam(dcpu) + ((uint16_t)addr));
				}

				else{
					DebugSymbol* s = Debug_GetDebugSymbolByItem(me, argv[i]);

					if(s){
						printf("[%s (0x%04x)]: 0x%04x\n", argv[i], s->addr, *Dcpu_GetRam(dcpu) + ((uint16_t)s->addr));
					}
					else printf("I don't know what a '%s' is\n", argv[i]);
				}
			}

			if(what >= 0) printf("%s: 0x%04x\n", argv[i], Dcpu_GetRegister(dcpu, what));

			// regs
			else if(what == -2){
				for(int i = 0; i <= DR_O; i++) printf("%s: 0x%04x ", printables[i].what, Dcpu_GetRegister(dcpu, i));
				printf("\n");
			}

			// stack
			else if(what == -3){
				uint16_t* ram = Dcpu_GetRam(dcpu);
				uint16_t sp = Dcpu_GetRegister(dcpu, DR_SP);

				if(sp){
					for(int i = 0xffff; i >= sp; i--){
						printf("  0x%04x\n", ram[i]);
					}
				}else printf("  (empty)\n");
			}
		}
	}


	Command commands[] = {
		{"help",  NULL, "this command",
			"  help            lists all commands\n"
			"  help [command]  shows help about [command]\n"
		},

		{"break", Break, "adds, enables, disables or removes a breakpoint",
			"  adding breakpoints\n"
			"    break add *[addr]          adds a breakpoint at address [addr]\n"
			"    break add +[offset]        adds a breakpoint at current pc + [offset]\n"
			"    break add -[offset]        adds a breakpoint at current pc - [offset]\n"
			"    break add [source]:[line]  adds a breakpoint at the given line [line] in the source file [source]\n"
			"    break add [function]       adds a breakpoint at the given [function] (label)\n"
			"\n"
			"  managing breakpoints\n"
			"    break list                 lists and enumerates breakpoints\n"
			"    break remove [index]       removes breakpoint at given index\n"
			"    break enable [index]       enables breakpoint at given index\n"
			"    break disable [index]      disables breakpoint at given index\n"
		},

		{"continue", Continue, "continues execution",
			"  continue ([n ins])   continues execution and runs the specified number of instructions,"
			" or forever if nothing is specified.\n"},

		{"print", Print, "prints status information",
			"  print [r] ([r], [r]...) where r is a|b|c|x|y|z|i|j|o|sp|pc|regs|stack"
			"                  prints the value of the given register(s)/stack\n"
		},
		
		{"quit", Quit, "quits the debugger",
			"  quit            quits the debugger\n"
		},

		{"run", Run, "runs the program",
			"  run ([n ins])   runs the specified number of instructions, or forever if nothing is specified.\n"},

		{"step", Step, "steps on instruction in the program", 
			"  step            steps on instruction in the program.\n"},

		{"where", Where, "shows the current line of code",
			"  where           shows the current line of code from the source file.\n"},

		};

	int nCommands = sizeof(commands) / sizeof(Command);	

	void Help(int argc, char** argv)
	{
		if(argc == 1){
			char n = 0;
			printf("\navailable commands:\n");
			for(int i = 0; i < nCommands; i++){
				printf("  %s %*s %s\n", commands[i].cmd, 
					(int)(10 - strlen(commands[i].cmd)), &n, commands[i].desc);
			}
		}else{
			for(int i = 0; i < nCommands; i++){
				if(!strcmp(commands[i].cmd, argv[1])){
					printf("\n%s usage:\n%s\n", commands[i].cmd, commands[i].help);
					return;
				}
			}
			printf("no help for: %s\n", argv[1]);
		}
	}

	commands[0].fun = Help;

	if(me->showNextIns) Where(0, NULL);
	me->showNextIns = false;

	while(!done){
		char input[512];

		printf("> ");

		if(gets(input) == NULL){
			// ctrl-d
			printf("quit\n");
			Quit(0, NULL);
		}

		int argc = 0;
		char* argv[128];

		char* pch = strtok(input, " ,");

		if(!pch) continue;

		do{
			argv[argc++] = strdup(pch);
		}while((pch = strtok(NULL, " ,")));

		int found = 0;
		int index = 0;

		for(int i = 0; i < nCommands; i++){
			if(!strncmp(argv[0], commands[i].cmd, strlen(argv[0]))){
				index = i;
				found++;
			}
		}
	
		if(found == 0) printf("%s - no such command, type help for more information\n", argv[0]);

		else if(found > 1){
			printf("'%s' is ambiguous, what did you mean? ", argv[0]);
			for(int i = 0; i < nCommands; i++)
				if(!strncmp(argv[0], commands[i].cmd, strlen(argv[0])))
					printf("%s ", commands[i].cmd);
			printf("\n");
		}

		else{
			commands[index].fun(argc, argv);
		}


		for(int i = 0; i < argc; i++) free(argv[i]);
	}
}

Debug* Debug_Create(Dcpu* dcpu)
{
	Debug* me = calloc(1, sizeof(Debug));

	Dcpu_SetInspector(dcpu, Debug_Inspector, me);
	me->dcpu = dcpu;

	Vector_Init(me->sourceFiles, SourceFilePtr);
	Vector_Init(me->debugSymbols, DebugSymbol);
	Vector_Init(me->breakPoints, BreakPoint);

	sDebug = me;

#ifndef WIN32
	// unix
	signal(SIGABRT, &signalHandler);
	signal(SIGTERM, &signalHandler);
	signal(SIGINT, &signalHandler);
#else
	// windows
	LAssertWarn(SetConsoleCtrlHandler( (PHANDLER_ROUTINE)ConsoleHandler, TRUE),
		"could not set control handler, ctrl-c will kill application");
#endif

	return me;
}

bool Debug_LoadSymbols(Debug* me, const char* filename)
{
	FILE* f = fopen(filename, "r");
	if(!f){
		LogW("debug symbols not loaded, could not open file: '%s'", filename);
		return false;
	}

	SourceFile* AddSourceFile(const char* filename){
		// If it's already added, return the old one
		SourceFile** it;
		Vector_ForEach(me->sourceFiles, it){
			if(!strcmp((*it)->filename, filename))
				return *it;
		}

		// Create a new SourceFile structure and read the 
		// entire file's contents into it

		SourceFile* sf = calloc(1, sizeof(SourceFile));

		sf->filename = strdup(filename);

		Vector_Init(sf->file, char);
		Vector_Init(sf->lineIndices, int);

		FILE* f = fopen(filename, "r");
		LAssert(f, "could not locate source file: %s", filename);

		bool done = false;
		int lineIndex = 0;
		int lineLength = 0;

		while(!done){
			while(!done){
				int c = fgetc(f);
				if(c == EOF){
					done = true;
					break;
				}
				
				lineLength++;

				if(c == '\n' || c == '\r'){
					Vector_Add(sf->file, '\0');
					break;
				}

				Vector_Add(sf->file, c);
			}

			Vector_Add(sf->lineIndices, lineIndex);
			lineIndex += lineLength;
			lineLength = 0;
		}

		fclose(f);

		Vector_Add(me->sourceFiles, sf);

		LogI("Loaded source file from: %s", filename);
		return sf;
	}

	while(true){
		char buffer[2048];
		char sourcename[512];
		if(fgets(buffer, sizeof(buffer), f) == NULL) break;

		unsigned int addr, length, line;
		char t[32][512];

		// Because fuck it
		int ret = sscanf(buffer, 
			"%x %x %u %s"
			"%s %s %s %s %s %s %s %s" "%s %s %s %s %s %s %s %s"
			"%s %s %s %s %s %s %s %s" "%s %s %s %s %s %s %s %s",
			&addr, &length, &line, sourcename,
			t[ 0], t[ 1], t[ 2], t[ 3], t[ 4], t[ 5], t[ 6], t[ 7], 
			t[ 8], t[ 9], t[10], t[11], t[12], t[13], t[14], t[15],
			t[16], t[17], t[18], t[19], t[20], t[21], t[22], t[23],
			t[24], t[25], t[26], t[27], t[28], t[29], t[30], t[31]
			);

		LAssert(ret >= 4, "could not parse debug file line in %s", filename);

		//LogI("line: %d", line);
		DebugSymbol s = {addr, length, line, AddSourceFile(sourcename)};

		Vector_Init(s.items, CharPtr);
		
		for(int i = 0; i < ret - 4; i++){
			Vector_Add(s.items, strdup(t[i]));
		}

		Vector_Add(me->debugSymbols, s);

		//puts(buffer);
	}

	/*SourceFile** it;
	Vector_ForEach(me->sourceFiles, it){
		LogI("file: %s", (*it)->filename);
		int* it2;
		Vector_ForEach((*it)->lineIndices, it2){
			puts((*it)->file.elems + *it2);
		}
	}*/

	fclose(f);

	LogI("loaded debug symbols from %s", filename);

	return true;
}

void Debug_Destroy(Debug** me)
{
	free(*me);
	*me = NULL;
}
