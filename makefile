CC = gcc
CFLAGS = -Wall -g -Iobj -Isrc
PARSER = bison_proj
LEXER = lex_proj

bin/tpcas: obj/$(LEXER).o obj/$(PARSER).o obj/tree.o
	$(CC) -o $@ $^ -lfl

obj/tree.o: src/tree.c src/tree.h

obj/$(PARSER).o: obj/$(PARSER).c src/tree.h
obj/$(LEXER).o: obj/$(LEXER).c obj/$(PARSER).h


obj/%.o: src/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

obj/$(LEXER).c: src/$(LEXER).lex obj/$(PARSER).h
	flex -o $@ $<

obj/$(PARSER).c obj/$(PARSER).h &: src/$(PARSER).y
	bison -d -o  obj/$(PARSER).c $<

clean:
	rm -f obj/*.o obj/*.c obj/*.h bin/tpcas