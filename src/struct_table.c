#include "struct_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static StructDef *struct_table = NULL;

void init_struct_table(void) {
    struct_table = NULL;
}

StructDef* find_struct(const char *name) {
    StructDef *curr = struct_table;
    while (curr) {
        if (strcmp(curr->name, name) == 0)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

int add_struct(const char *name, Field *fields) {
    if (find_struct(name) != NULL) {
        fprintf(stderr, "Erreur sémantique : structure '%s' déjà définie\n", name);
        return 0;
    }
    StructDef *new = malloc(sizeof(StructDef));
    if (!new) return 0;
    new->name = strdup(name);
    new->fields = fields;
    new->next = struct_table;
    struct_table = new;
    return 1;
}

Field* find_field(StructDef *s, const char *field_name) {
    Field *curr = s->fields;
    while (curr) {
        if (strcmp(curr->name, field_name) == 0)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// Créer un champ
Field* make_field(const char *name, const char *type, const char *struct_name) {
    Field *f = malloc(sizeof(Field));
    f->name = strdup(name);
    f->type = strdup(type);
    f->struct_name = struct_name ? strdup(struct_name) : NULL;
    f->next = NULL;
    return f;
}

// Ajouter un champ à la fin de la liste
void add_field(Field **head, Field *new_field) {
    if (*head == NULL) {
        *head = new_field;
        return;
    }
    Field *curr = *head;
    while (curr->next) curr = curr->next;
    curr->next = new_field;
}

// Libérer une liste de champs
void free_fields(Field *f) {
    while (f) {
        Field *next = f->next;
        free(f->name);
        free(f->type);
        if (f->struct_name) free(f->struct_name);
        free(f);
        f = next;
    }
}

void free_struct_table(void) {
    StructDef *curr = struct_table;
    while (curr) {
        StructDef *next = curr->next;
        free(curr->name);
        free_fields(curr->fields);
        free(curr);
        curr = next;
    }
    struct_table = NULL;
}

void print_struct_table(void) {
    printf("\n=== Tables des structures ===\n");
    for (StructDef *s = struct_table; s; s = s->next) {
        printf("struct %s {\n", s->name);
        for (Field *f = s->fields; f; f = f->next) {
            printf("  %s", f->type);
            if (strcmp(f->type, "struct") == 0 && f->struct_name)
                printf(" %s", f->struct_name);
            printf(" %s;\n", f->name);
        }
        printf("}\n");
    }
}