
ROOT_DIR = ../..

STM8MU_TOOLCHAIN_PATH = $(ROOT_DIR)
PATH := $(PATH):$(STM8MU_TOOLCHAIN_PATH)

####################################
#
#
####################################

ASM = stm8mu_asm

####################################
#
#
####################################

OBJS += syntax.l0

####################################
#
#
####################################

ASM_OPTIONS += -I
ASM_OPTIONS += -DTEST0=256

####################################
#
#
####################################

.PHONY: all clean depend flash go

%.l0: %.asm
	$(ASM) $(ASM_OPTIONS) $<

all: $(OBJS)

clean:
	rm -f $(OBJS)

