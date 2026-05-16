%{
#include "tree.h"
#include "bison_proj.h"
#include "compiler.h"
#include "semantique.h"
#include "struct_table.h"
#include <stdlib.h>
#include <string.h>


int lineno = 1;

char current_line[1024];

extern Node * root;

%}

%x COMMENT

%%

"/*"                    { BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>\n             { lineno++; }
<COMMENT>.              ;

"//".*                  ;




"char"|"int"            { yylval.ident = strdup(yytext); return TYPE; }
"void"                  { return VOID; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"return"                { return RETURN; }
"while"                 { return WHILE; }
"struct"                { return STRUCT; }


"=="|"!="               { yylval.ident = strdup(yytext); return EQ; }
"<="|">="|"<"|">"       { yylval.ident = strdup(yytext); return ORDER; }
"+"|"-"                 { yylval.ident = strdup(yytext); return ADDSUB; }
"*"|"/"|"%"             { yylval.ident = strdup(yytext); return DIVSTAR; }
"||"                    { yylval.ident = strdup(yytext); return OR; }
"&&"                    { yylval.ident = strdup(yytext); return AND; }


","                     { return ','; }
";"                     { return ';'; }
"="                     { return '='; }
"("                     { return '('; }
")"                     { return ')'; }
"{"                     { return '{'; }
"}"                     { return '}'; }
"."                     { return '.'; }
"!"                     { return '!'; }


[0-9]+                  {
                            yylval.num = atoi(yytext); 
                            return NUM; 
                        }

\'(\\n|\\t|\\'|[^\'\\])\'  {
                                if (yytext[1] == '\\') {
                                    if (yytext[2] == 'n') yylval.character = '\n';
                                    else if (yytext[2] == 't') yylval.character = '\t';
                                    else if (yytext[2] == '\'') yylval.character = '\'';
                                } else {
                                    yylval.character = yytext[1];
                                }
                                return CHARACTER;
                            }


[a-zA-Z_][a-zA-Z0-9_]*  {
                            yylval.ident = strdup(yytext); 
                            return IDENT; 
                        }


\n                      {lineno++;}

[ \t\r]+                ;


.                       {
                            fprintf(stderr,"Erreur lexicale ligne %d : %s\n", lineno, yytext);
                            return LEX_ERROR;
                        }

%%

void print_help(void) {
    printf("Usage: ./tpcc [OPTIONS] [FILE]\nAnalyse syntaxique du langage tpc\n\nOptions:\n\t-h, --help     Affiche cette aide et quitte\n\t-t, --tree     Affiche l'arbre syntaxique abstrait\n\nSans FILE, le programme lit l'entrée standard.\n");
}

int main(int argc, char **argv) {
    int tree = 0;
    int symbol = 0;
    FILE * input = NULL;
    char *input_filename = NULL;  

    //lecture des parametres
    if(argc > 1){
        for(int i = 1; i < argc; i++){
            if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")){
                print_help();
                return 0;
            }
            else if(!strcmp(argv[i], "-t") || !strcmp(argv[i], "--tree")){
                tree = 1;
            }
            else if(!strcmp(argv[i], "-s") || !strcmp(argv[i], "--symtabs")){
                symbol = 1;
            }
            else if(argv[i][0] == '-'){
                fprintf(stderr, "Option inconnue : %s\n", argv[i]);
                return 3;
            }
            else{
                if(input != NULL){
                    fprintf(stderr, "Plusieurs fichiers d'entrée fournis\n");
                    return 2;
                }
                input = fopen(argv[i], "r");
                if(!input){
                    perror("fopen");
                    return 2;
                }
                input_filename = argv[i];
                yyin = input;
            }
        }
    }

    if (yyparse() == 0){
        if(tree && root){
            printf("Affichage de l'arbre abstrait \n");
            printTree(root);
        }
        char asm_name[512];
        if (input_filename == NULL) {
            snprintf(asm_name, sizeof(asm_name), "_anonymous.asm");
        } else {
            strncpy(asm_name, input_filename, sizeof(asm_name) - 1);
            asm_name[sizeof(asm_name) - 1] = '\0';
            char *dot = strrchr(asm_name, '.');
            if (dot && strcmp(dot, ".tpc") == 0) {
                strcpy(dot, ".asm");   
            } else {
                strncat(asm_name, ".asm", sizeof(asm_name) - strlen(asm_name) - 1);
            }
        }

        FILE * anonym = fopen(asm_name, "w"); 
        if (!anonym) {
            perror("Impossible de créer le fichier ASM");
            return 3;
        }        
        Table_symb * tableGlobale = NULL;

        if(root) {
            init_builtins(&tableGlobale);
            init_struct_table();
            int ret = analyse_semantique(root, &tableGlobale, NULL, anonym, symbol, NULL); // On lance le parcours
            if (ret != 0) {
                fclose(anonym);
                freeTable(tableGlobale);
                return ret; // retourne 2
            }
            //verif si y'a un main
            if(!haveCorrectMain(&tableGlobale)){
                fprintf(stderr, "Erreur sémantique : Pas de fonction int main().\n");
                return 2;

            }
            //option table de symbol
            if(symbol){
                printf("\n--- Table Globale ---\n");
                printT(tableGlobale);
            
            }
        }
        
        generer_footer_asm(anonym);
        generer_bss(anonym, tableGlobale);
        freeTable(tableGlobale);
        free_struct_table();
        
        fclose(anonym); //fermeture de l'assembleur
        return 0;
    }
    else{
        return 1;
    }


}
