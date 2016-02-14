####################################
#
#
####################################

export SH = /bin/bash
export CC = gcc
export LD = gcc
export AR = ar

ifdef DEBUG
    export CFLAGS += -g
else
    export CFLAGS += -O2
endif
export CFLAGS += -Wall

ifndef DEBUG
    export LDFLAGS += -s
endif

export DEPFILE = depfile.mk

####################################
#
#
####################################

DIRS += common
DIRS += flash
DIRS += asm
DIRS += lkr

.PHONY: all clean depend samples
all:
	$(SH) foreach.sh $@ $(DIRS)

clean:
	$(SH) foreach.sh $@ $(DIRS)
	make -C samples clean

depend:
	$(SH) foreach.sh $@ $(DIRS)

samples: all
	make -C samples all

