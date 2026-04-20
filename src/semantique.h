#ifndef SEMANTIQUE_H
#define SEMANTIQUE_H

#include "tree.h"
#include "compiler.h"

void init_builtins(Table_symb ** tableGlobale);
void analyse_semantique(Node * node, Table_symb ** tableCourante, Table_symb ** tableGlobale, FILE * anonym);

#endif