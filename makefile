CC = gcc
CFLAGS = -Wall -g -Iobj -Isrc
PARSER = bison_proj
LEXER = lex_proj

ASM = src/_anonymous.asm
EXEC = out

# Cible principale
bin/tpcas: obj/$(LEXER).o obj/$(PARSER).o obj/tree.o obj/compiler.o obj/semantique.o
	$(CC) -o $@ $^ -lfl

# --- GESTION DES DEPENDANCES ---

obj/tree.o: src/tree.c src/tree.h
obj/compiler.o: src/compiler.c src/compiler.h src/tree.h
obj/semantique.o: src/semantique.c src/semantique.h src/tree.h

obj/$(PARSER).o: obj/$(PARSER).c src/tree.h
obj/$(LEXER).o: obj/$(LEXER).c obj/$(PARSER).h

# --- REGLES DE COMPILATION ---

obj/%.o: src/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

obj/%.o: obj/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

# --- GENERATION FLEX / BISON ---

obj/$(LEXER).c: src/$(LEXER).lex obj/$(PARSER).h
	flex -o $@ $<

obj/$(PARSER).c obj/$(PARSER).h: src/$(PARSER).y
	bison -d -o obj/$(PARSER).c $<

# --- GENERATION ASM ---

# Lance ton compilateur pour générer le .asm
$(ASM): bin/tpcas
	./bin/tpcas < $(INPUT)

# --- ASSEMBLAGE ---

# Assemble avec nasm 
$(EXEC): $(ASM)
	nasm -f elf64 $(ASM) -o out.o
	$(CC) out.o -o $(EXEC)

# --- EXECUTION ---

run: $(EXEC)
	./$(EXEC)

# --- PIPELINE COMPLET ---

all: bin/tpcas $(ASM) $(EXEC)

# --- NETTOYAGE ---

clean:
	rm -f obj/*.o obj/*.c obj/*.h bin/tpcas out.o $(ASM) $(EXEC)