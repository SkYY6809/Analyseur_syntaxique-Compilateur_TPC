#include <stdio.h>
#include <stdlib.h>

typedef struct Case {
    char * type;
    char * ident;
    struct Case * suiv;
} Table_symb;

Table_symb * makeCase(char * type, char * ident){
    Table_symb * t = malloc(sizeof(Table_symb));
    if(!t){
        printf("Run out of memory\n");
        exit(1);
    }
    t->type = type;
    t->ident = ident;
    t->suiv = NULL;
    return t;
}

int isInTable(char * type, char * ident, Table_symb * t){
    for(;t; t = t->suiv){
        if(t->ident == ident && t->type == type)
            return 1;
    }
    return 0;
}

int add(Table_symb * tab, Table_symb * c){
    if(!isInTable(c->type, c->ident, tab)){
        for(; tab && tab->suiv; tab = tab->suiv); //aller a la fin de la liste
        tab->suiv = c;
        return 1;
    }
    return 0;
}


void printT(Table_symb * t){
    for(;t ;t = t->suiv)
        printf("%s %s \n", t->type, t->ident);
}

int main(int argc, char * argv[]){
    Table_symb * t = makeCase("char", "a");
    add(t, makeCase("int", "b"));
    add(t, makeCase("int", "c"));
    add(t, makeCase("int", "b"));
    printf("%d \n", isInTable("int", "a", t));
    printT(t);

}