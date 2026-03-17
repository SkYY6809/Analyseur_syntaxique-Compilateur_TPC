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
        
}

void analyse_semantique(Node * node, Table_symb ** tableCourante, FILE * anonym) {
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

                    // --- 1. PROLOGUE (Avant l'analyse du corps) ---
                    if (strcmp(nomFonct->value, "main") == 0){
                        fprintf(anonym, "global _start\n");
                        fprintf(anonym, "section .text\n");
                        fprintf(anonym, "_start:\n");
                    }
                    printf("\n>>> Analyse de la fonction : %s\n", nomFonct->value);

                    // --- 2. GESTION DES PARAMÈTRES ---
                    Node * param = params->firstChild;
                    while(param != NULL) {
                        Node * typeParam = param->firstChild;
                        Node * nomParam = typeParam->nextSibling;
                        
                        add(&tableLocale, getTypeString(typeParam), nomParam->value);
                        param = param->nextSibling;
                    }

                    // --- 3. ANALYSE DU CORPS ---
                    analyse_semantique(corps, &tableLocale, anonym);

                    // --- 4. ÉPILOGUE (Après l'analyse du corps) ---
                    if (strcmp(nomFonct->value, "main") == 0){
                        fprintf(anonym, "    mov rax, 60\n");
                        fprintf(anonym, "    mov rdi, 0\n");
                        fprintf(anonym, "    syscall\n");
                    }

                    printf("--- Table des symboles (Locals + Params) pour '%s' ---\n", nomFonct->value);
                    printT(tableLocale);
                    
                    freeTable(tableLocale); 
                    break;
                }

        case L_ASSIGN: {
            Node * child ;

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