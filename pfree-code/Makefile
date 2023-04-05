
CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic
CPP=g++-11 -std=c++20
CPPFLAGS=-Wall -Wextra -Werror -pedantic -ggdb

OUT=output
BIN=bin
SRC=src
LOG=logs
INCLUDE=/mnt/c/Users/qberr/csci/reed/csci470/prefix-free/include

OBJ=$(SRC:.cpp=.o)

all: $(BIN)/parse $(BIN)/mt_parse

$(BIN)/parse: $(SRC)/pfree_parse.cpp 
		$(CPP) $(CPPFLAGS) -I$(INCLUDE) $^ -o $@ 

$(BIN)/mt_parse: $(SRC)/mt_pfree_parse.cpp 
		$(CPP) -pthread $(CPPFLAGS) -I$(INCLUDE) $^ -o $@ 

gsa/gsacak.o: gsa/gsacak.c gsa/gsacak.h
		$(CC) -c -o $@ $<

run: clean all
		clear
		@echo "Executing ..."
		./$(BIN)/$(EXECUTABLE)

clean:
		@echo "Cleaning up ..."
		@touch $(BIN)/~
		@rm $(BIN)/*
		@touch $(OUT)/~
		@rm $(OUT)/*
		@touch $(LOG)/~
		@rm $(LOG)/*
