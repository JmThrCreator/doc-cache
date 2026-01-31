CC = clang
CFLAGS += -std=c11 -Wall -O2 -pthread -Werror -fsanitize=address \
	  -I$(PWD)/lib -I$(PWD)/src \
	  -I$(PWD)/lib/mupdf

LDFLAGS = -g -fsanitize=address \
	-L$(PWD)/lib/mac/mupdf \
	-lmupdf -lmupdf-third

LIB = $(wildcard lib/*.c)
SRC = src/platform/mac.c
OUT = bin/doc-cache-mac

TEST_OUT = bin/test_runner

all: $(OUT)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LIB) $(LDFLAGS)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LIB) $(LDFLAGS)

run: $(OUT)
	$(CC) $(CFLAGS) -DKH_DEV -o $(OUT) $(SRC) $(LIB) $(LDFLAGS)
	./$(OUT) $(INPUT)

clean:
	rm -f $(OUT)

.PHONY: all clean
