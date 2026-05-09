#include "compiler.h"

Table_symb * makeCase(char * type, char * ident, char kind){
    Table_symb * t = malloc(sizeof(Table_symb));
    if(!t){
        printf("Run out of memory\n");
        exit(1);
    }
    t->type = strdup(type);
    t->ident = strdup(ident);
    t->offset = 0 ;
    t->kind = kind;
    t->suiv = NULL;
    return t;
}

int isInTable(char * ident, Table_symb * t){
    for(;t; t = t->suiv){
        if(strcmp(t->ident, ident) == 0)
            return 1;
    }
    return 0;
}

int isInTableWithKind(char * ident, Table_symb * t, char kind){
    for(;t; t = t->suiv){
        if(strcmp(t->ident, ident) == 0 && t->kind == kind)
            return 1;
    }
    return 0;
}

int add(Table_symb ** tab, char * type, char * ident, char kind){
    if(*tab == NULL){
        *tab = makeCase(type, ident, kind);
        return 1;
    }

    if(isInTable(ident, *tab)) return 0;

    Table_symb * cur = *tab;
    while(cur->suiv != NULL) cur = cur->suiv;
    
    cur->suiv = makeCase(type, ident, kind);
    return 1;
}


void printT(Table_symb * t){
    for(;t ;t = t->suiv)
        printf("Type : %s, Identificateur : %s, Kind : %c \n", t->type, t->ident, t->kind);
}

void freeTable(Table_symb * t) {
    while (t != NULL) {
        Table_symb * temp = t;
        t = t->suiv;
        free(temp->type);
        free(temp->ident);
        free(temp);
    }
}