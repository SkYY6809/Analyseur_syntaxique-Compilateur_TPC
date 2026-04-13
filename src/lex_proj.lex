%{
#include "tree.h"
#include "bison_proj.h"
#include "compiler.h"
#include "semantique.h"
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
    printf("Usage: ./tpcas [OPTIONS] [FILE]\nAnalyse syntaxique du langage tpc\n\nOptions:\n\t-h, --help     Affiche cette aide et quitte\n\t-t, --tree     Affiche l'arbre syntaxique abstrait\n\nSans FILE, le programme lit l'entrée standard.\n");
}

int main(int argc, char **argv) {
    int tree = 0;
    FILE * input = NULL;

    if(argc > 1){
        for(int i = 1; i < argc; i++){
            if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")){
                print_help();
                return 0;
            }
            else if(!strcmp(argv[i], "-t") || !strcmp(argv[i], "--tree")){
                tree = 1;
            }
            else if(argv[i][0] == '-'){
                fprintf(stderr, "Option inconnue : %s\n", argv[i]);
                return 2;
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
                yyin = input;
            }
        }
    }

    if (yyparse() == 0){
        if(tree && root){
            printf("Affichage de l'arbre abstrait \n");
            printTree(root);
        }
        FILE * anonym = fopen("src/_anonymous.asm", "w"); //création de l'assembleur
        

        if(root) {
            Table_symb * tableGlobale = NULL;
            analyse_semantique(root, &tableGlobale, NULL, anonym); // On lance le parcours
            printf("\n--- Table Globale ---\n");
            printT(tableGlobale);
            freeTable(tableGlobale);
        }
        fclose(anonym); //fermeture de l'assembleur
        return 0;
    }
    else{
        return 1;
    }


}
