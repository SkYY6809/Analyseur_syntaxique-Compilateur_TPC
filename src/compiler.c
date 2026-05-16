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
    t->struct_name = NULL;
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

// Nouvelle version de add avec struct_name
int add_struct_var(Table_symb ** tab, char * type, char * struct_name, char * ident, char kind){
    if(*tab == NULL){
        *tab = makeCase(type, ident, kind);
        if (struct_name) (*tab)->struct_name = strdup(struct_name);
        return 1;
    }

    if(isInTable(ident, *tab)) return 0;

    Table_symb * cur = *tab;
    while(cur->suiv != NULL) cur = cur->suiv;
    
    cur->suiv = makeCase(type, ident, kind);
    if (struct_name) cur->suiv->struct_name = strdup(struct_name);
    return 1;
}

// Garder l'ancienne fonction pour les types simples
int add(Table_symb ** tab, char * type, char * ident, char kind){
    return add_struct_var(tab, type, NULL, ident, kind);
}


void printT(Table_symb * t){
    for(; t; t = t->suiv) {
        if (strcmp(t->type, "struct") == 0 && t->struct_name) {
            printf("Type : struct %s, Identificateur : %s, Kind : %c \n", 
                   t->struct_name, t->ident, t->kind);
        } else {
            printf("Type : %s, Identificateur : %s, Kind : %c \n", 
                   t->type, t->ident, t->kind);
        }
    }
}

void freeTable(Table_symb * t) {
    while (t != NULL) {
        Table_symb * temp = t;
        t = t->suiv;
        free(temp->type);
        free(temp->ident);
        if (temp->struct_name) free(temp->struct_name);
        free(temp);
    }
}