
CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic
CXX=g++ -std=c++17
CXXFLAGS=-Wall -Wextra -Werror -pedantic -std=c++17 -ggdb

OUT=output
BIN=bin
SRC=src
INCLUDE=/mnt/c/Users/qberr/csci/reed/csci470/prefix-free/include
EXECUTABLE=pfree

OBJ=$(SRC:.cpp=.o)

all: $(BIN)/$(EXECUTABLE)

gsa/gsacak.o: gsa/gsacak.c gsa/gsacak.h
		$(CC) -c -o $@ $<

run: clean all
		clear
		@echo "Executing ..."
		./$(BIN)/$(EXECUTABLE)

$(BIN)/$(EXECUTABLE): $(SRC)/*.cpp gsa/gsacak.o
		@echo "Building ..."
		$(CXX) $(CXXFLAGS) -I$(INCLUDE) $^ -o $@ 

linker: 
		$(CXX) $(CXXFLAGS) $(BIN)/$(EXECUTABLE) -o $ somecode.o othercode.o

clean:
		@echo "Cleaning up ..."
		rm $(BIN)/*
		rm $(OUT)/*
