#ifndef STRUCT_TABLE_H
#define STRUCT_TABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structure pour représenter un champ d'une structure
typedef struct Field {
    char *name;
    char *type;
    char *struct_name;
    struct Field *next;
} Field;

// Structure pour représenter une définition de structure
typedef struct StructDef {
    char *name;
    Field *fields;
    struct StructDef *next;
} StructDef;

// TypeInfo pour getExprType
typedef struct TypeInfo {
    char *base_type;
    char *struct_name;
} TypeInfo;

// Prototypes pour la gestion des structures
void init_struct_table(void);
StructDef* find_struct(const char *name);
int add_struct(const char *name, Field *fields);
Field* find_field(StructDef *s, const char *field_name);
void free_struct_table(void);
void print_struct_table(void);

// Prototypes pour les champs
Field* make_field(const char *name, const char *type, const char *struct_name);
void add_field(Field **head, Field *new_field);
void free_fields(Field *f);

// Prototypes pour TypeInfo
TypeInfo* make_type_info(const char *base_type, const char *struct_name);
void free_type_info(TypeInfo *ti);
char* type_info_to_string(TypeInfo *ti);

#endif