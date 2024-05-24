# project/Makefile (顶层目录)
# SRCS_ASM = 
# SRCS_C =
SRCS_PATH =
CFLAGS =
include code/kernel/Makefile
#include code/user/Makefile

TOOLPREFIX = riscv64-unknown-elf-
QEMU = qemu-system-riscv64
GDB = gdb-multiarch
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)as
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

XCFLAGS =
DEFS +=

CFLAGS += -Wall -O0 -Werror -fno-omit-frame-pointer -ggdb
CFLAGS += -Wno-unused-function -Wno-unused-variable 
CFLAGS += $(XCFLAGS)
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += -fno-pie -no-pie
CFLAGS += -std=gnu11

# LDFLAGS = -Ttext=0x80000000

LDFLAGS ?= -T kernel.ld

LDFLAGS += -z max-page-size=4096

MKDIR = mkdir -p

KERNEL_PATH = code/kernel/kernel_output

${KERNEL_PATH}:
	@${MKDIR} $@

# 使用foreach和wildcard函数来找到所有C文件和ASM文件
SRCS_C := $(foreach dir,$(SRCS_PATH),$(wildcard $(dir)/*.c))
SRCS_ASM := $(foreach dir,$(SRCS_PATH),$(wildcard $(dir)/*.S))

OBJS_ASM := $(addprefix ${KERNEL_PATH}/, $(patsubst %.S, %.o, $(notdir ${SRCS_ASM})))
OBJS_C   := $(addprefix ${KERNEL_PATH}/, $(patsubst %.c, %.o, $(notdir ${SRCS_C})))
OBJS = ${OBJS_ASM} ${OBJS_C}

KERNEL_ELF = ${KERNEL_PATH}/kernel.elf
KERNEL_BIN = ${KERNEL_PATH}/kernel.bin

ifndef CPUS
CPUS := 8
endif

QEMUOPTS = -machine virt -bios none -kernel ${KERNEL_ELF} -m 128M -smp $(CPUS) -nographic

# 添加C文件的编译规则
define compile_c_file
$(CC) ${DEFS} ${CFLAGS} -c -o ${KERNEL_PATH}/$(notdir $(1:.c=.o)) $(1)
endef

# 添加汇编文件的编译规则
define compile_asm_file
$(CC) ${CFLAGS} -c -o ${KERNEL_PATH}/$(notdir $(1:.S=.o)) $(1)
endef

# 生成所有C文件的目标文件
compile_c: $(SRCS_C)
	$(foreach src, $(SRCS_C), $(call compile_c_file, $(src));)

# 生成所有汇编文件的目标文件
compile_S: $(SRCS_ASM)
	$(foreach src, $(SRCS_ASM), $(call compile_asm_file, $(src));)

# 将所有目标文件作为最终目标的依赖
${KERNEL_ELF}: ${KERNEL_PATH} compile_c compile_S
	$(LD) ${LDFLAGS} -o ${KERNEL_ELF} ${OBJS}
	$(OBJDUMP) -S ${KERNEL_ELF} > ${KERNEL_PATH}/kernel.asm
	$(OBJDUMP) -t ${KERNEL_ELF} | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > ${KERNEL_PATH}/kernel.sym
	${OBJCOPY} -O binary ${KERNEL_ELF} ${KERNEL_BIN}

GDBPORT = $(shell expr `id -u` % 5000 + 25027)

QEMUGDB = -gdb tcp::$(GDBPORT)

all: ${KERNEL_ELF}

qemu: clean all
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: clean ${KERNEL_ELF} .gdbinit
	@echo "*** Now run 'gdb' in another window." 1>&2
	@echo "$(QEMUGDB)"
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)	

debug: clean all
	@echo "Press Ctrl-C and then input 'quit' to exit GDB and QEMU"
	@echo "-------------------------------------------------------"
	@${QEMU} ${QEMUOPTS} -s -S &
	@${GDB} ${KERNEL_ELF} -q -x gdbinit

clean:
	rm -rf .gdbinit $(KERNEL_PATH)

test1: 
	@echo "$(SRCS_PATH)"
	@echo "$(SRCS_C)"
	@echo "$(SRCS_ASM)"
	@echo "$(OBJS)"
	@echo "$(CFLAGS)"
