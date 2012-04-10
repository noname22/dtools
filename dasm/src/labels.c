#include "dasmi.h"

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

void Labels_Define(Labels* lme, Dasm* me, const char* label, uint16_t address, const char* filename, int lineNumber)
{
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

uint16_t Labels_Get(Labels* me, const char* label, uint16_t current, const char* filename, int lineNumber)
{
	Label* l = Labels_Lookup(me, label);
	if(!l) l = Labels_Add(me, label);

	LabelRef ref;

	ref.lineNumber = lineNumber;
	ref.filename = strdup(filename);
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
				LogI("  %s:%d", ref->filename, ref->lineNumber);
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


