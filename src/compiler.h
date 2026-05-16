#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Case {
    char *type;
    char *ident;
    struct Case *suiv;
    int offset; // pour la gestion de la pile 
    char kind; // 'v' = variable, 'f' = fonction
    char * struct_name; // si type == struct, nom de la structure sinon null
} Table_symb;

Table_symb * makeCase(char * type, char * ident, char kind);

int isInTable(char * ident, Table_symb * t);

int isInTableWithKind(char * ident, Table_symb * t, char kind);

int add(Table_symb ** tab, char * type, char * ident, char kind);

int add_struct_var(Table_symb ** tab, char * type, char * struct_name, char * ident, char kind);

void printT(Table_symb * t);

void freeTable(Table_symb * t);


#endif