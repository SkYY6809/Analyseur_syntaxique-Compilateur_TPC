#ifndef SEMANTIQUE_H
#define SEMANTIQUE_H

#include "tree.h"
#include "compiler.h"

void analyse_semantique(Node * node, Table_symb ** tableCourante, FILE * anonym);

#endif