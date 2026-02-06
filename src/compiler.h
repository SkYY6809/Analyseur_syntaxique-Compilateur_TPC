#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Case {
    char * type;
    char * ident;
    struct Case * suiv;
} Table_symb;

Table_symb * makeCase(char * type, char * ident);

int isInTable(char * type, char * ident, Table_symb * t);

int add(Table_symb ** tab, char * type, char * ident);

void printT(Table_symb * t);

void freeTable(Table_symb * t);

#endif