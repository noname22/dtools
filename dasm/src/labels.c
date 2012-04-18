#include "dasmi.h"

#define REL "rel:"

bool IsRelative(const char* s)
{
	char tmp[strlen(REL) + 1];
	tmp[0] = 0;
	strncat(tmp, s, strlen(REL));
	for(int i = 0; i < strlen(REL); i++) tmp[i] = tolower(tmp[i]);
	return !strncmp(tmp, REL, strlen(REL));
}

const char* GetName(const char* s)
{
	if(IsRelative(s)) return s + strlen(REL);
	return s;
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
		if(!strcmp(it->label, GetName(label))) return it;
	}

	return NULL;
}

Label* Labels_Add(Labels* me, const char* label)
{
	label = GetName(label);
	
	// XXX: Why do I have two duplicate lable checks?
	LAssert(Labels_Lookup(me, label) == NULL, "duplicate label: %s", label);

	Label l;
	memset(&l, 0, sizeof(Label));
	l.label = strdup(label);
	l.id = me->count;
	Vector_Init(l.references, LabelRef);

	Vector_Add(*me, l);

	return &me->elems[me->count - 1];
}

void Labels_Define(Labels* lme, Dasm* me, const char* label, uint16_t address, const char* filename, int lineNumber)
{
	label = GetName(label);

	Label* l = Labels_Lookup(lme, label);
	if(!l) l = Labels_Add(lme, label);
	else 
		LAssertError(!l->found, 
			"duplicate label: %s, first defined at %s:%d", 
			label, l->filename, l->lineNumber);

	l->addr = address;
	l->found = true;
	l->filename = strdup(filename);
	l->lineNumber = lineNumber;
} 

uint16_t Labels_Get(Labels* me, const char* label, uint16_t current, uint16_t insAddr, const char* filename, int lineNumber)
{
	bool isRelative = IsRelative(label);
	label = GetName(label);

	Label* l = Labels_Lookup(me, label);
	if(!l) l = Labels_Add(me, label);

	LabelRef ref;

	ref.lineNumber = lineNumber;
	ref.filename = strdup(filename);
	ref.addr = current;
	ref.insAddr = insAddr;
	ref.relative = isRelative;

	Vector_Add(l->references, ref);

	return l->id;
}
	
void Labels_Replace(Labels* me, uint16_t* ram)
{
	LogD("replacing labels");

	LogD("label count: %d", me->count);

	Label* l;
	Vector_ForEach(*me, l){
		if(!l->found){
			LogF("No such label: %s", l->label);
			LogI("Referenced from:");

			LabelRef* ref;
			Vector_ForEach(l->references, ref){
				LogI("  %s:%d", ref->filename, ref->lineNumber);
			}
			exit(1);
		}
		LogD("label: %s", l->label);

		LabelRef* ref;
		Vector_ForEach(l->references, ref){
			const char* relname[] = {"absolute", "relative"};
			uint16_t addr = ref->relative ? -(ref->addr - l->addr) - 1: l->addr;
			ram[ref->addr] = addr;
			LogD("replaced label %s @ 0x%04x with %s address 0x%04x", 
				l->label, ref->addr, relname[ref->relative], addr);
		}
	}
}


