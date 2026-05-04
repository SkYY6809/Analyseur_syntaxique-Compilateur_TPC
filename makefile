CC     = gcc
CFLAGS = -Wall -g -Iobj -Isrc
NASM   = nasm
PARSER = bison_proj
LEXER  = lex_proj
ASM    = src/_anonymous.asm
TPC   ?= gen-code-types.tpc

all: bin/tpcc

bin/tpcc: obj/$(LEXER).o obj/$(PARSER).o obj/tree.o obj/compiler.o obj/semantique.o
	mkdir -p bin
	$(CC) -o $@ $^ -lfl

obj/tree.o:       src/tree.c       src/tree.h
obj/compiler.o:   src/compiler.c   src/compiler.h src/tree.h
obj/semantique.o: src/semantique.c src/semantique.h src/tree.h
obj/$(PARSER).o:  obj/$(PARSER).c  src/tree.h
obj/$(LEXER).o:   obj/$(LEXER).c   obj/$(PARSER).h

obj/%.o: src/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

obj/%.o: obj/%.c
	$(CC) -c -o $@ $< $(CFLAGS)
obj/$(LEXER).c: src/$(LEXER).lex obj/$(PARSER).h
	mkdir -p obj
	flex -o $@ $<

obj/$(PARSER).c obj/$(PARSER).h: src/$(PARSER).y
	mkdir -p obj
	bison -d -o obj/$(PARSER).c $<
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