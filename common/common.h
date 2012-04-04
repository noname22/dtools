#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "dcpu16ins.h"
#include "log.h"
#include "cvector.h"

extern int logLevel;

void DumpRam(uint16_t* ram);
void WriteRam(uint16_t* ram, const char* filename);
void LoadRam(uint16_t* ram, const char* filename);
bool opHasNextWord(uint16_t v);

#endif
