#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantique.h"

static int need_putchar = 0;
static int need_putint  = 0;
static int need_getchar = 0;
static int need_getint  = 0;
static int global_offset = 0; // pour la gestion de la pile

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

void init_builtins(Table_symb ** tableGlobale) {
    add(tableGlobale, "int",  "getint");
    add(tableGlobale, "void", "putint");
    add(tableGlobale, "int",  "getchar");
    add(tableGlobale, "void", "putchar");
}

void translate_to_asm(Node * node, FILE * anonym){  
        if(!node) return;
        switch (node->label){
            case L_IDENT :{
                fprintf(anonym, "\tpush qword [%s]\n", node->value);
                break;
            }
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

            case L_LT:
            case L_GT:
            case L_EQ:
            case L_NEQ:
            case L_LE:
            case L_GE: {               
                Node * left  = node->firstChild;
                Node * right = left->nextSibling;
                char * set = NULL;

                switch (node->label)
                {
                    case L_LT:  set = "l";  break; // Less Than
                    case L_GT:  set = "g";  break; // Greater Than
                    case L_EQ:  set = "e";  break; // Equal
                    case L_NEQ: set = "ne"; break; // Not Equal
                    case L_LE:  set = "le"; break; // Less or Equal
                    case L_GE:  set = "ge"; break; // Greater or Equal
                    default:    set = "e";  break; // Par sécurité
                }

                // Genère le code pour évaluer les deux opérandes
                translate_to_asm(left, anonym);
                translate_to_asm(right, anonym);

                // Récupère les valeurs, compare et stocke le résultat (0 ou 1)
                fprintf(anonym, "\tpop rbx\n");
                fprintf(anonym, "\tpop rax\n");
                fprintf(anonym, "\tcmp rax, rbx\n");
                fprintf(anonym, "\tset%s al\n", set);       // Exemple : setl al
                fprintf(anonym, "\tmovzx rax, al\n");       // Nettoie rax et garde le résultat
                fprintf(anonym, "\tpush rax\n");
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
            for (Table_symb *t = tableCourante; t; t = t->suiv)
                if (strcmp(t->ident, node->value) == 0) return t->type;
            for (Table_symb *t = tableGlobale; t; t = t->suiv)
                if (strcmp(t->ident, node->value) == 0) return t->type;
            return NULL;
        }
 
        case L_CALL: {
            const char *fname = node->firstChild->value;
            for (Table_symb *t = tableCourante; t; t = t->suiv)
                if (strcmp(t->ident, fname) == 0) return t->type;
            for (Table_symb *t = tableGlobale; t; t = t->suiv)
                if (strcmp(t->ident, fname) == 0) return t->type;
            return NULL;
        }
 
        case L_ADD: case L_SUB: case L_MUL: case L_DIV: case L_MOD:
        case L_NEG: {
            char *left  = getExprType(node->firstChild, tableCourante, tableGlobale);
            char *right = node->firstChild
                          ? getExprType(node->firstChild->nextSibling, tableCourante, tableGlobale)
                          : NULL;
            if (left && right) {
                if (strcmp(left, "int") == 0 || strcmp(right, "int") == 0)
                    return "int";
                return left; 
            }
            return left ? left : right;
        }
 
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
                } else {
                    if (tableGlobale == NULL || *tableGlobale == NULL) {
                        int taille = (strcmp(typeStr, "char") == 0) ? 1 : 8;
                        Table_symb *t = *tableCourante;
                        while (t->suiv != NULL) t = t->suiv; // aller au dernier
                        t->offset = global_offset;
                        global_offset += taille;
                    }
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

            Node * param = params->firstChild;
            while(param != NULL) {
                Node * typeParam = param->firstChild;
                Node * nomParam = typeParam->nextSibling;
                add(&tableLocale, getTypeString(typeParam), nomParam->value);
                param = param->nextSibling;
            }

            analyse_semantique(corps, &tableLocale, tableCourante, anonym);

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

            translate_to_asm(value, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tmov [%s], rax\n", ident->value);
            
            break;
        }

        case L_CALL: {
            Node * nom = node->firstChild;

            if(!isInAnyTable(nom->value, *tableCourante, tableGlobale ? *tableGlobale : NULL)){
                fprintf(stderr, "Erreur sémantique ligne %d: Fonction '%s' non déclarée.\n", 
                        node->lineno, nom->value);
            }

            Node * arg = nom->nextSibling;
            while (arg != NULL) {
                analyse_semantique(arg, tableCourante, tableGlobale, anonym);
                translate_to_asm(arg, anonym); 
                arg = arg->nextSibling;
            }

            if (strcmp(nom->value, "getint") == 0) {
                need_getint = 1;
                fprintf(anonym, "\tcall my_getint\n");
                fprintf(anonym, "\tpush rax\n"); 
            } else if (strcmp(nom->value, "putint") == 0) {
                need_putint = 1;
                fprintf(anonym, "\tcall my_putint\n");
                fprintf(anonym, "\tadd rsp, 8\n"); 
            } else if (strcmp(nom->value, "putchar") == 0) {
                need_putchar = 1;
                fprintf(anonym, "\tcall my_putchar\n");
            } else {
                fprintf(anonym, "\tcall %s\n", nom->value);
            }
            break;
        }

        case L_IF: {
            Node * cond = node->firstChild;
            Node * corps = cond->nextSibling;

            static int label_count = 0;
            int lbl = label_count++;

            // Évaluer la condition
            translate_to_asm(cond, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
            fprintf(anonym, "\tje .fin_if_%d\n", lbl);   // si faux → sauter

            // Corps du if
            analyse_semantique(corps, tableCourante, tableGlobale, anonym);

            fprintf(anonym, ".fin_if_%d:\n", lbl);
            break;
        }

        case L_IF_ELSE: {
            Node * cond  = node->firstChild;
            Node * corps_if   = cond->nextSibling;
            Node * corps_else = corps_if->nextSibling;

            static int label_count = 0;
            int lbl = label_count++;

            // Évaluer la condition
            translate_to_asm(cond, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
            fprintf(anonym, "\tje .sinon_%d\n", lbl);    // si faux → aller au else

            // Corps du if
            analyse_semantique(corps_if, tableCourante, tableGlobale, anonym);
            fprintf(anonym, "\tjmp .fin_si_%d\n", lbl);  // sauter le else

            // Corps du else
            fprintf(anonym, ".sinon_%d:\n", lbl);
            analyse_semantique(corps_else, tableCourante, tableGlobale, anonym);

            fprintf(anonym, ".fin_si_%d:\n", lbl);
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

void generer_footer_asm(FILE * anonym) {

        if (need_putint) {
        fprintf(anonym, "\nmy_putint:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tsub rsp, 32\n");
        fprintf(anonym, "\tpush rbx\n");

        fprintf(anonym, "\tmov rax, [rbp+16]\n");

        fprintf(anonym, "\tcmp rax, 0\n");
        fprintf(anonym, "\tjne .pi_nonzero\n");
        fprintf(anonym, "\tmov byte [rbp-32], '0'\n");
        fprintf(anonym, "\tmov rax, 1\n");
        fprintf(anonym, "\tmov rdi, 1\n");
        fprintf(anonym, "\tlea rsi, [rbp-32]\n");
        fprintf(anonym, "\tmov rdx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tjmp .pi_done\n");

        fprintf(anonym, ".pi_nonzero:\n");
        fprintf(anonym, "\txor r8, r8\n");
        fprintf(anonym, "\tcmp rax, 0\n");
        fprintf(anonym, "\tjge .pi_pos\n");
        fprintf(anonym, "\tmov r8, 1\n");
        fprintf(anonym, "\tneg rax\n");
        fprintf(anonym, ".pi_pos:\n");
        fprintf(anonym, "\tlea r9, [rbp-1]\n");  
        fprintf(anonym, "\tmov rcx, 0\n");
        fprintf(anonym, "\tmov rbx, 10\n");

        fprintf(anonym, ".pi_loop:\n");
        fprintf(anonym, "\tcmp rax, 0\n");
        fprintf(anonym, "\tje .pi_write\n");
        fprintf(anonym, "\txor rdx, rdx\n");
        fprintf(anonym, "\tdiv rbx\n");
        fprintf(anonym, "\tadd dl, '0'\n");
        fprintf(anonym, "\tmov [r9], dl\n");
        fprintf(anonym, "\tdec r9\n");
        fprintf(anonym, "\tinc rcx\n");
        fprintf(anonym, "\tjmp .pi_loop\n");

        fprintf(anonym, ".pi_write:\n");
        fprintf(anonym, "\tcmp r8, 0\n");
        fprintf(anonym, "\tje .pi_nosign\n");
        fprintf(anonym, "\tmov byte [r9], '-'\n");
        fprintf(anonym, "\tdec r9\n");
        fprintf(anonym, "\tinc rcx\n");
        fprintf(anonym, ".pi_nosign:\n");
        fprintf(anonym, "\tinc r9\n");            

        fprintf(anonym, "\tmov rax, 1\n");
        fprintf(anonym, "\tmov rsi, r9\n");       
        fprintf(anonym, "\tmov rdi, 1\n");
        fprintf(anonym, "\tmov rdx, rcx\n");
        fprintf(anonym, "\tsyscall\n");

        fprintf(anonym, ".pi_done:\n");
        fprintf(anonym, "\tpop rbx\n");
        fprintf(anonym, "\tmov rsp, rbp\n");
        fprintf(anonym, "\tpop rbp\n");
        fprintf(anonym, "\tret\n");
    }

    if (need_putchar) {
        fprintf(anonym, "\nmy_putchar:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tmov rax, [rbp+16] \n"); 
        
        fprintf(anonym, "\tmov [rbp-1], al      ; On met le char dans un petit coin de la pile\n");
        fprintf(anonym, "\tmov rax, 1           ; syscall: write\n");
        fprintf(anonym, "\tmov rdi, 1           ; file descriptor: stdout\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]     ; l'adresse de notre char\n");
        fprintf(anonym, "\tmov rdx, 1   \n");
        fprintf(anonym, "\tsyscall\n");
        
        fprintf(anonym, "\tmov rsp, rbp\n");
        fprintf(anonym, "\tpop rbp\n");
        fprintf(anonym, "\tret\n");
    }

    
    if (need_getint) {
        fprintf(anonym, "\nmy_getint:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tsub rsp, 16        \n");
        fprintf(anonym, "\txor r12, r12       \n");

        fprintf(anonym, "\tmov rax, 0        \n");
        fprintf(anonym, "\tmov rdi, 0         \n");
        fprintf(anonym, "\tlea rsi, [rbp-1]    \n");
        fprintf(anonym, "\tmov rdx, 1\n");
        fprintf(anonym, "\tsyscall\n");

        fprintf(anonym, "\tmovzx rbx, byte [rbp-1]\n");
        fprintf(anonym, "\tcmp rbx, '0'\n");
        fprintf(anonym, "\tjl .error           \n");
        fprintf(anonym, "\tcmp rbx, '9'\n");
        fprintf(anonym, "\tjg .error            \n");

        fprintf(anonym, "\n.loop:\n");
        fprintf(anonym, "\tsub rbx, '0'\n");
        fprintf(anonym, "\timul r12, 10      \n");
        fprintf(anonym, "\tadd r12, rbx        \n");

        fprintf(anonym, "\tmov rax, 0\n");
        fprintf(anonym, "\tmov rdi, 0\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov rdx, 1\n");
        fprintf(anonym, "\tsyscall\n");

        fprintf(anonym, "\n\tmovzx rbx, byte [rbp-1]\n");
        fprintf(anonym, "\tcmp rbx, '0'\n");
        fprintf(anonym, "\tjl .done\n");
        fprintf(anonym, "\tcmp rbx, '9'\n");
        fprintf(anonym, "\tjg .done\n");
        fprintf(anonym, "\tjmp .loop\n");

        fprintf(anonym, "\n.done:\n");
        fprintf(anonym, "\tmov rax, r12        \n");
        fprintf(anonym, "\tmov rsp, rbp\n");
        fprintf(anonym, "\tpop rbp\n");
        fprintf(anonym, "\tret\n");

        fprintf(anonym, "\n.error:\n");
        fprintf(anonym, "\tmov rax, 60          \n");
        fprintf(anonym, "\tmov rdi, 5           \n");
        fprintf(anonym, "\tsyscall\n");
    }
}
void generer_bss(FILE * anonym, Table_symb * tableGlobale) {
    fprintf(anonym, "\nsection .bss\n");
    for (Table_symb * t = tableGlobale; t; t = t->suiv) {
        if (strcmp(t->type, "int") == 0 && strcmp(t->ident, "getint") != 0 && strcmp(t->ident, "putint") != 0 && strcmp(t->ident, "putchar") != 0) {
            fprintf(anonym, "%s resq 1\n", t->ident);  
        } else if (strcmp(t->type, "char") == 0 && strcmp(t->ident, "getint") != 0 && strcmp(t->ident, "putint") != 0 && strcmp(t->ident, "putchar") != 0) {
            fprintf(anonym, "%s resb 1\n", t->ident); 
        }
    }
}