# project/Makefile (顶层目录)
CFLAGS =
K_SRCS_PATH =
include code/kernel/Makefile
U_CFLAGS =
U_SRCS_PATH =
include code/user/Makefile

U=code/user
K=code/kernel

TOOLPREFIX = riscv64-unknown-elf-
QEMU = qemu-system-riscv64
GDB = gdb-multiarch
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)as
LD = $(TOOLPREFIX)ld
DD = dd
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

XCFLAGS = 
DEFS += -DPRINT_KERNEL_INFO

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

KERNEL_PATH = ${K}/output

USER_PATH = ${U}/output

${KERNEL_PATH}:
	@${MKDIR} $@

${USER_PATH}:
	@${MKDIR} $@

# 使用foreach和wildcard函数来找到所有C文件和ASM文件
K_SRCS_C := $(foreach dir,$(K_SRCS_PATH),$(wildcard $(dir)/*.c))
K_SRCS_ASM := $(foreach dir,$(K_SRCS_PATH),$(wildcard $(dir)/*.S))
ifneq ($(filter $K/test/usys.S,$(K_SRCS_ASM)), $K/test/usys.S)
K_SRCS_ASM += $K/test/usys.S
endif

K_OBJS_ASM := $(addprefix ${KERNEL_PATH}/, $(patsubst %.S, %.o, $(notdir ${K_SRCS_ASM})))
K_OBJS_C   := $(addprefix ${KERNEL_PATH}/, $(patsubst %.c, %.o, $(notdir ${K_SRCS_C})))
K_OBJS = ${K_OBJS_ASM} ${K_OBJS_C}

KERNEL_ELF = ${KERNEL_PATH}/kernel.elf
KERNEL_BIN = ${KERNEL_PATH}/kernel.bin

ifndef CPUS
CPUS := 4
endif

swap.img:
	#${DD} if=/dev/zero of=$@ bs=64M count=1
	${DD} if=/dev/urandom of=$@ bs=64M count=1

fs.img:
	#${DD} if=/dev/zero of=$@ bs=128M count=1
	${DD} if=/dev/urandom of=$@ bs=128M count=1


QEMUOPTS = -machine virt -bios none -kernel ${KERNEL_ELF} -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -device virtio-rng-device,bus=virtio-mmio-bus.0
QEMUOPTS += -drive file=swap.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.1
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x1
QEMUOPTS += -device virtio-blk-device,drive=x1,bus=virtio-mmio-bus.2


# 添加C文件的编译规则
define K_compile_c_file
$(CC) ${DEFS} ${CFLAGS} -c -o ${KERNEL_PATH}/$(notdir $(1:.c=.o)) $(1)
endef

# 添加汇编文件的编译规则
define K_compile_asm_file
$(CC) ${CFLAGS} -c -o ${KERNEL_PATH}/$(notdir $(1:.S=.o)) $(1)
endef

# 添加C文件的编译规则
define U_compile_c_file
$(CC) ${DEFS} ${U_CFLAGS} -c -o ${USER_PATH}/$(notdir $(1:.c=.o)) $(1)
endef

# 添加汇编文件的编译规则
define U_compile_asm_file
$(CC) ${U_CFLAGS} -c -o ${USER_PATH}/$(notdir $(1:.S=.o)) $(1)
endef

U_SRCS_C := $(foreach dir,$(U_SRCS_PATH),$(wildcard $(dir)/*.c))
U_SRCS_ASM := $(foreach dir,$(U_SRCS_PATH),$(wildcard $(dir)/*.S))

U_OBJS_ASM := $(addprefix ${USER_PATH}/, $(patsubst %.S, %.o, $(notdir ${U_SRCS_ASM})))
U_OBJS_C   := $(addprefix ${USER_PATH}/, $(patsubst %.c, %.o, $(notdir ${U_SRCS_C})))
U_OBJS = ${U_OBJS_ASM} ${U_OBJS_C}

# 生成所有C文件的目标文件
K_compile_c: $(K_SRCS_C)
	$(foreach src, $(K_SRCS_C), $(call K_compile_c_file, $(src));)

# 生成所有汇编文件的目标文件
K_compile_S: $(K_SRCS_ASM)
	$(foreach src, $(K_SRCS_ASM), $(call K_compile_asm_file, $(src));)

# 生成所有C文件的目标文件
U_compile_c: $(K_SRCS_C)
	$(foreach src, $(K_SRCS_C), $(call K_compile_c_file, $(src));)

# 生成所有汇编文件的目标文件
U_compile_S: $(K_SRCS_ASM)
	$(foreach src, $(K_SRCS_ASM), $(call K_compile_asm_file, $(src));)

# ss: $U/usys.S

# $U/usys.S : $U/util/usys.py
# 	python3 $U/util/usys.py > $U/usys.S

ss: $K/test/usys.S

$K/test/usys.S : $K/test/usys.py
	python3 $K/test/usys.py > $K/test/usys.S

# 将所有目标文件作为最终目标的依赖
${KERNEL_ELF}: ${KERNEL_PATH} K_compile_c K_compile_S
	$(LD) ${LDFLAGS} -o ${KERNEL_ELF} ${K_OBJS}
	$(OBJDUMP) -S ${KERNEL_ELF} > ${KERNEL_PATH}/kernel.asm
	$(OBJDUMP) -t ${KERNEL_ELF} | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > ${KERNEL_PATH}/kernel.sym
	${OBJCOPY} -O binary ${KERNEL_ELF} ${KERNEL_BIN}

GDBPORT = $(shell expr `id -u` % 5000 + 25027)

QEMUGDB = -gdb tcp::$(GDBPORT)

all: ${KERNEL_ELF}

qemu: clean ss all swap.img fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: clean ss ${KERNEL_ELF} .gdbinit swap.img fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	@echo "$(QEMUGDB)"
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)	

debug: clean ss all swap.img fs.img
	@echo "Press Ctrl-C and then input 'quit' to exit GDB and QEMU"
	@echo "-------------------------------------------------------"
	@${QEMU} ${QEMUOPTS} -s -S &
	@${GDB} ${KERNEL_ELF} -q -x gdbinit

clean:
	rm -rf .gdbinit $K/test/usys.S $(KERNEL_PATH) kernel.p
all-clean:
	rm -rf .gdbinit $K/test/usys.S $(KERNEL_PATH) kernel.p *.img

