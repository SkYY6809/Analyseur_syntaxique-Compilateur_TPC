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
char* labelToString(label_t label) {
    switch(label) {
        case L_PROG: return "L_PROG";
        case L_DECL_STRUCT: return "L_DECL_STRUCT";
        case L_DECL_VAR: return "L_DECL_VAR";
        case L_DECL_FONCT: return "L_DECL_FONCT";
        case L_CHAMPS: return "L_CHAMPS";
        case L_CHAMP: return "L_CHAMP";

        case L_TYPE_INT: return "L_TYPE_INT";
        case L_TYPE_VOID: return "L_TYPE_VOID";
        case L_TYPE_CHAR: return "L_TYPE_CHAR";
        case L_TYPE_STRUCT: return "L_TYPE_STRUCT";

        case L_ENTETE_FONCT: return "L_ENTETE_FONCT";
        case L_PARAMETRES: return "L_PARAMETRES";
        case L_PARAM: return "L_PARAM";

        case L_CORPS: return "L_CORPS";
        case L_ASSIGN: return "L_ASSIGN";
        case L_FIELD_ASSIGN: return "L_FIELD_ASSIGN";
        case L_IF: return "L_IF";
        case L_IF_ELSE: return "L_IF_ELSE";
        case L_WHILE: return "L_WHILE";
        case L_RETURN: return "L_RETURN";
        case L_CALL: return "L_CALL";
        case L_BLOCK: return "L_BLOCK";

        case L_OR: return "L_OR";
        case L_AND: return "L_AND";
        case L_EQ: return "L_EQ";
        case L_NEQ: return "L_NEQ";
        case L_LT: return "L_LT";
        case L_GT: return "L_GT";
        case L_LE: return "L_LE";
        case L_GE: return "L_GE";
        case L_ADD: return "L_ADD";
        case L_SUB: return "L_SUB";
        case L_MUL: return "L_MUL";
        case L_DIV: return "L_DIV";
        case L_MOD: return "L_MOD";
        case L_NOT: return "L_NOT";
        case L_NEG: return "L_NEG";

        case L_IDENT: return "L_IDENT";
        case L_NUM: return "L_NUM";
        case L_CHAR: return "L_CHAR";
        case L_FIELD_ACCESS: return "L_FIELD_ACCESS";

        case L_DECLARATEURS: return "L_DECLARATEURS";
        case L_ARGUMENTS: return "L_ARGUMENTS";

        default: return "UNKNOWN";
    }
}

static int isInAnyTable(char * ident, Table_symb * locale, Table_symb * globale) {
    return isInTable(ident, locale) || isInTable(ident, globale);
}

void warningType(char * type_ident, char * type_value){
    if(strcmp(type_ident, "int") == 0 && strcmp(type_value, "char") == 0)
        printf("Warning : Vous assegnez un char à un int");
}


//passer par les label et pas par les les values belek sa fonctionnera
int isSameType(Node * ident, Node * value, Table_symb * tableCourante, Table_symb * tableGlobale){
    char * type_ident; 
    char * type_value;
    
    if(value->label != L_CALL) {
        //si valeur n'est pas une fonction
        
        //parcours sur tableCourante
        for(;tableCourante; tableCourante= tableCourante->suiv){
            //verif pour ident
            if(strcmp(tableCourante->ident, ident->value) == 0)
                type_ident = tableCourante->type;
                //verif pour value
            if(strcmp(tableCourante->ident, value->value) == 0)
                type_value = tableCourante->type;
        }
    
        //parcours sur tableGlobale
        for(;tableGlobale; tableGlobale= tableGlobale->suiv){
            //verif pour ident
            if(strcmp(tableGlobale->ident, ident->value) == 0)
                type_ident= tableGlobale->type;
            //verif pour value
            if(strcmp(tableGlobale->ident, value->value) == 0)
                type_value = tableGlobale->type;
        }
    }
    else{
        //si valeur est une fonction

        //parcours tableCourante    return (strcmp(type_ident, type_value)==0 || (strcmp(type_ident, "int") == 0 && strcmp(type_value, "char") == 0));
        for(;tableCourante; tableCourante= tableCourante->suiv){
            //verif pour ident
            if(strcmp(tableCourante->ident, ident->value) == 0)
                type_ident = tableCourante->type;
            //verif pour value
            if(strcmp(tableCourante->ident, value->firstChild->value) == 0)
                type_value = tableCourante->type;
        }
    
        //parcours tableGlobal
        for(;tableGlobale; tableGlobale= tableGlobale->suiv){
            //verif pour ident
            if(strcmp(tableGlobale->ident, ident->value) == 0)
                type_ident= tableGlobale->type;
            //verif pour value
            if(strcmp(tableGlobale->ident, value->firstChild->value) == 0)
                type_value = tableGlobale->type;
        }
    }
    printf("SCOOBYDOO BY DOOO %s AHHHHHHHHHHHHHHHH %s", type_ident, type_value);
    warningType(type_ident, type_value);
    return (strcmp(type_ident, type_value)==0 || (strcmp(type_ident, "int") == 0 && strcmp(type_value, "char") == 0));
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