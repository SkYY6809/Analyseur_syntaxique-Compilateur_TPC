#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantique.h"
#include "compiler.h"

static int need_putchar = 0;
static int need_putint  = 0;
static int need_getchar = 0;
static int need_getint  = 0;
static int global_offset = 0;

/* Compteur global de labels pour éviter les collisions entre if/while/booléens */
static int label_count = 0;

char* getTypeString(Node* typeNode) {
    if(!typeNode) return "unknown";
    switch(typeNode->label) {
        case L_TYPE_INT:    return "int";
        case L_TYPE_CHAR:   return "char";
        case L_TYPE_VOID:   return "void";
        case L_TYPE_STRUCT: return "struct";
        default:            return "unknown";
    }
}

void init_builtins(Table_symb ** tableGlobale) {
    add(tableGlobale, "int",  "getint",  'f');
    add(tableGlobale, "void", "putint",  'f');
    add(tableGlobale, "int",  "getchar", 'f');
    add(tableGlobale, "void", "putchar", 'f');
}

/* =========================================================
 *  GÉNÉRATION DE CODE ASM
 * ========================================================= */

void translate_to_asm(Node * node, FILE * anonym) {
    if(!node) return;
    switch (node->label) {

        case L_IDENT: {
            fprintf(anonym, "\tpush dword [%s]\n", node->value);
            break;
        }

        case L_NUM: {
            fprintf(anonym, "\tpush %s\n", node->value);
            break;
        }

        case L_CHAR: {
            /* node->value est de la forme 'x' ou '\n' etc. — on push la valeur ASCII */
            fprintf(anonym, "\tpush %s\n", node->value);
            break;
        }

        case L_NEG: {
            translate_to_asm(node->firstChild, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tneg eax\n");
            fprintf(anonym, "\tpush eax\n");
            break;
        }

        case L_NOT: {
            translate_to_asm(node->firstChild, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tsete al\n");
            fprintf(anonym, "\tmovzx eax, al\n");
            fprintf(anonym, "\tpush eax\n");
            break;
        }

        case L_AND: {
            int lbl = label_count++;
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tje .and_false_%d\n", lbl);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tje .and_false_%d\n", lbl);
            fprintf(anonym, "\tpush 1\n");
            fprintf(anonym, "\tjmp .and_end_%d\n", lbl);
            fprintf(anonym, ".and_false_%d:\n", lbl);
            fprintf(anonym, "\tpush 0\n");
            fprintf(anonym, ".and_end_%d:\n", lbl);
            break;
        }

        case L_OR: {
            int lbl = label_count++;
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tjne .or_true_%d\n", lbl);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tjne .or_true_%d\n", lbl);
            fprintf(anonym, "\tpush 0\n");
            fprintf(anonym, "\tjmp .or_end_%d\n", lbl);
            fprintf(anonym, ".or_true_%d:\n", lbl);
            fprintf(anonym, "\tpush 1\n");
            fprintf(anonym, ".or_end_%d:\n", lbl);
            break;
        }

        case L_SUB: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop ebx\n");
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tsub eax, ebx\n");
            fprintf(anonym, "\tpush eax\n");
            break;
        }

        case L_ADD: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop ebx\n");
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tadd eax, ebx\n");
            fprintf(anonym, "\tpush eax\n");
            break;
        }

        case L_MUL: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop ebx\n");
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\timul eax, ebx\n");
            fprintf(anonym, "\tpush eax\n");
            break;
        }

        case L_DIV: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop ebx\n");
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcdq\n");         // étend eax dans edx:eax (signé)
            fprintf(anonym, "\tidiv ebx\n");
            fprintf(anonym, "\tpush eax\n");    // quotient
            break;
        }

        case L_MOD: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop ebx\n");
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcdq\n");
            fprintf(anonym, "\tidiv ebx\n");
            fprintf(anonym, "\tpush edx\n");    // reste
            break;
        }

        case L_LT: case L_GT: case L_EQ: case L_NEQ: case L_LE: case L_GE: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            const char *set = NULL;
            switch (node->label) {
                case L_LT:  set = "l";  break;
                case L_GT:  set = "g";  break;
                case L_EQ:  set = "e";  break;
                case L_NEQ: set = "ne"; break;
                case L_LE:  set = "le"; break;
                case L_GE:  set = "ge"; break;
                default:    set = "e";  break;
            }
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop ebx\n");
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, ebx\n");
            fprintf(anonym, "\tset%s al\n", set);
            fprintf(anonym, "\tmovzx eax, al\n");
            fprintf(anonym, "\tpush eax\n");
            break;
        }
        default: {
            Node *child = node->firstChild;
            while (child) {
                translate_to_asm(child, anonym);
                child = child->nextSibling;
            }
            break;
        }
    }
}

/* =========================================================
 *  HELPERS SÉMANTIQUES
 * ========================================================= */

static int isInAnyTable(char *ident, Table_symb *locale, Table_symb *globale) {
    return isInTable(ident, locale) || isInTable(ident, globale);
}

/*
 * Émet un warning si on affecte un int à un char (dans le sens int → char).
 * Selon le sujet : char → int est CORRECT et ne doit PAS provoquer d'avertissement.
 */
void warningType(char *type_dest, char *type_src, int ligne) {
    if (!type_dest || !type_src) return;
    if (strcmp(type_dest, "char") == 0 && strcmp(type_src, "int") == 0)
        fprintf(stderr, "Warning ligne %d : affectation d'un int vers un char\n", ligne);
}


/*
 * POINT 1 — Vérifie qu'une expression n'est pas un appel de fonction void.
 * Retourne 2 et imprime une erreur si c'est le cas, 0 sinon.
 */
static int checkNotVoidExpr(Node *expr, Table_symb *tableCourante, Table_symb *tableGlobale, int ligne);

/*
 * Retourne le type d'une expression, ou NULL si indéterminable.
 * Règle du sujet : toute opération sur char produit un int (conversion implicite)
 */
static char* getExprType(Node *node, Table_symb *tableCourante, Table_symb *tableGlobale) {
    if (!node) return NULL;

    switch (node->label) {
        /* Terminaux */
        case L_NUM:
            return "int";
            
        case L_CHAR:
            return "char";
            
        case L_IDENT: {
            /* Chercher d'abord dans la table locale, puis globale */
            for (Table_symb *t = tableCourante; t; t = t->suiv)
                if (strcmp(t->ident, node->value) == 0) return t->type;
            for (Table_symb *t = tableGlobale; t; t = t->suiv)
                if (strcmp(t->ident, node->value) == 0) return t->type;
            return NULL;
        }
        
        case L_CALL: {
            /* Appel de fonction : retourne le type de retour de la fonction */
            if (!node->firstChild) return NULL;
            const char *fname = node->firstChild->value;
            for (Table_symb *t = tableCourante; t; t = t->suiv)
                if (strcmp(t->ident, fname) == 0) return t->type;
            for (Table_symb *t = tableGlobale; t; t = t->suiv)
                if (strcmp(t->ident, fname) == 0) return t->type;
            return NULL;
        }
        
        /* Opérateurs binaires arithmétiques : + - * / % */
        case L_ADD:
        case L_SUB:
        case L_MUL:
        case L_DIV:
        case L_MOD: {
            Node *left = node->firstChild;
            Node *right = left ? left->nextSibling : NULL;
            
            char *leftType = left ? getExprType(left, tableCourante, tableGlobale) : NULL;
            char *rightType = right ? getExprType(right, tableCourante, tableGlobale) : NULL;
            
            /* Si l'un des opérandes est invalide */
            if (!leftType || !rightType) return NULL;
            
            /* Si l'un des opérandes est int, le résultat est int */
            if (strcmp(leftType, "int") == 0 || strcmp(rightType, "int") == 0)
                return "int";
            
            /* char op char → int (conversion implicite selon le sujet) */
            if (strcmp(leftType, "char") == 0 && strcmp(rightType, "char") == 0)
                return "int";
            
            /* Cas par défaut */
            return "int";
        }
        
        /* Opérateur unaire NEG (- expr) */
        case L_NEG: {
            /* Le résultat de -expr est toujours un int */
            return "int";
        }

        /* Opérateurs logiques et de comparaison */
        case L_NOT:
        case L_AND:
        case L_OR:
        case L_EQ:
        case L_NEQ:
        case L_LT:
        case L_GT:
        case L_LE:
        case L_GE: {
            return "int";
        }

        default:
            return NULL;
    }
}


static int checkNotVoidExpr(Node *expr, Table_symb *tableCourante, Table_symb *tableGlobale, int ligne) {
    if (!expr) return 0;
    char *t = getExprType(expr, tableCourante, tableGlobale);
    if (t && strcmp(t, "void") == 0) {
        fprintf(stderr,
            "Erreur sémantique ligne %d: une fonction void ne peut pas être utilisée comme expression.\n",
            ligne);
        return 2;
    }
    return 0;
}

/*
 * Vérifie la compatibilité de type pour une affectation (ident = valeur).
 * Émet un warning int→char. Retourne 0 si compatible, 2 sinon.
 */
static int checkAssignType(Node *ident, Node *value,
                           Table_symb *tableCourante, Table_symb *tableGlobale) {
    char *type_dest = NULL;

    for (Table_symb *t = tableCourante; t; t = t->suiv)
        if (strcmp(t->ident, ident->value) == 0) { type_dest = t->type; break; }
    if (!type_dest)
        for (Table_symb *t = tableGlobale; t; t = t->suiv)
            if (strcmp(t->ident, ident->value) == 0) { type_dest = t->type; break; }

    if (!type_dest) return 0; /* variable inconnue, erreur déjà signalée ailleurs */

    char *type_src = getExprType(value, tableCourante, tableGlobale);
    if (!type_src) return 0;

    warningType(type_dest, type_src, value->lineno);

    /* Compatible : même type, ou char → int, ou int → char (avec warning déjà émis) */
    if (strcmp(type_dest, type_src) == 0)                                   return 0;
    if (strcmp(type_dest, "int")  == 0 && strcmp(type_src, "char") == 0)   return 0;
    if (strcmp(type_dest, "char") == 0 && strcmp(type_src, "int")  == 0)   return 0;
    if (strcmp(type_dest, "struct") == 0 || strcmp(type_src, "struct") == 0) return 0;

    fprintf(stderr, "Erreur sémantique ligne %d: types incompatibles ('%s' = '%s').\n",
            value->lineno, type_dest, type_src);
    return 2;
}

int haveCorrectMain(Table_symb **tableCourant) {
    if(!tableCourant) return 0;
    for (Table_symb *cur = *tableCourant; cur; cur = cur->suiv)
        if (strcmp(cur->ident, "main") == 0 &&
            strcmp(cur->type,  "int")  == 0 &&
            cur->kind == 'f')
            return 1;
    return 0;
}

int analyse_semantique(Node *node, Table_symb **tableCourante, Table_symb **tableGlobale,
                       FILE *anonym, int symbol, char *currentFctType) {
    if (node == NULL) return 0;

    switch (node->label) {

        /* ── Déclaration de structure globale (on l'enregistre pour les vérifs futures) ── */
        case L_DECL_STRUCT: {
            /*
             * Pour l'instant on ne vérifie pas les conflits de noms de structures
             * (le sujet dit que l'ident de struct peut être identique à une variable).
             * On traverse simplement pour ne pas crasher.
             */
            break;
        }

        /* ── Déclaration de variable ── */
        case L_DECL_VAR: {
            Node *typeNode = node->firstChild;
            if (!typeNode) break;

            char *typeStr = getTypeString(typeNode);

            Node *varNode = typeNode->nextSibling;
            while (varNode != NULL) {
                /* Conflit avec une fonction du même nom (contexte global uniquement) */
                if (tableGlobale == NULL || *tableGlobale == NULL) {
                    if (isInTableWithKind(varNode->value, *tableCourante, 'f')) {
                        fprintf(stderr,
                            "Erreur sémantique ligne %d: '%s' est déjà le nom d'une fonction.\n",
                            node->lineno, varNode->value);
                        return 2;
                    }
                }

                /* POINT 3 — Conflit param / variable locale :
                   add() retourne 0 si l'ident est déjà dans la table courante.
                   Quand on est dans le corps d'une fonction, tableCourante contient
                   déjà les paramètres → redéclarer un param comme variable locale
                   est détecté ici et produit l'erreur appropriée. */
                if (add(tableCourante, typeStr, varNode->value, 'v') == 0) {
                    fprintf(stderr,
                        "Erreur sémantique ligne %d: '%s' déjà déclaré (conflit param/variable locale ou double déclaration).\n",
                        node->lineno, varNode->value);
                    return 2;
                } else {
                    /* Offset uniquement pour les variables globales */
                    if (tableGlobale == NULL || *tableGlobale == NULL) {
                        int taille = (strcmp(typeStr, "char") == 0) ? 1 : 4;
                        Table_symb *t = *tableCourante;
                        while (t->suiv != NULL) t = t->suiv;
                        t->offset = global_offset;
                        global_offset += taille;
                    }
                }
                varNode = varNode->nextSibling;
            }
            break;
        }

        /* ── Déclaration de fonction ── */
        case L_DECL_FONCT: {
            Table_symb *tableLocale = NULL;

            Node *entete    = node->firstChild;
            Node *corps     = entete->nextSibling;
            Node *typeRetour = entete->firstChild;
            Node *nomFonct   = typeRetour->nextSibling;
            Node *params     = nomFonct->nextSibling;

            /* Conflit avec une variable globale du même nom */
            if (isInTableWithKind(nomFonct->value, *tableCourante, 'v')) {
                fprintf(stderr,
                    "Erreur sémantique ligne %d: '%s' est déjà le nom d'une variable globale.\n",
                    node->lineno, nomFonct->value);
                return 2;
            }

            /* Double déclaration de fonction */
            if (isInTableWithKind(nomFonct->value, *tableCourante, 'f')) {
                fprintf(stderr,
                    "Erreur sémantique ligne %d: fonction '%s' déjà déclarée.\n",
                    node->lineno, nomFonct->value);
                return 2;
            }

            /* Enregistrer la fonction dans la table globale AVANT l'analyse du corps
               (permet la récursivité directe et indirecte) */
            add(tableCourante, getTypeString(typeRetour), nomFonct->value, 'f');

            /* En-tête ASM pour main */
            if (strcmp(nomFonct->value, "main") == 0) {
                fprintf(anonym, "global _start\n");
                fprintf(anonym, "section .text\n");
                fprintf(anonym, "_start:\n");
            }

            printf("\n>>> Analyse de la fonction : %s\n", nomFonct->value);

            /* Ajout des paramètres dans la table locale */
            Node *param = params->firstChild;
            while (param != NULL) {
                Node *typeParam = param->firstChild;
                Node *nomParam  = typeParam->nextSibling;
                add(&tableLocale, getTypeString(typeParam), nomParam->value, 'v');
                param = param->nextSibling;
            }

            char *typeRetourStr = getTypeString(typeRetour);
            int ret = analyse_semantique(corps, &tableLocale, tableCourante,
                                         anonym, symbol, typeRetourStr);
            if (ret != 0) {
                freeTable(tableLocale);
                return ret;
            }

            /* Footer ASM pour main */
            if (strcmp(nomFonct->value, "main") == 0) {
                fprintf(anonym, "\tmov eax, 60\n");
                fprintf(anonym, "\tmov rdi, 0\n");
                fprintf(anonym, "\tsyscall\n");
            }

            if (symbol) {
                printf("--- Table des symboles (Locals + Params) pour '%s' ---\n", nomFonct->value);
                printT(tableLocale);
            }

            freeTable(tableLocale);
            break;
        }

        /* ── Affectation simple : ident = expr ── */
        case L_ASSIGN: {
            Node *ident = node->firstChild;
            Node *value = ident->nextSibling;

            if (!isInAnyTable(ident->value, *tableCourante,
                              tableGlobale ? *tableGlobale : NULL)) {
                fprintf(stderr,
                    "Erreur sémantique ligne %d: variable '%s' non déclarée.\n",
                    node->lineno, ident->value);
                return 2;
            }

            /* POINT 1 — interdire void en expression (côté droit de l'affectation) */
            int r = checkNotVoidExpr(value, *tableCourante,
                                     tableGlobale ? *tableGlobale : NULL, node->lineno);
            if (r != 0) return r;

            /* Vérification de type + warning int→char */
            r = checkAssignType(ident, value, *tableCourante,
                                tableGlobale ? *tableGlobale : NULL);
            if (r != 0) return r;

            translate_to_asm(value, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tmov [%s], eax\n", ident->value);
            break;
        }

        /* ── Affectation de champ de structure : ident.champ = expr ── */
        case L_FIELD_ASSIGN: {
            Node *child = node->firstChild;
            while (child) {
                int r = analyse_semantique(child, tableCourante, tableGlobale,
                                           anonym, symbol, currentFctType);
                if (r != 0) return r;
                child = child->nextSibling;
            }
            break;
        }

        /* ── Appel de fonction ── */
        case L_CALL: {
            Node *nom = node->firstChild;

            if (!isInAnyTable(nom->value, *tableCourante,
                              tableGlobale ? *tableGlobale : NULL)) {
                fprintf(stderr,
                    "Erreur sémantique ligne %d: fonction '%s' non déclarée.\n",
                    node->lineno, nom->value);
                return 2;
            }

            Node *arg = nom->nextSibling;
            while (arg != NULL) {
                /* Analyse sémantique récursive de l'argument */
                int r = analyse_semantique(arg, tableCourante, tableGlobale,
                                           anonym, symbol, currentFctType);
                if (r != 0) return r;

                /* POINT 1 — un argument ne peut pas être void */
                r = checkNotVoidExpr(arg, *tableCourante,
                                     tableGlobale ? *tableGlobale : NULL, arg->lineno);
                if (r != 0) return r;

                translate_to_asm(arg, anonym);
                arg = arg->nextSibling;
            }

            /* Génération de l'appel */
            if (strcmp(nom->value, "getint") == 0) {
                need_getint = 1;
                fprintf(anonym, "\tcall my_getint\n");
                fprintf(anonym, "\tpush eax\n");
            } else if (strcmp(nom->value, "putint") == 0) {
                need_putint = 1;
                fprintf(anonym, "\tcall my_putint\n");
                fprintf(anonym, "\tadd rsp, 8\n");
            } else if (strcmp(nom->value, "putchar") == 0) {
                need_putchar = 1;
                fprintf(anonym, "\tcall my_putchar\n");
            } else if (strcmp(nom->value, "getchar") == 0) {
                need_getchar = 1;
                fprintf(anonym, "\tcall my_getchar\n");
                fprintf(anonym, "\tpush eax\n");
            } else {
                fprintf(anonym, "\tcall %s\n", nom->value);
            }
            break;
        }

        /* ── if sans else ── */
        case L_IF: {
            Node *cond  = node->firstChild;
            Node *corps = cond->nextSibling;
            int lbl = label_count++;

            /* POINT 1 — la condition ne peut pas être void */
            int r = checkNotVoidExpr(cond, *tableCourante,
                                     tableGlobale ? *tableGlobale : NULL, node->lineno);
            if (r != 0) return r;

            /* Analyse sémantique de la condition (pour détecter les vars non déclarées etc.) */
            r = analyse_semantique(cond, tableCourante, tableGlobale,
                                   anonym, symbol, currentFctType);
            if (r != 0) return r;

            translate_to_asm(cond, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tje .fin_if_%d\n", lbl);

            r = analyse_semantique(corps, tableCourante, tableGlobale,
                                   anonym, symbol, currentFctType);
            if (r != 0) return r;

            fprintf(anonym, ".fin_if_%d:\n", lbl);
            break;
        }

        /* ── if / else ── */
        case L_IF_ELSE: {
            Node *cond       = node->firstChild;
            Node *corps_if   = cond->nextSibling;
            Node *corps_else = corps_if->nextSibling;
            int lbl = label_count++;

            /* POINT 1 */
            int r = checkNotVoidExpr(cond, *tableCourante,
                                     tableGlobale ? *tableGlobale : NULL, node->lineno);
            if (r != 0) return r;

            r = analyse_semantique(cond, tableCourante, tableGlobale,
                                   anonym, symbol, currentFctType);
            if (r != 0) return r;

            translate_to_asm(cond, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tje .sinon_%d\n", lbl);

            r = analyse_semantique(corps_if, tableCourante, tableGlobale,
                                   anonym, symbol, currentFctType);
            if (r != 0) return r;
            fprintf(anonym, "\tjmp .fin_si_%d\n", lbl);

            fprintf(anonym, ".sinon_%d:\n", lbl);
            r = analyse_semantique(corps_else, tableCourante, tableGlobale,
                                   anonym, symbol, currentFctType);
            if (r != 0) return r;

            fprintf(anonym, ".fin_si_%d:\n", lbl);
            break;
        }

        /* ── while ── */
        case L_WHILE: {
            Node *cond  = node->firstChild;
            Node *corps = cond->nextSibling;
            int lbl = label_count++;

            /* Vérifier que la condition n'est pas void */
            int r = checkNotVoidExpr(cond, *tableCourante,
                                     tableGlobale ? *tableGlobale : NULL, node->lineno);
            if (r != 0) return r;

            /* Analyse sémantique de la condition */
            r = analyse_semantique(cond, tableCourante, tableGlobale,
                                   anonym, symbol, currentFctType);
            if (r != 0) return r;

            /* Début de la boucle */
            fprintf(anonym, ".debut_while_%d:\n", lbl);
            
            /* Génération du code de la condition et test (32 bits) */
            translate_to_asm(cond, anonym);
            fprintf(anonym, "\tpop eax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tje .fin_while_%d\n", lbl);

            /* Corps de la boucle */
            r = analyse_semantique(corps, tableCourante, tableGlobale,
                                   anonym, symbol, currentFctType);
            if (r != 0) return r;

            /* Retour au début */
            fprintf(anonym, "\tjmp .debut_while_%d\n", lbl);
            fprintf(anonym, ".fin_while_%d:\n", lbl);
            break;
        }

        /* ── return ── */
        case L_RETURN: {
            Node *elem = node->firstChild;

            /* return sans valeur */
            if (elem == NULL) {
                if (currentFctType && strcmp(currentFctType, "void") != 0) {
                    fprintf(stderr,
                        "Erreur sémantique ligne %d: return sans valeur dans une fonction de type '%s'.\n",
                        node->lineno, currentFctType);
                    return 2;
                }
                break;
            }

            /* return avec valeur dans une fonction void */
            if (currentFctType && strcmp(currentFctType, "void") == 0) {
                fprintf(stderr,
                    "Erreur sémantique ligne %d: return avec valeur dans une fonction void.\n",
                    node->lineno);
                return 2;
            }

            /* POINT 1 — l'expression retournée ne peut pas être void */
            int r = checkNotVoidExpr(elem, *tableCourante,
                                     tableGlobale ? *tableGlobale : NULL, node->lineno);
            if (r != 0) return r;

            /* Vérification du type de retour */
            char *typeExpr = getExprType(elem, *tableCourante,
                                         tableGlobale ? *tableGlobale : NULL);
            if (typeExpr && currentFctType) {
                /* POINT 2 — warning si int retourné dans une fonction char */
                warningType(currentFctType, typeExpr, node->lineno);
                if (strcmp(typeExpr, currentFctType) != 0
                    && !(strcmp(currentFctType, "int")  == 0 && strcmp(typeExpr, "char") == 0)
                    && !(strcmp(currentFctType, "char") == 0 && strcmp(typeExpr, "int")  == 0)
                    && !(strcmp(currentFctType, "struct") == 0)) {
                    fprintf(stderr,
                        "Erreur sémantique ligne %d: type de retour incompatible (attendu '%s', obtenu '%s').\n",
                        node->lineno, currentFctType, typeExpr);
                    return 2;
                }
            }
            break;
        }

        /* ── Nœuds à traverser sans action particulière ── */
        default: {
            Node *child = node->firstChild;
            while (child != NULL) {
                int r = analyse_semantique(child, tableCourante, tableGlobale,
                                           anonym, symbol, currentFctType);
                if (r != 0) return r;
                child = child->nextSibling;
            }
            break;
        }
    }

    return 0;
}

void generer_footer_asm(FILE *anonym) {

    if (need_putint) {
        fprintf(anonym, "\nmy_putint:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tsub rsp, 32\n");
        fprintf(anonym, "\tpush rbx\n");
        /* Récupération du paramètre (32 bits) */
        fprintf(anonym, "\tmov eax, [rbp+16]\n");
        fprintf(anonym, "\tcmp eax, 0\n");
        fprintf(anonym, "\tjne .pi_nonzero\n");
        fprintf(anonym, "\tmov byte [rbp-32], '0'\n");
        fprintf(anonym, "\tmov eax, 1\n");
        fprintf(anonym, "\tmov edi, 1\n");
        fprintf(anonym, "\tlea rsi, [rbp-32]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tjmp .pi_done\n");
        fprintf(anonym, ".pi_nonzero:\n");
        fprintf(anonym, "\txor r8d, r8d\n");
        fprintf(anonym, "\tcmp eax, 0\n");
        fprintf(anonym, "\tjge .pi_pos\n");
        fprintf(anonym, "\tmov r8d, 1\n");
        fprintf(anonym, "\tneg eax\n");
        fprintf(anonym, ".pi_pos:\n");
        fprintf(anonym, "\tlea r9, [rbp-1]\n");
        fprintf(anonym, "\tmov ecx, 0\n");
        fprintf(anonym, "\tmov ebx, 10\n");
        fprintf(anonym, ".pi_loop:\n");
        fprintf(anonym, "\tcmp eax, 0\n");
        fprintf(anonym, "\tje .pi_write\n");
        fprintf(anonym, "\txor edx, edx\n");
        fprintf(anonym, "\tdiv ebx\n");
        fprintf(anonym, "\tadd dl, '0'\n");
        fprintf(anonym, "\tmov [r9], dl\n");
        fprintf(anonym, "\tdec r9\n");
        fprintf(anonym, "\tinc ecx\n");
        fprintf(anonym, "\tjmp .pi_loop\n");
        fprintf(anonym, ".pi_write:\n");
        fprintf(anonym, "\tcmp r8d, 0\n");
        fprintf(anonym, "\tje .pi_nosign\n");
        fprintf(anonym, "\tmov byte [r9], '-'\n");
        fprintf(anonym, "\tdec r9\n");
        fprintf(anonym, "\tinc ecx\n");
        fprintf(anonym, ".pi_nosign:\n");
        fprintf(anonym, "\tinc r9\n");
        fprintf(anonym, "\tmov eax, 1\n");
        fprintf(anonym, "\tmov rsi, r9\n");
        fprintf(anonym, "\tmov edi, 1\n");
        fprintf(anonym, "\tmov edx, ecx\n");
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
        /* Récupération du paramètre (32 bits, mais seul l'octet bas nous intéresse) */
        fprintf(anonym, "\tmov eax, [rbp+16]\n");
        fprintf(anonym, "\tmov [rbp-1], al\n");
        fprintf(anonym, "\tmov eax, 1\n");
        fprintf(anonym, "\tmov edi, 1\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmov rsp, rbp\n");
        fprintf(anonym, "\tpop rbp\n");
        fprintf(anonym, "\tret\n");
    }

    if (need_getint) {
        fprintf(anonym, "\nmy_getint:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tsub rsp, 16\n");
        fprintf(anonym, "\txor r12d, r12d\n");
        /* Lire le premier caractère */
        fprintf(anonym, "\tmov eax, 0\n");
        fprintf(anonym, "\tmov edi, 0\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmovzx ebx, byte [rbp-1]\n");
        /* Accepter signe + ou - */
        fprintf(anonym, "\txor r13d, r13d\n");        /* r13d = signe négatif (0 = positif) */
        fprintf(anonym, "\tcmp ebx, '-'\n");
        fprintf(anonym, "\tje .gi_minus\n");
        fprintf(anonym, "\tcmp ebx, '+'\n");
        fprintf(anonym, "\tje .gi_plus\n");
        fprintf(anonym, "\tjmp .gi_check_digit\n");
        fprintf(anonym, ".gi_minus:\n");
        fprintf(anonym, "\tmov r13d, 1\n");
        fprintf(anonym, ".gi_plus:\n");
        /* Lire le prochain caractère après le signe */
        fprintf(anonym, "\tmov eax, 0\n");
        fprintf(anonym, "\tmov edi, 0\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmovzx ebx, byte [rbp-1]\n");
        fprintf(anonym, ".gi_check_digit:\n");
        fprintf(anonym, "\tcmp ebx, '0'\n");
        fprintf(anonym, "\tjl .gi_error\n");
        fprintf(anonym, "\tcmp ebx, '9'\n");
        fprintf(anonym, "\tjg .gi_error\n");
        /* Boucle de lecture des chiffres */
        fprintf(anonym, ".gi_loop:\n");
        fprintf(anonym, "\tsub ebx, '0'\n");
        fprintf(anonym, "\timul r12d, 10\n");
        fprintf(anonym, "\tadd r12d, ebx\n");
        fprintf(anonym, "\tmov eax, 0\n");
        fprintf(anonym, "\tmov edi, 0\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmovzx ebx, byte [rbp-1]\n");
        fprintf(anonym, "\tcmp ebx, '0'\n");
        fprintf(anonym, "\tjl .gi_done\n");
        fprintf(anonym, "\tcmp ebx, '9'\n");
        fprintf(anonym, "\tjg .gi_done\n");
        fprintf(anonym, "\tjmp .gi_loop\n");
        /* Vérifier que le dernier caractère lu est un newline */
        fprintf(anonym, ".gi_done:\n");
        fprintf(anonym, "\tcmp ebx, 10\n");           /* 10 = '\n' */
        fprintf(anonym, "\tjne .gi_error\n");
        fprintf(anonym, "\tmov eax, r12d\n");
        fprintf(anonym, "\tcmp r13d, 0\n");
        fprintf(anonym, "\tje .gi_ret\n");
        fprintf(anonym, "\tneg eax\n");
        fprintf(anonym, ".gi_ret:\n");
        fprintf(anonym, "\tmov rsp, rbp\n");
        fprintf(anonym, "\tpop rbp\n");
        fprintf(anonym, "\tret\n");
        fprintf(anonym, ".gi_error:\n");
        fprintf(anonym, "\tmov eax, 60\n");
        fprintf(anonym, "\tmov edi, 5\n");
        fprintf(anonym, "\tsyscall\n");
    }

    if (need_getchar) {
        fprintf(anonym, "\nmy_getchar:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tsub rsp, 8\n");
        fprintf(anonym, "\tmov eax, 0\n");
        fprintf(anonym, "\tmov edi, 0\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmovzx eax, byte [rbp-1]\n");
        fprintf(anonym, "\tmov rsp, rbp\n");
        fprintf(anonym, "\tpop rbp\n");
        fprintf(anonym, "\tret\n");
    }
}

void generer_bss(FILE *anonym, Table_symb *tableGlobale) {
    fprintf(anonym, "\nsection .bss\n");
    for (Table_symb *t = tableGlobale; t; t = t->suiv) {
        if (t->kind != 'v') continue;
        if (strcmp(t->type, "int") == 0)
            fprintf(anonym, "%s resd 1\n", t->ident);
        else if (strcmp(t->type, "char") == 0)
            fprintf(anonym, "%s resb 1\n", t->ident);
    }
}