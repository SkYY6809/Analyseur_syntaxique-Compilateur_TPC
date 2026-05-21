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
static int label_count = 0; //COMPTEUR DE LABELS (pour if/while/&&/||)


/* Taille d'une structure (récursif) */
static int struct_size(const char *sname);

static int field_size(Field *f) {
    if (!f) return 0;
    if (strcmp(f->type, "int")  == 0) return 4;
    if (strcmp(f->type, "char") == 0) return 1;
    if (strcmp(f->type, "struct") == 0 && f->struct_name)
        return struct_size(f->struct_name);
    return 8;
}

static int struct_size(const char *sname) {
    if (!sname) return 0;
    StructDef *s = find_struct(sname);
    if (!s) return 0;
    int total = 0;
    for (Field *f = s->fields; f; f = f->next)
        total += field_size(f);
    return total;
}

/* Taille d'un type décrit par un nœud de type */
static int type_node_size(Node *typeNode) {
    if (!typeNode) return 8;
    switch (typeNode->label) {
        case L_TYPE_INT:    return 4;
        case L_TYPE_CHAR:   return 1;
        case L_TYPE_STRUCT: return struct_size(typeNode->value);
        default:            return 8;
    }
}


TypeInfo* make_type_info(const char *base_type, const char *struct_name) {
    TypeInfo *ti = malloc(sizeof(TypeInfo));
    ti->base_type   = strdup(base_type);
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
    if (strcmp(ti->base_type, "struct") == 0 && ti->struct_name)
        snprintf(buffer, sizeof(buffer), "struct %s", ti->struct_name);
    else
        snprintf(buffer, sizeof(buffer), "%s", ti->base_type);
    return buffer;
}

static int isInAnyTable(const char *ident, Table_symb *locale, Table_symb *globale) {
    return isInTable((char*)ident, locale) || isInTable((char*)ident, globale);
}

/* Cherche une entrée par nom dans locale puis globale, retourne-la ou NULL */
static Table_symb* findEntry(const char *ident, Table_symb *locale, Table_symb *globale) {
    for (Table_symb *t = locale; t; t = t->suiv)
        if (strcmp(t->ident, ident) == 0) return t;
    for (Table_symb *t = globale; t; t = t->suiv)
        if (strcmp(t->ident, ident) == 0) return t;
    return NULL;
}

void init_builtins(Table_symb **tableGlobale) {
    add(tableGlobale, "int",  "getint",  'f');
    add(tableGlobale, "void", "putint",  'f');
    add(tableGlobale, "int",  "getchar", 'f');
    add(tableGlobale, "void", "putchar", 'f');
}

static TypeInfo* getExprType(Node *node, Table_symb *tableCourante, Table_symb *tableGlobale) {
    if (!node) return NULL;

    switch (node->label) {

        case L_NUM:
            return make_type_info("int", NULL);

        case L_CHAR:
            return make_type_info("char", NULL);

        case L_IDENT: {
            Table_symb *e = findEntry(node->value, tableCourante, tableGlobale);
            if (!e) return NULL;
            return make_type_info(e->type,
                (strcmp(e->type, "struct") == 0) ? e->struct_name : NULL);
        }

        case L_CALL: {
            if (!node->firstChild) return NULL;
            Table_symb *e = findEntry(node->firstChild->value, tableCourante, tableGlobale);
            if (!e) return NULL;
            return make_type_info(e->type,
                (strcmp(e->type, "struct") == 0) ? e->struct_name : NULL);
        }

        case L_FIELD_ACCESS: {
            Node *base = node->firstChild;
            if (!base) return NULL;
            TypeInfo *base_type = getExprType(base, tableCourante, tableGlobale);
            if (!base_type) return NULL;
            if (strcmp(base_type->base_type, "struct") != 0) {
                free_type_info(base_type);
                return NULL;
            }
            StructDef *s = find_struct(base_type->struct_name);
            free_type_info(base_type);
            if (!s) return NULL;

            TypeInfo *cur = NULL;
            Node *field_node = base->nextSibling;
            while (field_node) {
                Field *f = find_field(s, field_node->value);
                if (!f) { if (cur) free_type_info(cur); return NULL; }
                if (cur) free_type_info(cur);
                cur = make_type_info(f->type, f->struct_name);
                if (strcmp(f->type, "struct") == 0 && f->struct_name)
                    s = find_struct(f->struct_name);
                else
                    s = NULL;
                field_node = field_node->nextSibling;
            }
            return cur;
        }

        case L_ADD: case L_SUB: case L_MUL: case L_DIV: case L_MOD:
        case L_NOT: case L_AND: case L_OR:
        case L_EQ:  case L_NEQ: case L_LT:  case L_GT:  case L_LE:  case L_GE:
        case L_NEG:
            return make_type_info("int", NULL);

        default:
            return NULL;
    }
}


/* Retourne 2 et imprime une erreur si l'expression est de type void */
static int checkNotVoidExpr(Node *expr, Table_symb *tableCourante, Table_symb *tableGlobale, int ligne) {
    if (!expr) return 0;
    TypeInfo *t = getExprType(expr, tableCourante, tableGlobale);
    if (!t) return 0;
    int is_void = (strcmp(t->base_type, "void") == 0);
    free_type_info(t);
    if (is_void) {
        fprintf(stderr,
            "Erreur sémantique ligne %d : une fonction void ne peut pas être utilisée comme expression.\n",
            ligne);
        return 2;
    }
    return 0;
}

/* Vérifie la compatibilité de type pour une affectation dest = src.
   Émet un warning int→char. Retourne 0 si OK, 2 si erreur. */
static int checkAssignType(const char *type_dest, const char *sname_dest, const char *type_src,  const char *sname_src, int ligne) {
    if (!type_dest || !type_src) return 0;

    /* Warning int → char */
    if (strcmp(type_dest, "char") == 0 && strcmp(type_src, "int") == 0) {
        fprintf(stderr, "Warning ligne %d : affectation d'un int vers un char.\n", ligne);
        return 0; /* compatible mais avec warning */
    }
    /* char → int : OK  */
    if (strcmp(type_dest, "int") == 0 && strcmp(type_src, "char") == 0)
        return 0;
    /* Mêmes types primitifs */
    if (strcmp(type_dest, type_src) == 0) {
        if (strcmp(type_dest, "struct") == 0) {
            if (!sname_dest || !sname_src || strcmp(sname_dest, sname_src) != 0) {
                fprintf(stderr,
                    "Erreur sémantique ligne %d : structures incompatibles ('%s' ≠ '%s').\n",
                    ligne,
                    sname_dest ? sname_dest : "?",
                    sname_src  ? sname_src  : "?");
                return 2;
            }
        }
        return 0;
    }
    fprintf(stderr,
        "Erreur sémantique ligne %d : types incompatibles ('%s' = '%s').\n",
        ligne, type_dest, type_src);
    return 2;
}


/* Émet le code pour pousser la valeur d'un identifiant sur la pile. */
static void emit_load_ident(const char *name, Table_symb *ctx_local, Table_symb *ctx_global, FILE *out) {
    /* Chercher en local d'abord */
    for (Table_symb *t = ctx_local; t; t = t->suiv) {
        if (strcmp(t->ident, name) == 0 && t->kind == 'v') {
            if (strcmp(t->type, "char") == 0) {
                fprintf(out, "\tmovsx eax, byte [rbp-%d]\n", t->offset);
            } else {
                fprintf(out, "\tmov eax, dword [rbp-%d]\n", t->offset);
            }
            fprintf(out, "\tpush rax\n");
            return;
        }
    }
    /* Chercher en global */
    for (Table_symb *t = ctx_global; t; t = t->suiv) {
        if (strcmp(t->ident, name) == 0 && t->kind == 'v') {
            if (strcmp(t->type, "char") == 0) {
                fprintf(out, "\tmovsx eax, byte [%s]\n", name);
            } else {
                fprintf(out, "\tmov eax, dword [%s]\n", name);
            }
            fprintf(out, "\tpush rax\n");
            return;
        }
    }
}

static void emit_expr(Node *node, Table_symb *ctx_local, Table_symb *ctx_global, FILE *out) {
    if (!node) return;

    switch (node->label) {

        case L_NUM:
            fprintf(out, "\tpush %s\n", node->value);
            break;

        case L_CHAR:
            /* node->value est de la forme 'x' ou '\n' etc. */
            fprintf(out, "\tpush %s\n", node->value);
            break;

        case L_IDENT:
            emit_load_ident(node->value, ctx_local, ctx_global, out);
            break;

        case L_NEG: {
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tneg eax\n");
            fprintf(out, "\tpush rax\n");
            break;
        }

        case L_NOT: {
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tcmp eax, 0\n");
            fprintf(out, "\tsete al\n");
            fprintf(out, "\tmovzx eax, al\n");
            fprintf(out, "\tpush rax\n");
            break;
        }

        case L_AND: {
            int lbl = label_count++;
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tcmp eax, 0\n");
            fprintf(out, "\tje .and_false_%d\n", lbl);
            emit_expr(node->firstChild->nextSibling, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tcmp eax, 0\n");
            fprintf(out, "\tje .and_false_%d\n", lbl);
            fprintf(out, "\tpush 1\n");
            fprintf(out, "\tjmp .and_end_%d\n", lbl);
            fprintf(out, ".and_false_%d:\n", lbl);
            fprintf(out, "\tpush 0\n");
            fprintf(out, ".and_end_%d:\n", lbl);
            break;
        }

        case L_OR: {
            int lbl = label_count++;
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tcmp eax, 0\n");
            fprintf(out, "\tjne .or_true_%d\n", lbl);
            emit_expr(node->firstChild->nextSibling, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tcmp eax, 0\n");
            fprintf(out, "\tjne .or_true_%d\n", lbl);
            fprintf(out, "\tpush 0\n");
            fprintf(out, "\tjmp .or_end_%d\n", lbl);
            fprintf(out, ".or_true_%d:\n", lbl);
            fprintf(out, "\tpush 1\n");
            fprintf(out, ".or_end_%d:\n", lbl);
            break;
        }

        case L_ADD: {
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            emit_expr(node->firstChild->nextSibling, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rbx\n");
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tadd eax, ebx\n");
            fprintf(out, "\tpush rax\n");
            break;
        }

        case L_SUB: {
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            emit_expr(node->firstChild->nextSibling, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rbx\n");
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tsub eax, ebx\n");
            fprintf(out, "\tpush rax\n");
            break;
        }

        case L_MUL: {
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            emit_expr(node->firstChild->nextSibling, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rbx\n");
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\timul eax, ebx\n");
            fprintf(out, "\tpush rax\n");
            break;
        }

        case L_DIV: {
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            emit_expr(node->firstChild->nextSibling, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rbx\n");
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tcdq\n");         
            fprintf(out, "\tidiv ebx\n");
            fprintf(out, "\tpush rax\n");    /* quotient */
            break;
        }

        case L_MOD: {
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            emit_expr(node->firstChild->nextSibling, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rbx\n");
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tcdq\n");
            fprintf(out, "\tidiv ebx\n");
            fprintf(out, "\tpush rdx\n");    /* reste */
            break;
        }

        case L_LT: case L_GT: case L_EQ: case L_NEQ: case L_LE: case L_GE: {
            const char *setcc = NULL;
            switch (node->label) {
                case L_LT:  setcc = "l";  break;
                case L_GT:  setcc = "g";  break;
                case L_EQ:  setcc = "e";  break;
                case L_NEQ: setcc = "ne"; break;
                case L_LE:  setcc = "le"; break;
                case L_GE:  setcc = "ge"; break;
                default:    setcc = "e";  break;
            }
            emit_expr(node->firstChild, ctx_local, ctx_global, out);
            emit_expr(node->firstChild->nextSibling, ctx_local, ctx_global, out);
            fprintf(out, "\tpop rbx\n");
            fprintf(out, "\tpop rax\n");
            fprintf(out, "\tcmp eax, ebx\n");
            fprintf(out, "\tset%s al\n", setcc);
            fprintf(out, "\tmovzx eax, al\n");
            fprintf(out, "\tpush rax\n");
            break;
        }

        /* Appel de fonction utilisé comme expression (retourne une valeur) */
        case L_CALL: {
            Node *name_node = node->firstChild;
            const char *fname = name_node->value;

            /* Pousser les arguments dans l'ordre inverse pour placement en registres */
            /* D'abord les émettre sur la pile dans l'ordre, puis les charger */
            int argc = 0;
            Node *arg = name_node->nextSibling;
            while (arg) { argc++; arg = arg->nextSibling; }

            /* Émettre les arguments sur la pile (ordre normal) */
            arg = name_node->nextSibling;
            while (arg) {
                emit_expr(arg, ctx_local, ctx_global, out);
                arg = arg->nextSibling;
            }

            /* Convention AMD64 : arguments dans rdi, rsi, rdx, rcx, r8, r9 */
            const char *arg_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

            /* On dépile dans l'ordre inverse pour les charger dans rdi, rsi... */
            for (int i = argc - 1; i >= 0 && i < 6; i--)
                fprintf(out, "\tpop %s\n", arg_regs[i]);

            /* Appels builtins */
            if (strcmp(fname, "getint") == 0) {
                need_getint = 1;
                fprintf(out, "\tcall my_getint\n");
                fprintf(out, "\tpush rax\n");
            } else if (strcmp(fname, "getchar") == 0) {
                need_getchar = 1;
                fprintf(out, "\tcall my_getchar\n");
                fprintf(out, "\tpush rax\n");
            } else if (strcmp(fname, "putint") == 0) {
                need_putint = 1;
                fprintf(out, "\tcall my_putint\n");
            } else if (strcmp(fname, "putchar") == 0) {
                need_putchar = 1;
                fprintf(out, "\tcall my_putchar\n");
            } else {
                /* Aligner la pile */
                fprintf(out, "\tsub rsp, 8\n");
                fprintf(out, "\tcall %s\n", fname);
                fprintf(out, "\tadd rsp, 8\n");
                fprintf(out, "\tpush rax\n");   /* résultat  */
            }
            break;
        }

        default:
            fprintf(out, "\tpush 0\t; expr non gérée (label=%d)\n", node->label);
            break;
    }
}

static void emit_store_ident(const char *name, Table_symb *ctx_local, Table_symb *ctx_global, FILE *out) {
    for (Table_symb *t = ctx_local; t; t = t->suiv) {
        if (strcmp(t->ident, name) == 0 && t->kind == 'v') {
            if (strcmp(t->type, "char") == 0)
                fprintf(out, "\tmov byte [rbp-%d], al\n", t->offset);
            else
                fprintf(out, "\tmov dword [rbp-%d], eax\n", t->offset);
            return;
        }
    }
    for (Table_symb *t = ctx_global; t; t = t->suiv) {
        if (strcmp(t->ident, name) == 0 && t->kind == 'v') {
            if (strcmp(t->type, "char") == 0)
                fprintf(out, "\tmov byte [%s], al\n", name);
            else
                fprintf(out, "\tmov dword [%s], eax\n", name);
            return;
        }
    }
}

/* Vérifie si un nom est une fonction built-in qui ne peut pas être redéfinie */
static int is_builtin_function(const char *name) {
    return (strcmp(name, "getint") == 0 ||
            strcmp(name, "getchar") == 0 ||
            strcmp(name, "putint") == 0 ||
            strcmp(name, "putchar") == 0);
}

int analyse_semantique(Node *node, Table_symb **tableCourante, Table_symb **tableGlobale, FILE *anonym, int symbol, char *currentFctType) {
    if (!node) return 0;

    switch (node->label) {

        /* ── Racine du programme ── */
        case L_PROG: {
            Node *child = node->firstChild;
            while (child) {
                int r = analyse_semantique(child, tableCourante, tableGlobale, anonym, symbol, currentFctType);
                if (r != 0) return r;
                child = child->nextSibling;
            }
            return 0;
        }

        /* ── Déclaration de structure globale ── */
        case L_DECL_STRUCT: {
            Node *name_node = node->firstChild;
            if (!name_node) break;
            char *sname = name_node->value;

            Field *fields = NULL;
            Node *champ = name_node->nextSibling;
            while (champ && champ->label == L_CHAMP) {
                Node *type_node   = champ->firstChild;
                Node *declarateurs = type_node->nextSibling;

                char *ftype = NULL, *fsname = NULL;
                if (type_node->label == L_TYPE_INT)        ftype = "int";
                else if (type_node->label == L_TYPE_CHAR)  ftype = "char";
                else if (type_node->label == L_TYPE_STRUCT) {
                    ftype  = "struct";
                    fsname = type_node->value;
                }

                Node *decl = declarateurs;
                while (decl && decl->label == L_IDENT) {
                    add_field(&fields, make_field(decl->value, ftype ? ftype : "int", fsname));
                    decl = decl->nextSibling;
                }
                champ = champ->nextSibling;
            }
            add_struct(sname, fields);
            break;
        }

        /* ── Déclaration de variable ── */
        case L_DECL_VAR: {
            Node *typeNode = node->firstChild;
            if (!typeNode) break;

            char *typeStr = NULL, *sname = NULL;
            if (typeNode->label == L_TYPE_INT)        typeStr = "int";
            else if (typeNode->label == L_TYPE_CHAR)  typeStr = "char";
            else if (typeNode->label == L_TYPE_STRUCT) {
                typeStr = "struct";
                sname   = typeNode->value;
                if (!find_struct(sname)) {
                    fprintf(stderr, "Erreur sémantique ligne %d : structure '%s' non définie.\n", node->lineno, sname);
                    return 2;
                }
            }

            /* Contexte global : tableCourante est la table globale */
            int is_global = (tableGlobale == NULL || *tableGlobale == NULL);

            Node *varNode = typeNode->nextSibling;
            while (varNode) {

                if (!is_global && tableGlobale && *tableGlobale) {
                    Table_symb *global_entry = findEntry(varNode->value, NULL, *tableGlobale);
                    if (global_entry && global_entry->kind == 'v') {
                        fprintf(stderr, "Warning ligne %d : variable locale '%s' masque une variable globale du même nom.\n", 
                                node->lineno, varNode->value);
                    }
                }
                
                if (is_global && is_builtin_function(varNode->value)) {
                    fprintf(stderr, "Erreur sémantique ligne %d : '%s' est une fonction built-in, impossible de déclarer une variable avec ce nom.\n", 
                            node->lineno, varNode->value);
                    return 2;
                }

                /* Conflit avec une fonction déjà déclarée (global only) */
                if (is_global && isInTableWithKind(varNode->value, *tableCourante, 'f')) {
                    fprintf(stderr, "Erreur sémantique ligne %d : '%s' est déjà le nom d'une fonction.\n", node->lineno, varNode->value);
                    return 2;
                }

                int added;
                if (sname)
                    added = add_struct_var(tableCourante, typeStr, sname, varNode->value, 'v');
                else
                    added = add(tableCourante, typeStr, varNode->value, 'v');

                if (!added) {
                    fprintf(stderr, "Erreur sémantique ligne %d : '%s' déjà déclaré.\n", node->lineno, varNode->value);
                    return 2;
                }

                /* Calcul de l'offset pour les variables locales. */
                if (!is_global) {
                    int sz = (sname) ? struct_size(sname) : (strcmp(typeStr, "char") == 0 ? 1 : 4);
                    /* Aligner sur 4 octets au minimum */
                    if (sz < 4) sz = 4;

                    /* L'offset de la nouvelle variable = max(offset actuel) + sz.
                       On parcourt la table locale pour trouver l'offset max. */
                    int max_off = 0;
                    for (Table_symb *t = *tableCourante; t; t = t->suiv)
                        if (t->offset > max_off) max_off = t->offset;

                    /* Trouver la nouvelle entrée (la dernière) */
                    Table_symb *last = *tableCourante;
                    while (last->suiv) last = last->suiv;
                    last->offset = max_off + sz;
                }
                varNode = varNode->nextSibling;
            }
            break;
        }

        /* ── Déclaration de fonction ── */
        case L_DECL_FONCT: {
            Node *entete    = node->firstChild;
            Node *corps     = entete->nextSibling;
            Node *typeRetour = entete->firstChild;
            Node *nomFonct   = typeRetour->nextSibling;
            Node *params     = nomFonct->nextSibling;

            /* Conflits */
            if (is_builtin_function(nomFonct->value)) {
                fprintf(stderr, "Erreur sémantique ligne %d : redéfinition de la fonction built-in '%s' interdite.\n", 
                        node->lineno, nomFonct->value);
                return 2;
            }
            if (isInTableWithKind(nomFonct->value, *tableCourante, 'v')) {
                fprintf(stderr, "Erreur sémantique ligne %d : '%s' est déjà le nom d'une variable globale.\n", node->lineno, nomFonct->value);
                return 2;
            }
            if (isInTableWithKind(nomFonct->value, *tableCourante, 'f')) {
                fprintf(stderr, "Erreur sémantique ligne %d : fonction '%s' déjà déclarée.\n", node->lineno, nomFonct->value);
                return 2;
            }

            /* Type de retour */
            char *ret_type = NULL, *ret_sname = NULL;
            if      (typeRetour->label == L_TYPE_INT)    ret_type = "int";
            else if (typeRetour->label == L_TYPE_CHAR)   ret_type = "char";
            else if (typeRetour->label == L_TYPE_VOID)   ret_type = "void";
            else if (typeRetour->label == L_TYPE_STRUCT) { ret_type = "struct"; ret_sname = typeRetour->value; }

            /* Enregistrement dans la table globale */
            if (ret_sname)
                add_struct_var(tableCourante, ret_type, ret_sname, nomFonct->value, 'f');
            else
                add(tableCourante, ret_type, nomFonct->value, 'f');

            /* ── Construction de la table locale : paramètres ── */
            Table_symb *tableLocale = NULL;

            /* Registres 64-bit et 32-bit pour les paramètres  */
            const char *param_regs64[] = { "rdi", "rsi", "rdx", "rcx", "r8",  "r9"  };
            const char *param_regs32[] = { "edi", "esi", "edx", "ecx", "r8d", "r9d" };
            const char *param_regs8[]  = { "dil", "sil", "dl",  "cl",  "r8b", "r9b" };
            int param_idx = 0;

            Node *param = params->firstChild;
            while (param) {
                Node *typeParam = param->firstChild;
                Node *nomParam  = typeParam->nextSibling;

                char *ptype = NULL, *psname = NULL;
                if      (typeParam->label == L_TYPE_INT)    ptype = "int";
                else if (typeParam->label == L_TYPE_CHAR)   ptype = "char";
                else if (typeParam->label == L_TYPE_STRUCT) { ptype = "struct"; psname = typeParam->value; }

                int sz = psname ? struct_size(psname) : (strcmp(ptype ? ptype : "int", "char") == 0 ? 1 : 4);
                if (sz < 4) sz = 4; /* aligner sur 4 */

                /* Offset cumulé */
                int max_off = 0;
                for (Table_symb *t = tableLocale; t; t = t->suiv)
                    if (t->offset > max_off) max_off = t->offset;

                if (psname)
                    add_struct_var(&tableLocale, ptype, psname, nomParam->value, 'v');
                else
                    add(&tableLocale, ptype ? ptype : "int", nomParam->value, 'v');

                /* Assigner l'offset à la nouvelle entrée */
                Table_symb *last = tableLocale;
                while (last->suiv) last = last->suiv;
                last->offset = max_off + sz;

                param = param->nextSibling;
                param_idx++;
            }

            /* ── Prologue ASM ── */
            if (strcmp(nomFonct->value, "main") == 0) {
                fprintf(anonym, "\nglobal main\n");
                fprintf(anonym, "global _start\n");
                fprintf(anonym, "section .text\n");
                /* _start → appelle main puis exit(rax) */
                fprintf(anonym, "\n_start:\n");
                fprintf(anonym, "\tcall main\n");
                fprintf(anonym, "\tmov edi, eax\n");
                fprintf(anonym, "\tmov eax, 60\n");
                fprintf(anonym, "\tsyscall\n");
            }
            fprintf(anonym, "\n%s:\n", nomFonct->value);
            fprintf(anonym, "\tpush rbp\n");
            fprintf(anonym, "\tmov rbp, rsp\n");

            /* Position dans le fichier pour patcher sub rsp */
            long patch_pos = ftell(anonym);
            fprintf(anonym, "\tsub rsp, 00000000h\t\n");

            /* Sauvegarder les arguments (registres → pile locale) */
            param = params->firstChild;
            param_idx = 0;
            for (Table_symb *t = tableLocale; t; t = t->suiv) {
                if (param_idx >= 6) break;
                /* Stocker le registre argument dans la zone locale */
                if (strcmp(t->type, "char") == 0)
                    fprintf(anonym, "\tmov byte [rbp-%d], %s\n", t->offset, param_regs8[param_idx]);
                else
                    fprintf(anonym, "\tmov dword [rbp-%d], %s\n", t->offset, param_regs32[param_idx]);
                param_idx++;
            }

            /* ── Analyse du corps ── */
            int ret = analyse_semantique(corps, &tableLocale, tableCourante, anonym, symbol, ret_type);
            if (ret != 0) {
                freeTable(tableLocale);
                return ret;
            }

            /* Calculer l'espace pile réel nécessaire (offset max, aligné 16) */
            int max_off = 0;
            for (Table_symb *t = tableLocale; t; t = t->suiv)
                if (t->offset > max_off) max_off = t->offset;
            /* Arrondir à un multiple de 16  */
            int frame_size = (max_off + 15) & ~15;
            if (frame_size == 0) frame_size = 16; /* au moins 16 */

            /* Patcher le sub rsp */
            long cur_pos = ftell(anonym);
            fseek(anonym, patch_pos, SEEK_SET);
            fprintf(anonym, "\tsub rsp, %-8d\t", frame_size);
            fseek(anonym, cur_pos, SEEK_SET);

            /* ── Épilogue ASM ── */
            fprintf(anonym, ".%s_epilogue:\n", nomFonct->value);
            fprintf(anonym, "\tmov rsp, rbp\n");
            fprintf(anonym, "\tpop rbp\n");
            fprintf(anonym, "\tret\n");

            /* Affichage table des symboles */
            if (symbol) {
                printf("\n--- Table des symboles (locals+params) pour '%s' ---\n", nomFonct->value);
                printT(tableLocale);
            }

            freeTable(tableLocale);
            break;
        }

        /* ── Corps de fonction (liste de déclarations locales + instructions) ── */
        case L_CORPS: {
            Node *child = node->firstChild;
            while (child) {
                int r = analyse_semantique(child, tableCourante, tableGlobale, anonym, symbol, currentFctType);
                if (r != 0) return r;
                child = child->nextSibling;
            }
            break;
        }

        /* ── Bloc { SuiteInstr } ── */
        case L_BLOCK: {
            Node *child = node->firstChild;
            while (child) {
                int r = analyse_semantique(child, tableCourante, tableGlobale, anonym, symbol, currentFctType);
                if (r != 0) return r;
                child = child->nextSibling;
            }
            break;
        }

        /* ── Affectation simple : ident = expr ── */
        case L_ASSIGN: {
            Node *lhs = node->firstChild;
            Node *rhs = lhs->nextSibling;

            /* Vérifier que la variable est déclarée */
            if (!isInAnyTable(lhs->value, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL)) {
                fprintf(stderr, "Erreur sémantique ligne %d : variable '%s' non déclarée.\n", node->lineno, lhs->value);
                return 2;
            }

            /* Vérifier que le côté droit n'est pas void */
            int r = checkNotVoidExpr(rhs, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, node->lineno);
            if (r) return r;

            /* Vérifier les types */
            TypeInfo *tdest = getExprType(lhs, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL);
            TypeInfo *tsrc  = getExprType(rhs, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL);

            if (tdest && tsrc) {
                r = checkAssignType(tdest->base_type, tdest->struct_name, tsrc->base_type,  tsrc->struct_name, node->lineno);
            }
            if (tdest) free_type_info(tdest);
            if (tsrc)  free_type_info(tsrc);
            if (r) return r;

            /* Analyse sémantique récursive (pour les appels imbriqués) */
            r = analyse_semantique(rhs, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;

            /* Génération de code */
            emit_expr(rhs, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, anonym);
            fprintf(anonym, "\tpop rax\n");
            emit_store_ident(lhs->value, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, anonym);
            break;
        }

        /* ── Affectation de champ : expr.champ = expr ── */
        case L_FIELD_ASSIGN: {
            Node *lhs = node->firstChild;  /* L_FIELD_ACCESS */
            Node *rhs = lhs->nextSibling;

            /* Vérif sémantique du type dest */
            TypeInfo *tdest = getExprType(lhs, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL);
            if (!tdest) {
                fprintf(stderr,
                    "Erreur sémantique ligne %d : accès de champ invalide.\n", node->lineno);
                return 2;
            }

            int r = checkNotVoidExpr(rhs, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, node->lineno);
            if (r) { free_type_info(tdest); return r; }

            TypeInfo *tsrc = getExprType(rhs, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL);

            if (tdest && tsrc) {
                r = checkAssignType(tdest->base_type, tdest->struct_name, tsrc->base_type,  tsrc->struct_name, node->lineno);
            }
            if (tdest) free_type_info(tdest);
            if (tsrc)  free_type_info(tsrc);
            if (r) return r;

            break;
        }

        /* ── Appel de fonction (instruction) ── */
        case L_CALL: {
            Node *nom = node->firstChild;

            if (!isInAnyTable(nom->value, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL)) {
                fprintf(stderr, "Erreur sémantique ligne %d : fonction '%s' non déclarée.\n", node->lineno, nom->value);
                return 2;
            }

            /* Vérifier les arguments */
            Node *arg = nom->nextSibling;
            while (arg) {
                int r = checkNotVoidExpr(arg, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, arg->lineno);
                if (r) return r;
                arg = arg->nextSibling;
            }

            /* Génération du code  */
            emit_expr(node, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, anonym);

            /* Si c'est un appel non-void, on a pushé un résultat → le retirer */
            Table_symb *fe = findEntry(nom->value, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL);
            if (fe && strcmp(fe->type, "void") != 0)
                fprintf(anonym, "\tadd rsp, 8\t; discard return value\n");
            break;
        }

        /* ── if sans else ── */
        case L_IF: {
            Node *cond  = node->firstChild;
            Node *corps = cond->nextSibling;
            int lbl = label_count++;

            int r = checkNotVoidExpr(cond, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, node->lineno);
            if (r) return r;

            r = analyse_semantique(cond, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;

            emit_expr(cond, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tje .fin_if_%d\n", lbl);

            r = analyse_semantique(corps, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;

            fprintf(anonym, ".fin_if_%d:\n", lbl);
            break;
        }

        /* ── if / else ── */
        case L_IF_ELSE: {
            Node *cond       = node->firstChild;
            Node *corps_if   = cond->nextSibling;
            Node *corps_else = corps_if->nextSibling;
            int lbl = label_count++;

            int r = checkNotVoidExpr(cond, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, node->lineno);
            if (r) return r;

            r = analyse_semantique(cond, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;

            emit_expr(cond, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tje .sinon_%d\n", lbl);

            r = analyse_semantique(corps_if, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;
            fprintf(anonym, "\tjmp .fin_si_%d\n", lbl);

            fprintf(anonym, ".sinon_%d:\n", lbl);
            r = analyse_semantique(corps_else, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;

            fprintf(anonym, ".fin_si_%d:\n", lbl);
            break;
        }

        /* ── while ── */
        case L_WHILE: {
            Node *cond  = node->firstChild;
            Node *corps = cond->nextSibling;
            int lbl = label_count++;

            int r = checkNotVoidExpr(cond, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, node->lineno);
            if (r) return r;

            r = analyse_semantique(cond, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;

            fprintf(anonym, ".debut_while_%d:\n", lbl);

            emit_expr(cond, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, anonym);
            fprintf(anonym, "\tpop rax\n");
            fprintf(anonym, "\tcmp eax, 0\n");
            fprintf(anonym, "\tje .fin_while_%d\n", lbl);

            r = analyse_semantique(corps, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;

            fprintf(anonym, "\tjmp .debut_while_%d\n", lbl);
            fprintf(anonym, ".fin_while_%d:\n", lbl);
            break;
        }

        /* ── return ── */
        case L_RETURN: {
            Node *expr = node->firstChild;

            if (!expr) {
                /* return sans valeur */
                if (currentFctType && strcmp(currentFctType, "void") != 0) {
                    fprintf(stderr, "Erreur sémantique ligne %d : return sans valeur dans une fonction '%s'.\n", node->lineno, currentFctType);
                    return 2;
                }
                /* Aller à l'épilogue */
                /* On ne connaît pas le nom de la fonction ici, on émet ret directement */
                fprintf(anonym, "\tmov rsp, rbp\n");
                fprintf(anonym, "\tpop rbp\n");
                fprintf(anonym, "\tret\n");
                break;
            }

            /* return avec valeur dans une fonction void */
            if (currentFctType && strcmp(currentFctType, "void") == 0) {
                fprintf(stderr, "Erreur sémantique ligne %d : return avec valeur dans une fonction void.\n", node->lineno);
                return 2;
            }

            /* Vérification : l'expression retournée n'est pas void */
            int r = checkNotVoidExpr(expr, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, node->lineno);
            if (r) return r;

            /* Vérification du type de retour */
            TypeInfo *texpr = getExprType(expr, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL);
            if (texpr && currentFctType) {
                r = checkAssignType(currentFctType, NULL, texpr->base_type, texpr->struct_name, node->lineno);
                if (r == 2) {
                    free_type_info(texpr);
                    return r;
                }
            }
            if (texpr) free_type_info(texpr);

            /* Génération : evaluer l'expression → résultat dans rax */
            r = analyse_semantique(expr, tableCourante, tableGlobale, anonym, symbol, currentFctType);
            if (r) return r;

            emit_expr(expr, tableCourante ? *tableCourante : NULL, tableGlobale  ? *tableGlobale  : NULL, anonym);
            fprintf(anonym, "\tpop rax\n");
            /* Épilogue inline pour le return */
            fprintf(anonym, "\tmov rsp, rbp\n");
            fprintf(anonym, "\tpop rbp\n");
            fprintf(anonym, "\tret\n");
            break;
        }

        /* ── Nœuds à traverser sans action particulière ── */
        default: {
            Node *child = node->firstChild;
            while (child) {
                int r = analyse_semantique(child, tableCourante, tableGlobale, anonym, symbol, currentFctType);
                if (r != 0) return r;
                child = child->nextSibling;
            }
            break;
        }
    }

    return 0;
}

int haveCorrectMain(Table_symb **tableCourant) {
    if (!tableCourant) return 0;
    for (Table_symb *cur = *tableCourant; cur; cur = cur->suiv)
        if (strcmp(cur->ident, "main") == 0 && strcmp(cur->type,  "int")  == 0 && cur->kind == 'f')
            return 1;
    return 0;
}

void generer_bss(FILE *anonym, Table_symb *tableGlobale) {
    int has_globals = 0;
    for (Table_symb *t = tableGlobale; t; t = t->suiv)
        if (t->kind == 'v') { has_globals = 1; break; }
    if (!has_globals) return;

    fprintf(anonym, "\nsection .bss\n");
    for (Table_symb *t = tableGlobale; t; t = t->suiv) {
        if (t->kind != 'v') continue;
        if (strcmp(t->type, "int") == 0)
            fprintf(anonym, "%s resd 1\n", t->ident);
        else if (strcmp(t->type, "char") == 0)
            fprintf(anonym, "%s resb 1\n", t->ident);
        else if (strcmp(t->type, "struct") == 0 && t->struct_name) {
            int sz = struct_size(t->struct_name);
            if (sz > 0)
                fprintf(anonym, "%s resb %d\n", t->ident, sz);
        }
    }
}

void generer_footer_asm(FILE *anonym) {

    /* ── my_putint(rdi) : affiche un entier signé suivi d'un '\n' ── */
    if (need_putint) {
        fprintf(anonym, "\nmy_putint:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tsub rsp, 32\n");
        fprintf(anonym, "\tpush rbx\n");
        fprintf(anonym, "\tmov eax, edi\n");          /* paramètre dans edi (convention AMD64) */
        /* Cas zéro */
        fprintf(anonym, "\tcmp eax, 0\n");
        fprintf(anonym, "\tjne .pi_nonzero\n");
        fprintf(anonym, "\tmov byte [rbp-32], '0'\n");
        fprintf(anonym, "\tmov byte [rbp-31], 10\n"); /* '\n' */
        fprintf(anonym, "\tmov eax, 1\n");
        fprintf(anonym, "\tmov edi, 1\n");
        fprintf(anonym, "\tlea rsi, [rbp-32]\n");
        fprintf(anonym, "\tmov edx, 2\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tjmp .pi_done\n");
        fprintf(anonym, ".pi_nonzero:\n");
        fprintf(anonym, "\txor r8d, r8d\n");
        fprintf(anonym, "\tcmp eax, 0\n");
        fprintf(anonym, "\tjge .pi_pos\n");
        fprintf(anonym, "\tmov r8d, 1\n");
        fprintf(anonym, "\tneg eax\n");
        fprintf(anonym, ".pi_pos:\n");
        fprintf(anonym, "\tlea r9, [rbp-2]\n");       /* laisser place pour '\n' à [rbp-1] */
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
        fprintf(anonym, "\tmov byte [r9+rcx], 10\n"); /* '\n' après les chiffres */
        fprintf(anonym, "\tinc ecx\n");
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

    /* ── my_putchar(rdi) : affiche un caractère ── */
    if (need_putchar) {
        fprintf(anonym, "\nmy_putchar:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tsub rsp, 8\n");
        fprintf(anonym, "\tmov [rbp-1], dil\n");      /* dil = octet bas de rdi */
        fprintf(anonym, "\tmov eax, 1\n");
        fprintf(anonym, "\tmov edi, 1\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmov rsp, rbp\n");
        fprintf(anonym, "\tpop rbp\n");
        fprintf(anonym, "\tret\n");
    }

    /* ── my_getint() → rax : lit un entier signé suivi de '\n' ── */
    if (need_getint) {
        fprintf(anonym, "\nmy_getint:\n");
        fprintf(anonym, "\tpush rbp\n");
        fprintf(anonym, "\tmov rbp, rsp\n");
        fprintf(anonym, "\tsub rsp, 16\n");
        fprintf(anonym, "\txor r12d, r12d\n");          /* accumulateur */
        
        /* Lire premier caractère */
        fprintf(anonym, "\tmov eax, 0\n");
        fprintf(anonym, "\tmov edi, 0\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmovzx ebx, byte [rbp-1]\n");
        
        /* Gestion du signe */
        fprintf(anonym, "\txor r13d, r13d\n");          /* 0 = positif, 1 = négatif */
        fprintf(anonym, "\tcmp ebx, '-'\n");
        fprintf(anonym, "\tje .gi_handle_minus\n");
        fprintf(anonym, "\tcmp ebx, '+'\n");
        fprintf(anonym, "\tje .gi_handle_plus\n");
        fprintf(anonym, "\tjmp .gi_check_digit\n");
        
        /* Cas du signe - */
        fprintf(anonym, ".gi_handle_minus:\n");
        fprintf(anonym, "\tmov r13d, 1\n");
        fprintf(anonym, "\tjmp .gi_read_next\n");
        
        /* Cas du signe + */
        fprintf(anonym, ".gi_handle_plus:\n");
        fprintf(anonym, "\tmov r13d, 0\n");             /* signe positif explicite */
        
        /* Lire le caractère après le signe */
        fprintf(anonym, ".gi_read_next:\n");
        fprintf(anonym, "\tmov eax, 0\n");
        fprintf(anonym, "\tmov edi, 0\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmovzx ebx, byte [rbp-1]\n");
        
        /* Vérifier que c'est un chiffre */
        fprintf(anonym, ".gi_check_digit:\n");
        fprintf(anonym, "\tcmp ebx, '0'\n");
        fprintf(anonym, "\tjl .gi_error\n");
        fprintf(anonym, "\tcmp ebx, '9'\n");
        fprintf(anonym, "\tjg .gi_error\n");
        
        /* Boucle de lecture des chiffres */
        fprintf(anonym, ".gi_loop:\n");
        fprintf(anonym, "\tsub ebx, '0'\n");
        fprintf(anonym, "\timul r12d, r12d, 10\n");
        fprintf(anonym, "\tadd r12d, ebx\n");
        
        /* Lire le caractère suivant */
        fprintf(anonym, "\tmov eax, 0\n");
        fprintf(anonym, "\tmov edi, 0\n");
        fprintf(anonym, "\tlea rsi, [rbp-1]\n");
        fprintf(anonym, "\tmov edx, 1\n");
        fprintf(anonym, "\tsyscall\n");
        fprintf(anonym, "\tmovzx ebx, byte [rbp-1]\n");
        
        /* Si c'est encore un chiffre, continuer */
        fprintf(anonym, "\tcmp ebx, '0'\n");
        fprintf(anonym, "\tjl .gi_done\n");
        fprintf(anonym, "\tcmp ebx, '9'\n");
        fprintf(anonym, "\tjg .gi_done\n");
        fprintf(anonym, "\tjmp .gi_loop\n");
        
        /* Fin de la lecture : doit se terminer par '\n' */
        fprintf(anonym, ".gi_done:\n");
        fprintf(anonym, "\tcmp ebx, 10\n");             /* 10 = '\n' */
        fprintf(anonym, "\tjne .gi_error\n");
        
        /* Appliquer le signe et retourner */
        fprintf(anonym, "\tmov eax, r12d\n");
        fprintf(anonym, "\tcmp r13d, 0\n");
        fprintf(anonym, "\tje .gi_return\n");
        fprintf(anonym, "\tneg eax\n");
        fprintf(anonym, ".gi_return:\n");
        fprintf(anonym, "\tmov rsp, rbp\n");
        fprintf(anonym, "\tpop rbp\n");
        fprintf(anonym, "\tret\n");
        
        /* Erreur : quitter avec code 5 */
        fprintf(anonym, ".gi_error:\n");
        fprintf(anonym, "\tmov eax, 60\n");             /* syscall exit */
        fprintf(anonym, "\tmov edi, 5\n");              /* code retour 5 */
        fprintf(anonym, "\tsyscall\n");
    }

    /* ── my_getchar() → rax : lit un caractère ── */
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
