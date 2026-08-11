all: dry

dry:
	gcc \
		src/main.c src/globals.c \
		src/util/util.c src/util/da.c src/util/map.c \
		src/frontend/lexer.c src/frontend/parser.c \
		src/stdlib/stdlib.c \
		src/compiler.c src/codegen.c \
		-o build/dry

clean:
	rm build/dry