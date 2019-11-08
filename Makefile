.PHONY: print_vars
OS = $(shell uname -s)
CFLAGS= -fPIC -shared
INC=-I../../../include
SRC=aggregation.c
OUTPUT= aggregation.so

ifneq (,$(filter $(OS),Linux linux))
	CC=gcc
else
    ifeq ($(OS), FreeBSD)
    CC=clang
    INC += -I/usr/local/include
    else
	@echo "Error"
	@exit 1;
    endif
endif


all:
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC) $(INC)


debug: CFLAGS += -ggdb

debug: all




    
    