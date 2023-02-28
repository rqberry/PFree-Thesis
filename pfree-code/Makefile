
CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic
CPP=g++ -std=c++17
CPPFLAGS=-Wall -Wextra -Werror -pedantic -std=c++17 -ggdb

OUT=output
BIN=bin
SRC=src
LOG=logs
INCLUDE=/mnt/c/Users/qberr/csci/reed/csci470/prefix-free/include

OBJ=$(SRC:.cpp=.o)

all: $(BIN)/parse

$(BIN)/parse: $(SRC)/pfree_parse.cpp 
		$(CPP) $(CPPFLAGS) -I$(INCLUDE) $^ -o $@ 

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
