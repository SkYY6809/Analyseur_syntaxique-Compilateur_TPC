#ifndef SEMANTIQUE_H
#define SEMANTIQUE_H

#include "tree.h"
#include "compiler.h"

/* Fonction principale à appeler depuis le main */
void analyse_semantique(Node * node, Table_symb ** tableCourante);

#endif