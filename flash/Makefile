####################################
#
#
####################################

ROOT_DIR = ..

TARGET = $(ROOT_DIR)/stm8mu_flash

####################################
#
#
####################################
CFLAGS += -I../common

####################################
#
#
####################################
C_FILES += main.c
C_FILES += cport.c
C_FILES += program.c
C_FILES += ew.c

C_OBJS = $(foreach obj,$(C_FILES) ,$(patsubst %c, %o, $(obj)))
OBJS += $(C_OBJS)

LIBS += -lcommon

VPATH += $(ROOT_DIR)
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

$(TARGET): $(OBJS) $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

-include $(DEPFILE)

