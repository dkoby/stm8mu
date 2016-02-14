####################################
#
#
####################################

ROOT_DIR = ..
TARGET = $(ROOT_DIR)/libcommon.a

####################################
#
#
####################################
CFLAGS += -I.

####################################
#
#
####################################
C_FILES += btorder.c
C_FILES += llist.c
C_FILES += token.c
C_FILES += memdata.c
C_FILES += stm8chip.c
C_FILES += debug.c
C_FILES += memdata.c
C_FILES += srec.c
C_FILES += bmem.c
C_FILES += l0.c
C_FILES += symbol.c
C_FILES += section.c
C_FILES += relocation.c
C_FILES += lang_constexpr.c
C_FILES += lang_util.c

C_OBJS = $(foreach obj,$(C_FILES) ,$(patsubst %c, %o, $(obj)))
OBJS += $(C_OBJS)

####################################
#
#
####################################

.PHONY: all clean depend
all: $(TARGET)
clean:
	rm -f $(TARGET) $(OBJS) $(DEPFILE)
depend:
	$(CC) $(CFLAGS) -MM $(C_FILES) > $(DEPFILE)

$(TARGET): $(OBJS)
	$(AR) r $@ $^

-include $(DEPFILE)

