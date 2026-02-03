%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"

extern int yylex(void);
extern char *yytext;
extern int lineno;
Node *root = NULL;

void yyerror(const char *s){
    fprintf(stderr,"Erreur syntaxique ligne %d pret de %s : %s\n", lineno, yytext, s);
}
%}

%union {
    int num;
    char character;
    char* ident;
    Node* node;
}

%token <ident> IDENT
%token <ident> TYPE  
%token <ident> EQ  
%token <ident> ORDER     
%token <ident> ADDSUB     
%token <ident> DIVSTAR    
%token <ident> OR          
%token <ident> AND
%token <num> NUM
%token <character> CHARACTER

%token STRUCT VOID IF ELSE WHILE RETURN LEX_ERROR

%type <node> Prog DeclGlobales DeclGlobale DeclFoncts DeclFonct
%type <node> Corps DeclLocales DeclLocale
%type <node> Champs Champ Type Declarateurs
%type <node> EnTeteFonct Parametres ListTypVar
%type <node> SuiteInstr Instr
%type <node> Exp TB FB M E T F
%type <node> Var Arguments ListExp

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

Prog:  
     DeclGlobales DeclFoncts
     { 
         $$ = makeNode(L_PROG);
         if ($1) siblingsToChildren($$, $1);
         if ($2) siblingsToChildren($$, $2);
         root = $$;
     }
    ;

DeclGlobales:
       DeclGlobales DeclGlobale
       {
           if ($1 == NULL) {
               $$ = $2;
           } else {
               addSibling($1, $2);
               $$ = $1;
           }
       }
    |  { $$ = NULL; }
    ;

DeclGlobale:
       STRUCT IDENT '{' Champs '}' ';'
       {
           $$ = makeNode(L_DECL_STRUCT);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($2);
           addChild($$, name);
           if ($4) siblingsToChildren($$, $4);
       }
    |  TYPE Declarateurs ';'
       {
           $$ = makeNode(L_DECL_VAR);
           Node * type;
           if (strcmp($1, "int") == 0) {
               type = makeNode(L_TYPE_INT);
           } else {
               type = makeNode(L_TYPE_CHAR);
           }
           addChild($$, type); 
           if ($2) siblingsToChildren($$, $2);
       }
    |  STRUCT IDENT Declarateurs ';'
       {
           $$ = makeNode(L_DECL_VAR);
           Node *type = makeNode(L_TYPE_STRUCT);
           type->value = strdup($2);
           addChild($$, type);
           if ($3) siblingsToChildren($$, $3);
       }
    ;

Champs:
      Champ Champs
      {
          if ($2 == NULL) {
              $$ = $1;
          } else {
              addSibling($1, $2);
              $$ = $1;
          }
      }
    | Champ { $$ = $1; }
    ;

Champ:
      Type Declarateurs ';'
      {
          $$ = makeNode(L_CHAMP);
          addChild($$, $1);
          if ($2) siblingsToChildren($$, $2);
      }
    ;

Type:
      TYPE
      {
          if (strcmp($1, "int") == 0) {
              $$ = makeNode(L_TYPE_INT);
          } else {
              $$ = makeNode(L_TYPE_CHAR);
          }
      }
    | STRUCT IDENT
      {
          $$ = makeNode(L_TYPE_STRUCT);
          $$->value = strdup($2);
      }
    ;
    
Declarateurs:
       Declarateurs ',' IDENT
       {
           Node *ident = makeNode(L_IDENT);
           ident->value = strdup($3);
           if ($1 == NULL) {
               $$ = ident;
           } else {
               addSibling($1, ident);
               $$ = $1;
           }
       }
    |  IDENT
       {
           $$ = makeNode(L_IDENT);
           $$->value = strdup($1);
       }
    ;
    
DeclFoncts:
       DeclFoncts DeclFonct
       {
           if ($1 == NULL) {
               $$ = $2;
           } else {
               addSibling($1, $2);
               $$ = $1;
           }
       }
    |  DeclFonct { $$ = $1; }
    ;
    
DeclFonct:
       EnTeteFonct Corps
       {
           $$ = makeNode(L_DECL_FONCT);
           addChild($$, $1);
           addChild($$, $2);
       }
    ;
    
EnTeteFonct:
       TYPE IDENT '(' Parametres ')'
       {
           $$ = makeNode(L_ENTETE_FONCT);
           Node *retType;
           if (strcmp($1, "int") == 0) {
               retType = makeNode(L_TYPE_INT);
           } else {
               retType = makeNode(L_TYPE_CHAR);
           }
           addChild($$, retType);
           
           Node *name = makeNode(L_IDENT);
           name->value = strdup($2);
           addChild($$, name);
           addChild($$, $4);
       }
    |  VOID IDENT '(' Parametres ')'
       {
           $$ = makeNode(L_ENTETE_FONCT);
           Node *type = makeNode(L_TYPE_VOID);
           addChild($$, type);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($2);
           addChild($$, name);
           addChild($$, $4);
       }
    |  STRUCT IDENT IDENT '(' Parametres ')'
       {
           $$ = makeNode(L_ENTETE_FONCT);
           Node *type = makeNode(L_TYPE_STRUCT);
           type->value = strdup($2);
           addChild($$, type);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($3);
           addChild($$, name);
           addChild($$, $5);
       }
    ;
    
Parametres:
       VOID
       {
           $$ = makeNode(L_PARAMETRES);
       }
    |  ListTypVar
       {
           $$ = makeNode(L_PARAMETRES);
           if ($1) siblingsToChildren($$, $1);
       }
    |  
       {
           $$ = makeNode(L_PARAMETRES);
       }
    ;
    
ListTypVar:
       ListTypVar ',' TYPE IDENT
       {
           Node *param = makeNode(L_PARAM);
           Node *type;
           if (strcmp($3, "int") == 0) {
               type = makeNode(L_TYPE_INT);
           } else {
               type = makeNode(L_TYPE_CHAR);
           }
           addChild(param, type);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($4);
           addChild(param, name);
           
           if ($1 == NULL) {
               $$ = param;
           } else {
               addSibling($1, param);
               $$ = $1;
           }
       }
    |  ListTypVar ',' STRUCT IDENT IDENT
       {
           Node *param = makeNode(L_PARAM);
           Node *type = makeNode(L_TYPE_STRUCT);
           type->value = strdup($4);
           addChild(param, type);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($5);
           addChild(param, name);
           
           if ($1 == NULL) {
               $$ = param;
           } else {
               addSibling($1, param);
               $$ = $1;
           }
       }
    |  TYPE IDENT
       {
           $$ = makeNode(L_PARAM);
           Node *type;
           if (strcmp($1, "int") == 0) {
               type = makeNode(L_TYPE_INT);
           } else {
               type = makeNode(L_TYPE_CHAR);
           }
           addChild($$, type);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($2);
           addChild($$, name);
       }
    |  STRUCT IDENT IDENT
       {
           $$ = makeNode(L_PARAM);
           Node *type = makeNode(L_TYPE_STRUCT);
           type->value = strdup($2);
           addChild($$, type);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($3);
           addChild($$, name);
       }
    ;
    
Corps: '{' DeclLocales SuiteInstr '}'
    {
        $$ = makeNode(L_CORPS);
        if ($2) siblingsToChildren($$, $2);
        if ($3) siblingsToChildren($$, $3);
    }
    ;
    
DeclLocales:
       DeclLocales DeclLocale
       {
           if ($1 == NULL) {
               $$ = $2;
           } else {
               addSibling($1, $2);
               $$ = $1;
           }
       }
    |  { $$ = NULL; }
    ;

DeclLocale:
       STRUCT IDENT '{' Champs '}' ';'
       {
           $$ = makeNode(L_DECL_STRUCT);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($2);
           addChild($$, name);
           if ($4) siblingsToChildren($$, $4);
       }
    |  TYPE Declarateurs ';'
       {
           $$ = makeNode(L_DECL_VAR);
           Node *type;
           if (strcmp($1, "int") == 0) {
               type = makeNode(L_TYPE_INT);
           } else {
               type = makeNode(L_TYPE_CHAR);
           }
           addChild($$, type);
           if ($2) siblingsToChildren($$, $2);
       }
    |  STRUCT IDENT Declarateurs ';'
       {
           $$ = makeNode(L_DECL_VAR);
           Node *type = makeNode(L_TYPE_STRUCT);
           type->value = strdup($2);
           addChild($$, type);
           if ($3) siblingsToChildren($$, $3);
       }
    ;

SuiteInstr:
       SuiteInstr Instr
       {
           if ($1 == NULL) {
               $$ = $2;
           } else {
               addSibling($1, $2);
               $$ = $1;
           }
       }
    |  { $$ = NULL; }
    ;
    
Instr:
       IDENT '=' Exp ';'
       {
           $$ = makeNode(L_ASSIGN);
           Node *ident = makeNode(L_IDENT);
           ident->value = strdup($1);
           addChild($$, ident);
           addChild($$, $3);
       }
    |  IDENT '.' IDENT Var '=' Exp ';'
       {
           $$ = makeNode(L_FIELD_ASSIGN);
           
           // Construire l'accès au champ
           Node *field_access = makeNode(L_FIELD_ACCESS);
           Node *base = makeNode(L_IDENT);
           base->value = strdup($1);
           addChild(field_access, base);
           
           Node *field = makeNode(L_IDENT);
           field->value = strdup($3);
           addChild(field_access, field);
           
           // Ajouter les accès supplémentaires (Var)
           if ($4) siblingsToChildren(field_access, $4);
           
           addChild($$, field_access);
           addChild($$, $6);
       }
    |  IF '(' Exp ')' Instr %prec LOWER_THAN_ELSE
       {
           $$ = makeNode(L_IF);
           addChild($$, $3);
           addChild($$, $5);
       }
    |  IF '(' Exp ')' Instr ELSE Instr
       {
           $$ = makeNode(L_IF_ELSE);
           addChild($$, $3);
           addChild($$, $5);
           addChild($$, $7);
       }
    |  WHILE '(' Exp ')' Instr
       {
           $$ = makeNode(L_WHILE);
           addChild($$, $3);
           addChild($$, $5);
       }
    |  IDENT '(' Arguments ')' ';'
       {
           $$ = makeNode(L_CALL);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($1);
           addChild($$, name);
           if ($3) siblingsToChildren($$, $3);
       }
    |  RETURN Exp ';'
       {
           $$ = makeNode(L_RETURN);
           addChild($$, $2);
       }
    |  RETURN ';'
       {
           $$ = makeNode(L_RETURN);
       }
    |  '{' SuiteInstr '}'
       {
           $$ = makeNode(L_BLOCK);
           if ($2) siblingsToChildren($$, $2);
       }
    |  ';'
       {
           $$ = NULL; // Instruction vide ne crée pas de nœud
       }
    ;

Exp:  Exp OR TB
      {
          $$ = makeNode(L_OR);
          addChild($$, $1);
          addChild($$, $3);
      }
    | TB { $$ = $1; }
    ;

TB:  TB AND FB
     {
         $$ = makeNode(L_AND);
         addChild($$, $1);
         addChild($$, $3);
     }
    | FB { $$ = $1; }
    ;

FB:  FB EQ M
     {
         if (strcmp($2, "==") == 0) {
             $$ = makeNode(L_EQ);
         } else {
             $$ = makeNode(L_NEQ);
         }
         addChild($$, $1);
         addChild($$, $3);
     }
    | M { $$ = $1; }
    ;

M:  M ORDER E
    {
        if (strcmp($2, "<") == 0) {
            $$ = makeNode(L_LT);
        } else if (strcmp($2, ">") == 0) {
            $$ = makeNode(L_GT);
        } else if (strcmp($2, "<=") == 0) {
            $$ = makeNode(L_LE);
        } else {
            $$ = makeNode(L_GE);
        }
        addChild($$, $1);
        addChild($$, $3);
    }
    | E { $$ = $1; }
    ;

E:  E ADDSUB T
    {
        if (strcmp($2, "+") == 0) {
            $$ = makeNode(L_ADD);
        } else {
            $$ = makeNode(L_SUB);
        }
        addChild($$, $1);
        addChild($$, $3);
    }
    | T { $$ = $1; }
    ;    

T:  T DIVSTAR F 
    {
        if (strcmp($2, "*") == 0) {
            $$ = makeNode(L_MUL);
        } else if (strcmp($2, "/") == 0) {
            $$ = makeNode(L_DIV);
        } else {
            $$ = makeNode(L_MOD);
        }
        addChild($$, $1);
        addChild($$, $3);
    }
    | F { $$ = $1; }
    ;

F:  ADDSUB F
    {
        $$ = makeNode(L_NEG);
        addChild($$, $2);
    }
    |  '!' F
       {
           $$ = makeNode(L_NOT);
           addChild($$, $2);
       }
    |  '(' Exp ')'
       {
           $$ = $2;
       }
    |  NUM
       {
           $$ = makeNode(L_NUM);
           $$->value = malloc(20);
           sprintf($$->value, "%d", $1);
       }
    |  CHARACTER
       {
           $$ = makeNode(L_CHAR);
           $$->value = malloc(10);
           sprintf($$->value, "'%c'", $1);
       }
    |  IDENT
       {
           $$ = makeNode(L_IDENT);
           $$->value = strdup($1);
       }
    |  IDENT '(' Arguments ')'
       {
           $$ = makeNode(L_CALL);
           Node *name = makeNode(L_IDENT);
           name->value = strdup($1);
           addChild($$, name);
           if ($3) siblingsToChildren($$, $3);
       }
    |  IDENT '.' IDENT Var
       {
           $$ = makeNode(L_FIELD_ACCESS);
           Node *base = makeNode(L_IDENT);
           base->value = strdup($1);
           addChild($$, base);
           
           Node *field = makeNode(L_IDENT);
           field->value = strdup($3);
           addChild($$, field);
           
           if ($4) siblingsToChildren($$, $4);
       }
    ;

Var: 
     '.' IDENT Var
     {
         Node *field = makeNode(L_IDENT);
         field->value = strdup($2);
         if ($3 == NULL) {
             $$ = field;
         } else {
             addSibling(field, $3);
             $$ = field;
         }
     }
    | { $$ = NULL; }
    ;
    
Arguments:
       ListExp { $$ = $1; }
    |  { $$ = NULL; }
    ;
    
ListExp:
       ListExp ',' Exp
       {
           if ($1 == NULL) {
               $$ = $3;
           } else {
               addSibling($1, $3);
               $$ = $1;
           }
       }
    |  Exp { $$ = $1; }
    ;

%%