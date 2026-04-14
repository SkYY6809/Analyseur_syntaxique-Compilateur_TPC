#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantique.h"

char* getTypeString(Node* typeNode) {
    if(!typeNode) return "unknown";
    switch(typeNode->label) {
        case L_TYPE_INT: return "int";
        case L_TYPE_CHAR: return "char";
        case L_TYPE_VOID: return "void";
        case L_TYPE_STRUCT: return "struct"; 
        default: return "unknown";
    }
}

void translate_to_asm(Node * node, FILE * anonym){  
        if(!node) return;
        switch (node->label){
            case L_NUM :{
                fprintf(anonym, "\tpush %s\n", node->value);
                break;
            }
            case L_SUB:{
                Node * left = node->firstChild;
                Node * right = left->nextSibling;
                translate_to_asm(left, anonym);
                translate_to_asm(right, anonym);
                fprintf(anonym, "\tpop rbx\n");
                fprintf(anonym, "\tpop rax\n");
                fprintf(anonym, "\tsub rax, rbx\n");
                fprintf(anonym,"\tpush rax\n");
                break;
            }
            case L_ADD:{
                Node * left = node->firstChild;
                Node * right = left->nextSibling;
                translate_to_asm(left, anonym);
                translate_to_asm(right, anonym);
                fprintf(anonym, "\tpop rbx\n");
                fprintf(anonym, "\tpop rax\n");
                fprintf(anonym, "\tadd rax, rbx\n");
                fprintf(anonym,"\tpush rax\n");
                break;
            }
            case L_MUL:{
                Node * left = node->firstChild;
                Node * right = left->nextSibling;
                translate_to_asm(left, anonym);
                translate_to_asm(right, anonym);
                fprintf(anonym, "\tpop rbx\n");
                fprintf(anonym, "\tpop rax\n");
                fprintf(anonym, "\timul rax, rbx\n");
                fprintf(anonym,"\tpush rax\n");
                break;
            }
            default:{
                Node * child = node->firstChild;
                while(child){
                    translate_to_asm(child, anonym);
                    child = child->nextSibling;
                }
                break;
            }
        }
}

static int isInAnyTable(char * ident, Table_symb * locale, Table_symb * globale) {
    return isInTable(ident, locale) || isInTable(ident, globale);
}

void warningType(char * type_ident, char * type_value, int ligne){
    if(strcmp(type_ident, "int") == 0 && strcmp(type_value, "char") == 0)
        printf("Warning ligne %d : Vous assegnez un char à un int\n", ligne);
}

static char* getExprType(Node * node, Table_symb * tableCourante, Table_symb * tableGlobale) {
    if (!node) return NULL;
 
    switch (node->label) {
        case L_NUM:
            return "int";
        case L_CHAR:
            return "char";
 
        case L_IDENT: {
            /* cherche en local d'abord, puis global */
            for (Table_symb *t = tableCourante; t; t = t->suiv)
                if (strcmp(t->ident, node->value) == 0) return t->type;
            for (Table_symb *t = tableGlobale; t; t = t->suiv)
                if (strcmp(t->ident, node->value) == 0) return t->type;
            return NULL;
        }
 
        case L_CALL: {
            /* type de retour = type enregistré dans la table pour le nom de la fonction */
            const char *fname = node->firstChild->value;
            for (Table_symb *t = tableCourante; t; t = t->suiv)
                if (strcmp(t->ident, fname) == 0) return t->type;
            for (Table_symb *t = tableGlobale; t; t = t->suiv)
                if (strcmp(t->ident, fname) == 0) return t->type;
            return NULL;
        }
 
        /* Opérations arithmétiques : le type dominant est int,
           sauf si les deux opérandes sont char → résultat char */
        case L_ADD: case L_SUB: case L_MUL: case L_DIV: case L_MOD:
        case L_NEG: {
            char *left  = getExprType(node->firstChild, tableCourante, tableGlobale);
            char *right = node->firstChild
                          ? getExprType(node->firstChild->nextSibling, tableCourante, tableGlobale)
                          : NULL;
            if (left && right) {
                if (strcmp(left, "int") == 0 || strcmp(right, "int") == 0)
                    return "int";
                return left; /* les deux sont char */
            }
            return left ? left : right;
        }
 
        /* Opérations logiques/comparaisons → résultat entier (0 ou 1) */
        case L_OR: case L_AND: case L_NOT:
        case L_EQ: case L_NEQ:
        case L_LT: case L_GT: case L_LE: case L_GE:
            return "int";
 
        default:
            return NULL;
    }
}

int isSameType(Node * ident, Node * value, Table_symb * tableCourante, Table_symb * tableGlobale){
    char * type_ident = NULL;
 
    // Cherche le type de la variable à gauche du =
    for (Table_symb * t = tableCourante; t; t = t->suiv)
        if (strcmp(t->ident, ident->value) == 0) { type_ident = t->type; break; }
    if (!type_ident)
        for (Table_symb * t = tableGlobale; t; t = t->suiv)
            if (strcmp(t->ident, ident->value) == 0) { type_ident = t->type; break; }
 
    // Déduit le type de l'expression à droite du =
    char * type_value = getExprType(value, tableCourante, tableGlobale);
 
    warningType(type_ident, type_value, value->lineno);
    return (strcmp(type_ident, type_value) == 0
            || (strcmp(type_ident, "int") == 0 && strcmp(type_value, "char") == 0));
}

void analyse_semantique(Node * node, Table_symb ** tableCourante, Table_symb ** tableGlobale, FILE * anonym) {
    if (node == NULL) return;

    switch(node->label) {

        case L_DECL_VAR: {
            Node *typeNode = node->firstChild;
            if (!typeNode) break;

            char *typeStr = getTypeString(typeNode);
            
            Node *varNode = typeNode->nextSibling;
            while (varNode != NULL) {
                if (add(tableCourante, typeStr, varNode->value) == 0) {
                    fprintf(stderr, "Erreur sémantique ligne %d: Variable '%s' déjà déclarée.\n", 
                            node->lineno, varNode->value);
                }
                varNode = varNode->nextSibling;
            }
            break; 
        }

        case L_DECL_FONCT: {
            Table_symb * tableLocale = NULL;

            Node * entete = node->firstChild; 
            Node * corps = entete->nextSibling;

            Node * typeRetour = entete->firstChild;
            Node * nomFonct = typeRetour->nextSibling;
            Node * params = nomFonct->nextSibling;

            if(isInTable(nomFonct->value, *tableCourante)){
                fprintf(stderr, "Erreur sémantique ligne %d: Fonction '%s' déjà déclarée.\n", 
                            node->lineno, nomFonct->value);
            }
            else{
                add(tableCourante, getTypeString(typeRetour), nomFonct->value);
            }

            if (strcmp(nomFonct->value, "main") == 0){
                fprintf(anonym, "global _start\n");
                fprintf(anonym, "section .text\n");
                fprintf(anonym, "_start:\n");
            }
            printf("\n>>> Analyse de la fonction : %s\n", nomFonct->value);

            // parametres
            Node * param = params->firstChild;
            while(param != NULL) {
                Node * typeParam = param->firstChild;
                Node * nomParam = typeParam->nextSibling;
                add(&tableLocale, getTypeString(typeParam), nomParam->value);
                param = param->nextSibling;
            }

            analyse_semantique(corps, &tableLocale, tableCourante, anonym);
            translate_to_asm(corps, anonym);

            if (strcmp(nomFonct->value, "main") == 0){
                fprintf(anonym, "\tmov rax, 60\n");
                fprintf(anonym, "\tmov rdi, 0\n");
                fprintf(anonym, "\tsyscall\n");
            }

            printf("--- Table des symboles (Locals + Params) pour '%s' ---\n", nomFonct->value);
            printT(tableLocale);
            
            freeTable(tableLocale); 
            break;
        }

        case L_ASSIGN: {
            Node * ident = node->firstChild;
            Node * value = ident->nextSibling;
            if(!isInAnyTable(ident->value, *tableCourante, tableGlobale ? *tableGlobale : NULL)){
                fprintf(stderr, "Erreur sémantique ligne %d: Variable '%s' non déclarée.\n", 
                            node->lineno, ident->value);
            }
            if(value->label == L_CALL && !isInAnyTable(value->firstChild->value, *tableCourante, tableGlobale ? *tableGlobale : NULL))
                fprintf(stderr, "Erreur sémantique ligne %d: Fonction '%s' non déclarée.\n", 
                            node->lineno, value->firstChild->value);
            
            if(!isSameType(ident, value, *tableCourante, *tableGlobale))
                fprintf(stderr, "Erreur sémantique ligne %d: Pas le même type.\n", 
                            node->lineno);
            
            break;
        }

        case L_CALL: {
            Node * nom = node->firstChild;
           if(!isInAnyTable(nom->value, *tableCourante, tableGlobale ? *tableGlobale : NULL)){
                fprintf(stderr, "Erreur sémantique ligne %d: Fonction '%s' non déclarée.\n", 
                            node->lineno, nom->value);
            }
            Node * child = nom->firstChild;
            while (child != NULL) {
                analyse_semantique(child, tableCourante, tableGlobale, anonym);
                child = child->nextSibling;
            }
            break;
        }

        default: {
            Node * child = node->firstChild;
            while (child != NULL) {
                analyse_semantique(child, tableCourante, tableGlobale, anonym);
                child = child->nextSibling;
            }
            break;
        }
    }
}