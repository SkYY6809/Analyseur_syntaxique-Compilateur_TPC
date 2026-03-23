#include "compiler.h"

Table_symb * makeCase(char * type, char * ident){
    Table_symb * t = malloc(sizeof(Table_symb));
    if(!t){
        printf("Run out of memory\n");
        exit(1);
    }
    t->type = strdup(type);
    t->ident = strdup(ident);
    t->suiv = NULL;
    return t;
}

int isInTable(char * type, char * ident, Table_symb * t){
    for(;t; t = t->suiv){
        if(strcmp(t->ident, ident) == 0)
            return 1;
    }
    return 0;
}

int add(Table_symb ** tab, char * type, char * ident){
    if(*tab == NULL){
        *tab = makeCase(type, ident);
        return 1;
    }

    if(isInTable(ident, type, *tab)) return 0;

    Table_symb * cur = *tab;
    while(cur->suiv != NULL) cur = cur->suiv;
    
    cur->suiv = makeCase(type, ident);
    return 1;
}


void printT(Table_symb * t){
    for(;t ;t = t->suiv)
        printf("Type : %s, Identificateur : %s \n", t->type, t->ident);
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