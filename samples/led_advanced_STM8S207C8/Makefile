
ROOT_DIR = ../..

STM8MU_TOOLCHAIN_PATH = $(ROOT_DIR)
PATH := $(PATH):$(STM8MU_TOOLCHAIN_PATH)

####################################
#
#
####################################

ASM   = stm8mu_asm
LKR   = stm8mu_lkr
FLASH = stm8mu_flash

FLASH_CHIP  = STM8S207C8
FLASH_BAUD  = 115200
FLASH_CPORT = /dev/ttyUSB0

####################################
#
#
####################################
TARGET = led.s19

OBJS += led.l0
OBJS += delay.l0

LSCRIPT = led.lkr

####################################
#
#
####################################

ASM_OPTIONS += -I

ifdef PRINTMAP
    LKR_OPTIONS += -M
    LKR_OPTIONS += -MD
endif
LKR_OPTIONS += -p
LKR_OPTIONS += --script=$(LSCRIPT)
LKR_OPTIONS += --output=$(TARGET)
LKR_OPTIONS += -DINTERVAL=1000
LKR_OPTIONS += --s19head="Test LED"

####################################
#
#
####################################

.PHONY: all clean depend flash go

%.l0: %.asm
	$(ASM) $(ASM_OPTIONS) $<

all: $(TARGET) $(OBJS)

$(TARGET): $(OBJS) $(LSCRIPT)
	$(LKR) $(LKR_OPTIONS) $(OBJS)

flash: $(TARGET)
	$(FLASH) --chip=$(FLASH_CHIP) --baud=$(FLASH_BAUD) --cport=$(FLASH_CPORT) --input=$^ write

go:
	$(FLASH) --chip=$(FLASH_CHIP) --baud=$(FLASH_BAUD) --cport=$(FLASH_CPORT) go

clean:
	rm -f $(OBJS) $(TARGET)

ifdef SAMPLES_FORCE_REBUILD
.PHONY: force
$(OBJS): force
force:
endif

