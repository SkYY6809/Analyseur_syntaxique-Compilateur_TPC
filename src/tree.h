/* tree.h */


typedef enum {
    // Programme et déclarations
    L_PROG,
    L_DECL_STRUCT,
    L_DECL_VAR,
    L_DECL_FONCT,
    L_CHAMPS,
    L_CHAMP,
    
    // Types
    L_TYPE_INT,
    L_TYPE_VOID,
    L_TYPE_CHAR,
    L_TYPE_STRUCT,
    
    // En-tête de fonction
    L_ENTETE_FONCT,
    L_PARAMETRES,
    L_PARAM,
    
    // Instructions
    L_CORPS,
    L_ASSIGN,
    L_FIELD_ASSIGN,
    L_IF,
    L_IF_ELSE,
    L_WHILE,
    L_RETURN,
    L_CALL,
    L_BLOCK,
    
    // Expressions
    L_OR,
    L_AND,
    L_EQ,
    L_NEQ,
    L_LT,
    L_GT,
    L_LE,
    L_GE,
    L_ADD,
    L_SUB,
    L_MUL,
    L_DIV,
    L_MOD,
    L_NOT,
    L_NEG,
    
    // Terminaux
    L_IDENT,
    L_NUM,
    L_CHAR,
    L_FIELD_ACCESS,
    
    // Listes
    L_DECLARATEURS,
    L_ARGUMENTS
} label_t;

typedef struct Node {
  label_t label;
  char * value;
  struct Node *firstChild, *nextSibling;
  int lineno;
} Node;

Node *makeNode(label_t label);
void addSibling(Node *node, Node *sibling);
void addChild(Node *parent, Node *child);
void siblingsToChildren(Node *parent, Node *firstSibling);
void deleteTree(Node*node);
void printTree(Node *node);

#define FIRSTCHILD(node) node->firstChild
#define SECONDCHILD(node) node->firstChild->nextSibling
#define THIRDCHILD(node) node->firstChild->nextSibling->nextSibling
