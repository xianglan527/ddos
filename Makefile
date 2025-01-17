# project/Makefile (顶层目录)
CFLAGS =
K_SRCS_PATH =
K_INCLUDE =
include code/kernel/Makefile

K=code/kernel

TOOLPREFIX = riscv64-unknown-elf-
QEMU = sudo qemu-system-riscv64
GDB = sudo gdb-multiarch
DD = sudo dd
OBJCOPY = sudo $(TOOLPREFIX)objcopy
OBJDUMP = sudo $(TOOLPREFIX)objdump
AR = sudo $(TOOLPREFIX)ar
CC = sudo $(TOOLPREFIX)gcc
AS = sudo $(TOOLPREFIX)as
LD = sudo $(TOOLPREFIX)ld

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

${KERNEL_PATH}:
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
CPUS := 1
endif

DEFS += -DCPUS=$(CPUS)

BR_IF = br0

# 延迟变量赋值，使用递归展开
TAP0_MAC = $(shell ip link show tap0 | awk '/ether/ {print $$2}')
TAP1_MAC = $(shell ip link show tap1 | awk '/ether/ {print $$2}')

# TAP0_MAC = $(shell ip link show br0 | awk '/ether/ {print $$2}')
# TAP1_MAC = $(shell ip link show br0 | awk '/ether/ {print $$2}')

# 移除即时条件检查
# ifeq ($(TAP0_MAC),)
#   $(error 无法获取 tap0 的 MAC 地址。请确保 tap0 接口存在且已配置。)
# endif

# ifeq ($(TAP1_MAC),)
#   $(error 无法获取 tap1 的 MAC 地址。请确保 tap1 接口存在且已配置。)
# endif

# 动态获取 br0 的 IP 和网关
BR_IP_EXIST = $(shell ip -4 addr show "$(BR_IF)" | grep -oP '(?<=inet\s)\d+(\.\d+){3}' | head -n1)
BR_GW_EXIST = $(shell ip route | grep "^default.*$(BR_IF)" | awk '{print $$3}' | head -n1)

# 设置默认值（可选）
BR_IP_EXIST ?= "0.0.0.0"
BR_GW_EXIST ?= "0.0.0.0"

# 获取 br0 的 CIDR 后缀
BR_CIDR = $(shell ip -4 addr show "$(BR_IF)" | grep -oP '(?<=inet\s)\d+(\.\d+){3}/\d+' | head -n1 | cut -d/ -f2)

# 将 CIDR 转换为子网掩码
BR_NETMASK_EXIST := $(shell \
	cidr=$$(echo $(BR_CIDR)); \
	mask=0; \
	for i in $$(seq 1 $$cidr); do \
		mask=$$((mask + (1 << (32 - i)))); \
	done; \
	printf "%d.%d.%d.%d" \
		$$(( (mask >> 24) & 0xFF )) \
		$$(( (mask >> 16) & 0xFF )) \
		$$(( (mask >> 8) & 0xFF )) \
		$$(( mask & 0xFF )) \
)

# 设置默认值
BR_NETMASK_EXIST ?= "0.0.0.0"

# 将这些信息添加到 DEFS
DEFS += -DPRINT_KERNEL_INFO
DEFS += -DBR0_IP=\"$(BR_IP_EXIST)\"
DEFS += -DBR0_GATEWAY=\"$(BR_GW_EXIST)\"
DEFS += -DBR0_NETMASK=\"$(BR_NETMASK_EXIST)\"
DEFS += -DTAP0_MAC=\"$(TAP0_MAC)\"
DEFS += -DTAP1_MAC=\"$(TAP1_MAC)\"

swap.img:
	${DD} if=/dev/zero of=$@ bs=1024M count=1
	# ${DD} if=/dev/urandom of=$@ bs=1024M count=1

mkfs: mkfs.c 
	gcc -g -Werror -Wall -o mkfs mkfs.c


# QEMU 选项，使用递归变量
QEMUOPTS = -machine virt -bios none -kernel ${KERNEL_ELF} -m 1024M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -device virtio-rng-device,bus=virtio-mmio-bus.0
QEMUOPTS += -drive file=swap.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.1
QEMUOPTS += -drive file=disk0.img,if=none,format=raw,id=x1
QEMUOPTS += -device virtio-blk-device,drive=x1,bus=virtio-mmio-bus.2

QEMUOPTS += -netdev tap,id=net0,ifname=tap0,script=no,downscript=no
QEMUOPTS += -object filter-dump,id=net0,netdev=net0,file=$(KERNEL_PATH)/packets0.pcap
QEMUOPTS += -device virtio-net-device,netdev=net0,mac=$(TAP0_MAC),bus=virtio-mmio-bus.3

QEMUOPTS += -netdev tap,id=net1,ifname=tap1,script=no,downscript=no
QEMUOPTS += -object filter-dump,id=net1,netdev=net1,file=$(KERNEL_PATH)/packets1.pcap
QEMUOPTS += -device virtio-net-device,netdev=net1,mac=$(TAP1_MAC),bus=virtio-mmio-bus.4

# 添加C文件的编译规则
define K_compile_c_file
$(CC) ${DEFS} ${K_INCLUDE} ${CFLAGS} -c -o ${KERNEL_PATH}/$(notdir $(1:.c=.o)) $(1)
endef

# 添加汇编文件的编译规则
define K_compile_asm_file
$(CC) ${K_INCLUDE} ${CFLAGS} -c -o ${KERNEL_PATH}/$(notdir $(1:.S=.o)) $(1)
endef

# 生成所有C文件的目标文件
K_compile_c: $(K_SRCS_C)
	$(foreach src, $(K_SRCS_C), $(call K_compile_c_file, $(src));)

# 生成所有汇编文件的目标文件
K_compile_S: $(K_SRCS_ASM)
	$(foreach src, $(K_SRCS_ASM), $(call K_compile_asm_file, $(src));)

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

# USER......................................................

U=code/user
U_OUTPUT_PATH = ${U}/output
U_CMD_PATH = ${U}/cmd
U_LIBS_PATH = ${U}/libs

U_INCLUDE = -I${U_LIBS_PATH} 
U_INCLUDE += -I./code/kernel/config

USER_DEFS += -DCPUS=$(CPUS)

${U_OUTPUT_PATH}:
	@${MKDIR} $@

# usys 目标生成 usys.S 文件
$U/libs/usys.S: $U/usys.py
	python3 $U/usys.py > $U/libs/usys.S

# 获取 U_LIBS_PATH 下的所有 .c 文件
USER_LIB_C_SRCS = $(wildcard ${U_LIBS_PATH}/*.c)

# 先执行 usys 目标，确保 usys.S 文件存在
USER_LIB_ASM_SRCS = $(wildcard ${U_LIBS_PATH}/*.S) $U/libs/usys.S

# ifneq ($(filter ${U}/libs/usys.S,$(U_SRCS_ASM)),${U}/libs/usys.S)
# U_SRCS_ASM += ${U}/libs/usys.S
# endif

ULDFLAGS ?= -T user.ld

ULDFLAGS += -z max-page-size=4096

# 获取 U_CMD_PATH 下的所有 .c 文件
USER_CMD_SRCS = $(wildcard ${U_CMD_PATH}/*.c)

# 提取所有命令的目标对象文件
USER_CMD_OBJS = $(addprefix ${U_OUTPUT_PATH}/, $(patsubst %.c, %.o, $(notdir ${USER_CMD_SRCS})))
USER_LIB_C_OBJS = $(addprefix ${U_OUTPUT_PATH}/, $(patsubst %.c, %.o, $(notdir ${USER_LIB_C_SRCS})))
USER_LIB_ASM_OBJS = $(addprefix ${U_OUTPUT_PATH}/, $(patsubst %.S, %.o, $(notdir ${USER_LIB_ASM_SRCS})))

# 通用的编译规则，用于编译所有命令的 C 文件
define CMD_compile_c_file
$(CC) ${USER_DEFS} ${U_INCLUDE} ${CFLAGS} -c -o ${U_OUTPUT_PATH}/$(notdir $(1:.c=.o)) $(1)
endef

# 生成所有命令的目标文件
CMD_compile_c: $(USER_CMD_SRCS)
	$(foreach src, $(USER_CMD_SRCS), $(call CMD_compile_c_file, $(src));)

# 编译库中 C 文件的规则
define U_compile_c_file
$(CC) ${USER_DEFS} ${U_INCLUDE} ${CFLAGS} -c -o ${U_OUTPUT_PATH}/$(notdir $(1:.c=.o)) $(1)
endef

# 编译库中汇编文件的规则
define U_compile_asm_file
$(CC) ${U_INCLUDE} ${CFLAGS} -c -o ${U_OUTPUT_PATH}/$(notdir $(1:.S=.o)) $(1)
endef

# 生成库中的目标文件
U_compile: $(USER_LIB_C_SRCS) $(USER_LIB_ASM_SRCS)
	$(foreach src, $(USER_LIB_C_SRCS), $(call U_compile_c_file, $(src));)
	$(foreach src, $(USER_LIB_ASM_SRCS), $(call U_compile_asm_file, $(src));)

# 链接成静态库 libulib.a
USER_LIB = ${U_OUTPUT_PATH}/libulib.a
$(USER_LIB): U_compile
	$(AR) rcs $(USER_LIB) $(USER_LIB_C_OBJS) $(USER_LIB_ASM_OBJS)

# 通用的链接规则，用于生成每个命令的 ELF 文件，文件名前加上 "_"
define CMD_link_elf_file
$(LD) ${ULDFLAGS} -o ${U_OUTPUT_PATH}/_$(notdir $(1:.c=)) ${U_OUTPUT_PATH}/$(notdir $(1:.c=.o)) $(USER_LIB)
$(OBJDUMP) -S ${U_OUTPUT_PATH}/_$(notdir $(1:.c=)) > ${U_OUTPUT_PATH}/$(notdir $(1:.c=)).asm
$(OBJDUMP) -t ${U_OUTPUT_PATH}/_$(notdir $(1:.c=)) | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > ${U_OUTPUT_PATH}/$(notdir $(1:.c=)).sym
${OBJCOPY} -O binary ${U_OUTPUT_PATH}/_$(notdir $(1:.c=)) ${U_OUTPUT_PATH}/$(notdir $(1:.c=)).bin
endef

# 为所有命令生成 ELF 文件
CMD_link_all: ${U_OUTPUT_PATH} CMD_compile_c $(USER_LIB)
	$(foreach cmd, $(USER_CMD_SRCS), $(call CMD_link_elf_file, $(cmd));)

cmd_target_path = $(addprefix ${U_OUTPUT_PATH}/_, $(notdir $(USER_CMD_SRCS:.c=)))

# 默认目标，先执行 usys 目标，再执行 CMD_link_all
user: $U/libs/usys.S CMD_link_all

disk0.img: CMD_link_all mkfs README 
	./mkfs disk0.img README $(cmd_target_path)

#END USER......................................................................

.PHONY: uptap.sh

uptap.sh:
	@echo "Executing ./uptap.sh..."
	./uptap.sh

kernel: uptap.sh ${KERNEL_ELF}

qemu: uptap.sh clean kernel swap.img disk0.img 
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: uptap.sh clean kernel .gdbinit swap.img disk0.img 
	@echo "*** Now run 'gdb' in another window." 1>&2
	@echo "$(QEMUGDB)"
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)	

debug: uptap.sh clean kernel swap.img disk0.img 
	@echo "Press Ctrl-C and then input 'quit' to exit GDB and QEMU"
	@echo "-------------------------------------------------------"
	@${QEMU} ${QEMUOPTS} -s -S &
	@${GDB} ${KERNEL_ELF} -q -x gdbinit

clean:
	rm -rf .gdbinit $K/test/usys.S $(KERNEL_PATH) kernel.p $U/libs/usys.S $(U_OUTPUT_PATH)

all-clean: clean
	rm -rf *.img *.txt mkfs
