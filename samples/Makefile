
.PHONY: all clean depend

SAMPLES += asm_syntax
SAMPLES += led_simple_STM8S207C8
SAMPLES += led_advanced_STM8S207C8

all:
	$(SH) ../foreach.sh $@ $(SAMPLES)

clean:
	$(SH) ../foreach.sh $@ $(SAMPLES)

depend:
	$(SH) ../foreach.sh $@ $(SAMPLES)

