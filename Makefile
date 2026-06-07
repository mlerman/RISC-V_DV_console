CC = riscv64-unknown-elf-gcc
CFLAGS = -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany -ffreestanding -nostdlib -O0 -g

all: console.elf

console.elf: start.S uart.c aplic.c trap.c mode.c main.c smp.c link.ld
	$(CC) $(CFLAGS) -T link.ld -o console.elf start.S uart.c aplic.c trap.c mode.c main.c smp.c

run: console.elf
# 2 harts — NUM_HARTS must match
	qemu-system-riscv64 -machine virt,aia=aplic -bios none -kernel console.elf -nographic -smp 8
# 4 harts — set NUM_HARTS=4 in smp.h first
#	qemu-system-riscv64 -machine virt,aia=aplic -bios none -kernel console.elf -nographic -smp 4

clean:
	rm -f console.elf
