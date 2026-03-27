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

void analyse_semantique(Node * node, Table_symb ** tableCourante, FILE * anonym) {
    if (node == NULL) return;
    Table_symb * tableLocale = NULL;

    switch(node->label) {

        case L_DECL_VAR: {

            Node *typeNode = node->firstChild;
            if (!typeNode) break;

            char *typeStr = getTypeString(typeNode);
            
            Node *varNode = typeNode->nextSibling;
            while (varNode != NULL) {

                if (add(&tableLocale, typeStr, varNode->value) == 0) {
                    fprintf(stderr, "Erreur sémantique ligne %d: Variable '%s' déjà déclarée.\n", 
                            node->lineno, varNode->value);
                }
                varNode = varNode->nextSibling;
            }
            break; 
        }

        case L_DECL_FONCT: {

            Node * entete = node->firstChild; 
            Node * corps = entete->nextSibling;

            Node * typeRetour = entete->firstChild;
            Node * nomFonct = typeRetour->nextSibling;
            Node * params = nomFonct->nextSibling;

            if(isInTable(nomFonct->value, *tableCourante)){
                fprintf(stderr, "Erreur sémantique ligne %d: Fonction '%s' déjà déclarée.\n", 
                            node->lineno, nomFonct->value);
                //exit(EXIT_FAILURE);
                }
            else{
                add(tableCourante, getTypeString(typeRetour), nomFonct->value);
            }

            // en tete de l'assambleur si y'a un main
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

            // corps
            analyse_semantique(corps, &tableLocale, anonym);
            translate_to_asm(corps, anonym);

            // fin de m'assembleur si y'a main
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
            if(!isInTable(ident->value, tableLocale)){
                fprintf(stderr, "Erreur sémantique ligne %d: Variable '%s' non déclarée.\n", 
                            node->lineno, ident->value);
                //exit(EXIT_FAILURE);
                }
        }

        case L_CALL: {
            Node * nom = node->firstChild;
            Node * args = node->nextSibling;
            if(!isInTable(nom->value, *tableCourante)){
                fprintf(stderr, "Erreur sémantique ligne %d: Fonction '%s' non déclarée.\n", 
                            node->lineno, nom->value);
                ////exit(EXIT_FAILURE);
                }
            /*
            while(args){
                if(!isInTable(args->value, *tableCourante)){
                fprintf(stderr, "Erreur sémantique ligne %d: Variable '%s' non déclarée.\n", 
                            node->lineno, args->value);
                exit(EXIT_FAILURE);
                }
                args = args->firstChild;
            }*/
            
        }

        default: {
            Node * child = node->firstChild;
            while (child != NULL) {
                analyse_semantique(child, tableCourante, anonym);
                child = child->nextSibling;
            }
            break;
        }
    }
}
