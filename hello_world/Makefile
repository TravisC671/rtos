PREFIX  = riscv32-unknown-elf-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
CFLAGS  = -g -march=rv32i -mabi=ilp32 -nostdlib -nostartfiles -T link.ld
TARGET  = hello

all: $(TARGET).hex $(TARGET).coe $(TARGET).bin $(TARGET).mem $(TARGET).elf

$(TARGET).elf: hello.S link.ld
	$(CC) $(CFLAGS) -o $@ hello.S

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# One 32-bit hex word per line (for GHDL testbenches / ITCM init)
$(TARGET).hex: $(TARGET).bin
	od -An -tx4 -w4 -v $< | tr -d ' ' | tr 'a-f' 'A-F' > $@

# Xilinx COE format (for Block RAM initialization)
$(TARGET).coe: $(TARGET).bin
	@echo "memory_initialization_radix=16;" > $@
	@echo "memory_initialization_vector=" >> $@
	@od -An -tx4 -w4 -v $< | tr -d ' ' | tr 'a-f' 'A-F' | \
		sed '$$s/$$/;/' | sed '$$!s/$$/,/' >> $@

# Binary text: one bit per character, one 32-bit word per line
$(TARGET).mem: $(TARGET).bin
	od -An -tx4 -w4 -v $< | tr -d ' ' | \
		python3 -c "import sys; [print(bin(int(l.strip(),16))[2:].zfill(32)) for l in sys.stdin if l.strip()]" > $@

disasm: $(TARGET).elf
	$(OBJDUMP) -d $<

clean:
	rm -f $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).coe $(TARGET).mem

.PHONY: all clean disasm
