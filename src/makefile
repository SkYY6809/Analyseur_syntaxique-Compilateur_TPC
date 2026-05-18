CC     = gcc
CFLAGS = -Wall -g -Iobj -Isrc
NASM   = nasm
PARSER = bison_proj
LEXER  = lex_proj
ASM    = src/_anonymous.asm
TPC   ?= gen-code-types.tpc

all: bin/tpcc

# Ajout de struct_table.o aux dépendances
bin/tpcc: obj/$(LEXER).o obj/$(PARSER).o obj/tree.o obj/compiler.o obj/semantique.o obj/struct_table.o
	mkdir -p bin
	$(CC) -o $@ $^ -lfl

# Règles pour les fichiers objets depuis src/
obj/tree.o:       src/tree.c       src/tree.h
obj/compiler.o:   src/compiler.c   src/compiler.h src/tree.h
obj/semantique.o: src/semantique.c src/semantique.h src/tree.h src/struct_table.h
obj/struct_table.o: src/struct_table.c src/struct_table.h
obj/$(PARSER).o:  obj/$(PARSER).c  src/tree.h
obj/$(LEXER).o:   obj/$(LEXER).c   obj/$(PARSER).h

# Compilation générique depuis src/
obj/%.o: src/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

# Compilation depuis obj/ (pour flex/bison)
obj/%.o: obj/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

# Génération du lexeur
obj/$(LEXER).c: src/$(LEXER).lex obj/$(PARSER).h
	mkdir -p obj
	flex -o $@ $<

# Génération du parseur
obj/$(PARSER).c obj/$(PARSER).h: src/$(PARSER).y
	mkdir -p obj
	bison -d -o obj/$(PARSER).c $<

# TPC -> ASM
asm: bin/tpcc
	./bin/tpcc $(TPC)

# TPC -> ASM -> objet -> exécutable -> exécution
run: asm
	$(NASM) -f elf64 $(ASM) -o out.o
	ld -no-pie out.o -o out
	./out; echo "Code de retour : $$?"

clean:
	rm -f obj/*.o obj/*.c obj/*.h bin/tpcc $(ASM) out.o out

.PHONY: all asm run clean