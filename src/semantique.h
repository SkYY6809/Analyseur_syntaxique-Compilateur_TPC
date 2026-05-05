#ifndef SEMANTIQUE_H
#define SEMANTIQUE_H

#include "tree.h"
#include "compiler.h"

void init_builtins(Table_symb ** tableGlobale);
int analyse_semantique(Node * node, Table_symb ** tableCourante, Table_symb ** tableGlobale, FILE * anonym, int symbol, char * currentFctType);
void generer_footer_asm(FILE * anonym) ;
int haveCorrectMain(Table_symb ** tableCourant);
void generer_bss(FILE * anonym, Table_symb * tableGlobale);
#endif