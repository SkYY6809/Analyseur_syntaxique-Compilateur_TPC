#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantique.h"
#include "compiler.h"
#include "struct_table.h"

static int need_putchar = 0;
static int need_putint  = 0;
static int need_getchar = 0;
static int need_getint  = 0;
static int global_offset = 0;
static int label_count = 0;
//static StructDef * struct_definitions = NULL; 


TypeInfo* make_type_info(const char *base_type, const char *struct_name) {
    TypeInfo *ti = malloc(sizeof(TypeInfo));
    ti->base_type = strdup(base_type);
    ti->struct_name = struct_name ? strdup(struct_name) : NULL;
    return ti;
}

void free_type_info(TypeInfo *ti) {
    if (ti) {
        free(ti->base_type);
        if (ti->struct_name) free(ti->struct_name);
        free(ti);
    }
}

char* type_info_to_string(TypeInfo *ti) {
    static char buffer[256];
    if (!ti) return "unknown";
    if (strcmp(ti->base_type, "struct") == 0 && ti->struct_name) {
        snprintf(buffer, sizeof(buffer), "struct %s", ti->struct_name);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", ti->base_type);
    }
    return buffer;
}

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
            /* Ici il faudra checker dans les tables, pour l'instant simplifié */
            fprintf(anonym, "\tmov eax, dword [%s]\n", node->value);
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        case L_NUM: {
            fprintf(anonym, "\tmov rax, %s\n", node->value);
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        case L_CHAR: {
            fprintf(anonym, "\tmov rax, %s\n", node->value);
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        case L_NEG: {
            translate_to_asm(node->firstChild, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tneg rax\n");
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        case L_NOT: {
            translate_to_asm(node->firstChild, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
            fprintf(anonym, "\tsete al\n");
            fprintf(anonym, "\tmovzx rax, al\n");
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        case L_AND: {
            int lbl = label_count++;
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
            fprintf(anonym, "\tje .and_false_%d\n", lbl);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
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
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
            fprintf(anonym, "\tjne .or_true_%d\n", lbl);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
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
            fprintf(anonym, "\tpop rbx\n");
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tsub rax, rbx\n");
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        case L_ADD: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop rbx\n");
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tadd rax, rbx\n");
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        case L_MUL: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop rbx\n");
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\timul rax, rbx\n");
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        case L_DIV: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop rbx\n");
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcqo\n");          /* sign extension rax -> rdx:rax */
            fprintf(anonym, "\tidiv rbx\n");
            fprintf(anonym, "\tpush rax\n");     /* quotient */
            break;
        }

        case L_MOD: {
            Node *left  = node->firstChild;
            Node *right = left->nextSibling;
            translate_to_asm(left, anonym);
            translate_to_asm(right, anonym);
            fprintf(anonym, "\tpop rbx\n");
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcqo\n");
            fprintf(anonym, "\tidiv rbx\n");
            fprintf(anonym, "\tpush rdx\n");     /* reste */
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
            fprintf(anonym, "\tpop rbx\n");
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, rbx\n");
            fprintf(anonym, "\tset%s al\n", set);
            fprintf(anonym, "\tmovzx rax, al\n");
            fprintf(anonym, "\tpush rax\n");
            break;
        }

        default :
            break;
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
 * Retourne le type d'une expression, ou NULL si indéterminable.
 * Règle du sujet : toute opération sur char produit un int (conversion implicite)
 */
// Remplacer l'ancienne fonction getExprType par celle-ci
static TypeInfo* getExprType(Node *node, Table_symb *tableCourante, Table_symb *tableGlobale) {
    if (!node) return NULL;

    switch (node->label) {
        case L_NUM:
            return make_type_info("int", NULL);
            
        case L_CHAR:
            return make_type_info("char", NULL);
            
        case L_IDENT: {
            Table_symb *t;
            for (t = tableCourante; t; t = t->suiv)
                if (strcmp(t->ident, node->value) == 0)
                    return make_type_info(t->type, 
                        (strcmp(t->type, "struct") == 0 && t->struct_name) ? t->struct_name : NULL);
            for (t = tableGlobale; t; t = t->suiv)
                if (strcmp(t->ident, node->value) == 0)
                    return make_type_info(t->type,
                        (strcmp(t->type, "struct") == 0 && t->struct_name) ? t->struct_name : NULL);
            return NULL;
        }
        
        case L_CALL: {
            if (!node->firstChild) return NULL;
            const char *fname = node->firstChild->value;
            Table_symb *t;
            for (t = tableCourante; t; t = t->suiv)
                if (strcmp(t->ident, fname) == 0)
                    return make_type_info(t->type,
                        (strcmp(t->type, "struct") == 0 && t->struct_name) ? t->struct_name : NULL);
            for (t = tableGlobale; t; t = t->suiv)
                if (strcmp(t->ident, fname) == 0)
                    return make_type_info(t->type,
                        (strcmp(t->type, "struct") == 0 && t->struct_name) ? t->struct_name : NULL);
            return NULL;
        }
        
        case L_FIELD_ACCESS: {
            // Récupérer le type de la base
            Node *base = node->firstChild;
            if (!base) return NULL;
            
            TypeInfo *base_type = getExprType(base, tableCourante, tableGlobale);
            if (!base_type) return NULL;
            
            // Vérifier que la base est une structure
            if (strcmp(base_type->base_type, "struct") != 0) {
                fprintf(stderr, "Erreur sémantique ligne %d : accès champ sur un type non-structure (%s)\n",
                        node->lineno, base_type->base_type);
                free_type_info(base_type);
                return NULL;
            }
            
            // Chercher la définition de la structure
            StructDef *s = find_struct(base_type->struct_name);
            if (!s) {
                fprintf(stderr, "Erreur sémantique ligne %d : structure '%s' non définie\n",
                        node->lineno, base_type->struct_name);
                free_type_info(base_type);
                return NULL;
            }
            
            // Parcourir les champs (le premier champ est à base->nextSibling)
            Node *field_node = base->nextSibling;
            TypeInfo *current_type = base_type;
            StructDef *current_struct = s;
            
            while (field_node && field_node->label == L_IDENT) {
                Field *f = find_field(current_struct, field_node->value);
                if (!f) {
                    fprintf(stderr, "Erreur sémantique ligne %d : champ '%s' inexistant dans structure '%s'\n",
                            node->lineno, field_node->value, current_struct->name);
                    free_type_info(current_type);
                    return NULL;
                }
                
                // Libérer l'ancien type_info sauf pour le premier
                if (current_type != base_type) free_type_info(current_type);
                
                // Créer le nouveau type_info pour le champ
                current_type = make_type_info(f->type, f->struct_name);
                
                // Si le champ est une structure, la suivante pour l'imbrication
                if (strcmp(f->type, "struct") == 0 && f->struct_name) {
                    current_struct = find_struct(f->struct_name);
                    if (!current_struct) {
                        fprintf(stderr, "Erreur sémantique ligne %d : structure '%s' non définie\n",
                                node->lineno, f->struct_name);
                        free_type_info(current_type);
                        return NULL;
                    }
                } else {
                    current_struct = NULL;
                }
                
                field_node = field_node->nextSibling;
            }
            
            free_type_info(base_type);
            return current_type;
        }
        
        case L_ADD: case L_SUB: case L_MUL: case L_DIV: case L_MOD: {
            Node *left = node->firstChild;
            Node *right = left ? left->nextSibling : NULL;
            
            TypeInfo *leftType = left ? getExprType(left, tableCourante, tableGlobale) : NULL;
            TypeInfo *rightType = right ? getExprType(right, tableCourante, tableGlobale) : NULL;
            
            if (!leftType || !rightType) {
                if (leftType) free_type_info(leftType);
                if (rightType) free_type_info(rightType);
                return NULL;
            }
            
            int is_left_struct = (strcmp(leftType->base_type, "struct") == 0);
            int is_right_struct = (strcmp(rightType->base_type, "struct") == 0);
            
            if (is_left_struct || is_right_struct) {
                fprintf(stderr, "Erreur sémantique ligne %d : opération arithmétique sur des structures\n",
                        node->lineno);
                free_type_info(leftType);
                free_type_info(rightType);
                return NULL;
            }
            
            free_type_info(leftType);
            free_type_info(rightType);
            return make_type_info("int", NULL);
        }
        
        case L_NOT: case L_AND: case L_OR:
        case L_EQ: case L_NEQ: case L_LT: case L_GT: case L_LE: case L_GE:
        case L_NEG:
            return make_type_info("int", NULL);
            
        default: {
            // Pour les autres nœuds, on propage (mais normalement on ne devrait pas arriver là)
            Node *child = node->firstChild;
            while (child) {
                TypeInfo *ti = getExprType(child, tableCourante, tableGlobale);
                if (ti && strcmp(ti->base_type, "void") != 0) {
                    free_type_info(ti);
                    return make_type_info("int", NULL);
                }
                if (ti) free_type_info(ti);
                child = child->nextSibling;
            }
            return NULL;
        }
    }
}

static int checkNotVoidExpr(Node *expr, Table_symb *tableCourante, Table_symb *tableGlobale, int ligne) {
    if (!expr) return 0;
    TypeInfo *t = getExprType(expr, tableCourante, tableGlobale);
    if (!t) return 0;
    
    int is_void = (strcmp(t->base_type, "void") == 0);
    free_type_info(t);
    
    if (is_void) {
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
static int checkAssignType(Node *ident, Node *value, Table_symb *tableCourante, Table_symb *tableGlobale) {
    TypeInfo *type_dest = getExprType(ident, tableCourante, tableGlobale);
    TypeInfo *type_src = getExprType(value, tableCourante, tableGlobale);
    
    if (!type_dest || !type_src) {
        if (type_dest) free_type_info(type_dest);
        if (type_src) free_type_info(type_src);
        return 0;
    }
    
    // Warning int -> char
    if (strcmp(type_dest->base_type, "char") == 0 && strcmp(type_src->base_type, "int") == 0) {
        fprintf(stderr, "Warning ligne %d : affectation d'un int vers un char\n", value->lineno);
    }
    
    // Vérification de compatibilité
    int compatible = 0;
    
    if (strcmp(type_dest->base_type, type_src->base_type) == 0) {
        if (strcmp(type_dest->base_type, "struct") == 0) {
            // Même structure
            if (type_dest->struct_name && type_src->struct_name &&
                strcmp(type_dest->struct_name, type_src->struct_name) == 0) {
                compatible = 1;
            }
        } else {
            compatible = 1;
        }
    }
    
    // char <-> int (avec warning déjà émis)
    if ((strcmp(type_dest->base_type, "int") == 0 && strcmp(type_src->base_type, "char") == 0) ||
        (strcmp(type_dest->base_type, "char") == 0 && strcmp(type_src->base_type, "int") == 0)) {
        compatible = 1;
    }
    
    if (!compatible) {
        fprintf(stderr, "Erreur sémantique ligne %d: types incompatibles ('%s' = '%s').\n",
                value->lineno, type_info_to_string(type_dest), type_info_to_string(type_src));
        free_type_info(type_dest);
        free_type_info(type_src);
        return 2;
    }
    
    free_type_info(type_dest);
    free_type_info(type_src);
    return 0;
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

int analyse_semantique(Node *node, Table_symb **tableCourante, Table_symb **tableGlobale, FILE *anonym, int symbol, char *currentFctType) {
    if (node == NULL) return 0;

    switch (node->label) {

        /* ── Déclaration de structure globale (on l'enregistre pour les vérifs futures) ── */
        case L_DECL_STRUCT: {
            Node *struct_name_node = node->firstChild;
            if (!struct_name_node) break;
            
            char *struct_name = struct_name_node->value;
            
            // Collecter les champs
            Field *fields = NULL;
            Node *champ = struct_name_node->nextSibling;
            
            while (champ && champ->label == L_CHAMP) {
                Node *type_node = champ->firstChild;
                Node *declarateurs = type_node->nextSibling;
                
                char *field_type = NULL;
                char *field_struct_name = NULL;
                
                if (type_node->label == L_TYPE_INT) {
                    field_type = "int";
                } else if (type_node->label == L_TYPE_CHAR) {
                    field_type = "char";
                } else if (type_node->label == L_TYPE_STRUCT) {
                    field_type = "struct";
                    field_struct_name = type_node->value;
                    // Vérifier que la structure du champ existe déjà
                    if (!find_struct(field_struct_name)) {
                        // Ce n'est pas une erreur si elle est définie plus tard (en C c'est autorisé)
                        // Mais on peut émettre un warning
                        fprintf(stderr, "Warning ligne %d : structure '%s' utilisée avant définition\n",
                                node->lineno, field_struct_name);
                    }
                }
                
                // Parcourir les déclarateurs
                Node *decl = declarateurs;
                while (decl && decl->label == L_IDENT) {
                    Field *f = make_field(decl->value, field_type, field_struct_name);
                    add_field(&fields, f);
                    decl = decl->nextSibling;
                }
                
                champ = champ->nextSibling;
            }
            
            add_struct(struct_name, fields);
            break;
        }

        /* ── Déclaration de variable ── */
        case L_DECL_VAR: {
            Node *typeNode = node->firstChild;
            if (!typeNode) break;

            char *typeStr = NULL;
            char *struct_name = NULL;
            
            if (typeNode->label == L_TYPE_INT) {
                typeStr = "int";
            } else if (typeNode->label == L_TYPE_CHAR) {
                typeStr = "char";
            } else if (typeNode->label == L_TYPE_STRUCT) {
                typeStr = "struct";
                struct_name = typeNode->value;
                // Vérifier que la structure existe
                if (!find_struct(struct_name)) {
                    fprintf(stderr, "Erreur sémantique ligne %d: structure '%s' non définie\n",
                            node->lineno, struct_name);
                    return 2;
                }
            }

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

                /* Ajout de la variable avec ou sans structure */
                int added;
                if (struct_name) {
                    added = add_struct_var(tableCourante, typeStr, struct_name, varNode->value, 'v');
                } else {
                    added = add(tableCourante, typeStr, varNode->value, 'v');
                }
                
                if (added == 0) {
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

            /* Récupérer le type de retour (peut être struct) */
            char *ret_type = NULL;
            char *ret_struct_name = NULL;
            
            if (typeRetour->label == L_TYPE_INT) {
                ret_type = "int";
            } else if (typeRetour->label == L_TYPE_CHAR) {
                ret_type = "char";
            } else if (typeRetour->label == L_TYPE_VOID) {
                ret_type = "void";
            } else if (typeRetour->label == L_TYPE_STRUCT) {
                ret_type = "struct";
                ret_struct_name = typeRetour->value;
            }
            
            /* Enregistrer la fonction avec les infos de structure si nécessaire */
            if (ret_struct_name) {
                add_struct_var(tableCourante, ret_type, ret_struct_name, nomFonct->value, 'f');
            } else {
                add(tableCourante, ret_type, nomFonct->value, 'f');
            }

            /* En-tête ASM pour main */
            if (strcmp(nomFonct->value, "main") == 0) {
                if (strcmp(ret_type, "int") != 0) {
                    fprintf(stderr, "Erreur sémantique ligne %d: main doit retourner int\n", node->lineno);
                    return 2;
                }
                fprintf(anonym, "global main\n");
                fprintf(anonym, "section .text\n");
                fprintf(anonym, "main:\n");
                fprintf(anonym, "\tpush rbp\n");
                fprintf(anonym, "\tmov rbp, rsp\n");
            }

            printf("\n>>> Analyse de la fonction : %s\n", nomFonct->value);

            /* Ajout des paramètres dans la table locale */
            Node *param = params->firstChild;
            while (param != NULL) {
                Node *typeParam = param->firstChild;
                Node *nomParam  = typeParam->nextSibling;
                
                char *param_type = NULL;
                char *param_struct_name = NULL;
                
                if (typeParam->label == L_TYPE_INT) {
                    param_type = "int";
                } else if (typeParam->label == L_TYPE_CHAR) {
                    param_type = "char";
                } else if (typeParam->label == L_TYPE_STRUCT) {
                    param_type = "struct";
                    param_struct_name = typeParam->value;
                }
                
                if (param_struct_name) {
                    add_struct_var(&tableLocale, param_type, param_struct_name, nomParam->value, 'v');
                } else {
                    add(&tableLocale, param_type, nomParam->value, 'v');
                }
                param = param->nextSibling;
            }

            int ret = analyse_semantique(corps, &tableLocale, tableCourante,
                                        anonym, symbol, ret_type);
            if (ret != 0) {
                freeTable(tableLocale);
                return ret;
            }

            /* Footer ASM pour main */
            if (strcmp(nomFonct->value, "main") == 0) {
                fprintf(anonym, "\tmov rsp, rbp\n");
                fprintf(anonym, "\tpop rbp\n");
                fprintf(anonym, "\tret\n");
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
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tmov [%s], eax\n", ident->value);
            break;
        }

        /* ── Affectation de champ de structure : ident.champ = expr ── */
        case L_FIELD_ASSIGN: {
            Node *field_access = node->firstChild;
            Node *value = field_access ? field_access->nextSibling : NULL;
            
            if (!field_access || !value) break;
            
            // Vérifier le type du côté gauche (l'accès au champ)
            TypeInfo *left_type = getExprType(field_access, *tableCourante,
                                            tableGlobale ? *tableGlobale : NULL);
            if (!left_type) return 2;
            
            // Vérifier que le côté gauche n'est pas void
            if (strcmp(left_type->base_type, "void") == 0) {
                fprintf(stderr, "Erreur sémantique ligne %d: affectation à une expression void\n",
                        node->lineno);
                free_type_info(left_type);
                return 2;
            }
            
            // Vérifier le type du côté droit
            int r = checkNotVoidExpr(value, *tableCourante,
                                    tableGlobale ? *tableGlobale : NULL, node->lineno);
            if (r != 0) {
                free_type_info(left_type);
                return r;
            }
            
            TypeInfo *right_type = getExprType(value, *tableCourante,
                                            tableGlobale ? *tableGlobale : NULL);
            
            // Vérifier la compatibilité
            int compatible = 0;
            if (strcmp(left_type->base_type, right_type->base_type) == 0) {
                if (strcmp(left_type->base_type, "struct") == 0) {
                    if (left_type->struct_name && right_type->struct_name &&
                        strcmp(left_type->struct_name, right_type->struct_name) == 0)
                        compatible = 1;
                } else {
                    compatible = 1;
                }
            }
            
            if (strcmp(left_type->base_type, "char") == 0 && strcmp(right_type->base_type, "int") == 0) {
                fprintf(stderr, "Warning ligne %d : affectation d'un int vers un char\n", node->lineno);
                compatible = 1;
            }
            
            if (strcmp(left_type->base_type, "int") == 0 && strcmp(right_type->base_type, "char") == 0) {
                compatible = 1;
            }
            
            if (!compatible) {
                fprintf(stderr, "Erreur sémantique ligne %d: types incompatibles pour l'affectation de champ\n",
                        node->lineno);
                free_type_info(left_type);
                free_type_info(right_type);
                return 2;
            }
            
            free_type_info(left_type);
            free_type_info(right_type);
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
                fprintf(anonym, "\tpush rax\n");
            } else if (strcmp(nom->value, "putint") == 0) {
                need_putint = 1;
                /* Récupérer l'argument depuis la pile (convention de votre compilateur) */
                fprintf(anonym, "\tpop rdi\n");  /* 1er argument dans rdi */
                fprintf(anonym, "\tcall my_putint\n");
            } else if (strcmp(nom->value, "putchar") == 0) {
                need_putchar = 1;
                fprintf(anonym, "\tpop rdi\n");
                fprintf(anonym, "\tcall my_putchar\n");
            } else if (strcmp(nom->value, "getchar") == 0) {
                need_getchar = 1;
                fprintf(anonym, "\tcall my_getchar\n");
                fprintf(anonym, "\tpush rax\n");
            } else {
                /* Pour un appel normal, les arguments sont déjà sur la pile
                Il faut les dépiler dans les registres dans l'ordre inverse */
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
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
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
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
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
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp rax, 0\n");
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
            TypeInfo *typeExpr = getExprType(elem, *tableCourante,
                                            tableGlobale ? *tableGlobale : NULL);
            if (typeExpr && currentFctType) {
                /* POINT 2 — warning si int retourné dans une fonction char */
                warningType(currentFctType, typeExpr->base_type, node->lineno);
                
                int compatible = 0;
                if (strcmp(typeExpr->base_type, currentFctType) == 0) {
                    compatible = 1;
                } else if (strcmp(currentFctType, "int") == 0 && strcmp(typeExpr->base_type, "char") == 0) {
                    compatible = 1;
                } else if (strcmp(currentFctType, "char") == 0 && strcmp(typeExpr->base_type, "int") == 0) {
                    compatible = 1;
                }
                
                if (!compatible) {
                    fprintf(stderr,
                        "Erreur sémantique ligne %d: type de retour incompatible (attendu '%s', obtenu '%s').\n",
                        node->lineno, currentFctType, typeExpr->base_type);
                    free_type_info(typeExpr);
                    return 2;
                }
            }
            if (typeExpr) free_type_info(typeExpr);
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