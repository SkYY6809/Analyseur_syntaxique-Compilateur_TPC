CC = gcc
CFLAGS = -Wall -g -Iobj -Isrc
PARSER = bison_proj
LEXER = lex_proj

# Cible principale
bin/tpcas: obj/$(LEXER).o obj/$(PARSER).o obj/tree.o obj/compiler.o obj/semantique.o
	$(CC) -o $@ $^ -lfl

# --- GESTION DES DEPENDANCES ---

obj/tree.o: src/tree.c src/tree.h
obj/compiler.o: src/compiler.c src/compiler.h src/tree.h
obj/semantique.o: src/semantique.c src/semantique.h src/tree.h

# Dépendances spécifiques pour les fichiers générés
obj/$(PARSER).o: obj/$(PARSER).c src/tree.h
obj/$(LEXER).o: obj/$(LEXER).c obj/$(PARSER).h

# --- REGLES DE COMPILATION ---

# 1. Règle pour les fichiers sources "normaux" (dans src/)
obj/%.o: src/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

# 2. [NOUVEAU] Règle pour les fichiers sources générés (dans obj/)
# C'est cette règle qui manquait pour recompiler lex_proj.c une fois régénéré
obj/%.o: obj/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

# --- GENERATION FLEX / BISON ---

# Génération du lexeur (Flex)
# Si .lex change -> on régénère le .c
obj/$(LEXER).c: src/$(LEXER).lex obj/$(PARSER).h
	flex -o $@ $<

# Génération du parseur (Bison)
# Si .y change -> on régénère le .c et le .h
obj/$(PARSER).c obj/$(PARSER).h: src/$(PARSER).y
	bison -d -o obj/$(PARSER).c $<

# --- NETTOYAGE ---

clean:
	rm -f obj/*.o obj/*.c obj/*.h bin/tpcas