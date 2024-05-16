
code/kernel/kernel_output/kernel.elf:     file format elf64-littleriscv


Disassembly of section .text:

0000000080000000 <_entry>:
	.global	_entry
.section .text

_entry:
    la sp, stack0
    80000000:	00001117          	auipc	sp,0x1
    80000004:	19010113          	addi	sp,sp,400 # 80001190 <stack0>
    li a0, 1024*4
    80000008:	6505                	lui	a0,0x1
	csrr a1, mhartid
    8000000a:	f14025f3          	csrr	a1,mhartid
    addi a1, a1, 1
    8000000e:	0585                	addi	a1,a1,1
    mul a0, a0, a1
    80000010:	02b50533          	mul	a0,a0,a1
    add sp, sp, a0
    80000014:	912a                	add	sp,sp,a0
	# jump to start_kernel() in start.c
    call start_kernel
    80000016:	0a6000ef          	jal	ra,800000bc <start_kernel>

000000008000001a <spin>:
spin:
        j spin
    8000001a:	a001                	j	8000001a <spin>

000000008000001c <test>:
#include "stdio.h"
#include "uart.h"

__attribute__((aligned(16))) char stack0[4096 * MAXNUM_CPU];

void test(void) {
    8000001c:	1101                	addi	sp,sp,-32
    8000001e:	ec06                	sd	ra,24(sp)
    80000020:	e822                	sd	s0,16(sp)
    80000022:	1000                	addi	s0,sp,32
    int version = 20230917;
    80000024:	0134b7b7          	lui	a5,0x134b
    80000028:	3057879b          	addiw	a5,a5,773
    8000002c:	fef42223          	sw	a5,-28(s0)
    char *hello = "Hello, qemu and risc-v!";
    80000030:	00001797          	auipc	a5,0x1
    80000034:	e4078793          	addi	a5,a5,-448 # 80000e70 <strnlen+0x48>
    80000038:	fef43423          	sd	a5,-24(s0)
    cprintf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    8000003c:	00001517          	auipc	a0,0x1
    80000040:	e4c50513          	addi	a0,a0,-436 # 80000e88 <strnlen+0x60>
    80000044:	00000097          	auipc	ra,0x0
    80000048:	206080e7          	jalr	518(ra) # 8000024a <cprintf>
    cprintf("  version is: %d\n", version);
    8000004c:	fe442783          	lw	a5,-28(s0)
    80000050:	85be                	mv	a1,a5
    80000052:	00001517          	auipc	a0,0x1
    80000056:	e5e50513          	addi	a0,a0,-418 # 80000eb0 <strnlen+0x88>
    8000005a:	00000097          	auipc	ra,0x0
    8000005e:	1f0080e7          	jalr	496(ra) # 8000024a <cprintf>
    cprintf("  version is: 0x%08x\n", version);
    80000062:	fe442783          	lw	a5,-28(s0)
    80000066:	85be                	mv	a1,a5
    80000068:	00001517          	auipc	a0,0x1
    8000006c:	e6050513          	addi	a0,a0,-416 # 80000ec8 <strnlen+0xa0>
    80000070:	00000097          	auipc	ra,0x0
    80000074:	1da080e7          	jalr	474(ra) # 8000024a <cprintf>
    cprintf("  pointer is: %p\n", &version);
    80000078:	fe440793          	addi	a5,s0,-28
    8000007c:	85be                	mv	a1,a5
    8000007e:	00001517          	auipc	a0,0x1
    80000082:	e6250513          	addi	a0,a0,-414 # 80000ee0 <strnlen+0xb8>
    80000086:	00000097          	auipc	ra,0x0
    8000008a:	1c4080e7          	jalr	452(ra) # 8000024a <cprintf>
    cprintf("  %s\n", hello);
    8000008e:	fe843583          	ld	a1,-24(s0)
    80000092:	00001517          	auipc	a0,0x1
    80000096:	e6650513          	addi	a0,a0,-410 # 80000ef8 <strnlen+0xd0>
    8000009a:	00000097          	auipc	ra,0x0
    8000009e:	1b0080e7          	jalr	432(ra) # 8000024a <cprintf>
    cprintf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    800000a2:	00001517          	auipc	a0,0x1
    800000a6:	e5e50513          	addi	a0,a0,-418 # 80000f00 <strnlen+0xd8>
    800000aa:	00000097          	auipc	ra,0x0
    800000ae:	1a0080e7          	jalr	416(ra) # 8000024a <cprintf>
}
    800000b2:	0001                	nop
    800000b4:	60e2                	ld	ra,24(sp)
    800000b6:	6442                	ld	s0,16(sp)
    800000b8:	6105                	addi	sp,sp,32
    800000ba:	8082                	ret

00000000800000bc <start_kernel>:

void start_kernel(void) {
    800000bc:	1141                	addi	sp,sp,-16
    800000be:	e406                	sd	ra,8(sp)
    800000c0:	e022                	sd	s0,0(sp)
    800000c2:	0800                	addi	s0,sp,16
    uart_init();
    800000c4:	00000097          	auipc	ra,0x0
    800000c8:	022080e7          	jalr	34(ra) # 800000e6 <uart_init>
    // uart_puts("Hello, DDOS!\n");
    cputs("Hello, DDOS!");
    800000cc:	00001517          	auipc	a0,0x1
    800000d0:	e5c50513          	addi	a0,a0,-420 # 80000f28 <strnlen+0x100>
    800000d4:	00000097          	auipc	ra,0x0
    800000d8:	1f8080e7          	jalr	504(ra) # 800002cc <cputs>
    test();
    800000dc:	00000097          	auipc	ra,0x0
    800000e0:	f40080e7          	jalr	-192(ra) # 8000001c <test>
    while (1) {};  // stop here!
    800000e4:	a001                	j	800000e4 <start_kernel+0x28>

00000000800000e6 <uart_init>:
#define LSR_TX_IDLE (1 << 5)

#define uart_read_reg(reg) (*(UART_REG(reg)))
#define uart_write_reg(reg, v) (*(UART_REG(reg)) = (v))

void uart_init() {
    800000e6:	1101                	addi	sp,sp,-32
    800000e8:	ec22                	sd	s0,24(sp)
    800000ea:	1000                	addi	s0,sp,32
    /* disable interrupts. */
    uart_write_reg(IER, 0x00);
    800000ec:	100007b7          	lui	a5,0x10000
    800000f0:	0785                	addi	a5,a5,1
    800000f2:	00078023          	sb	zero,0(a5) # 10000000 <_entry-0x70000000>
    uint8_t lcr = uart_read_reg(LCR);
    800000f6:	100007b7          	lui	a5,0x10000
    800000fa:	078d                	addi	a5,a5,3
    800000fc:	0007c783          	lbu	a5,0(a5) # 10000000 <_entry-0x70000000>
    80000100:	fef407a3          	sb	a5,-17(s0)
    uart_write_reg(LCR, lcr | (1 << 7));
    80000104:	100007b7          	lui	a5,0x10000
    80000108:	078d                	addi	a5,a5,3
    8000010a:	fef44703          	lbu	a4,-17(s0)
    8000010e:	f8076713          	ori	a4,a4,-128
    80000112:	0ff77713          	andi	a4,a4,255
    80000116:	00e78023          	sb	a4,0(a5) # 10000000 <_entry-0x70000000>
    uart_write_reg(DLL, 0x03);
    8000011a:	100007b7          	lui	a5,0x10000
    8000011e:	470d                	li	a4,3
    80000120:	00e78023          	sb	a4,0(a5) # 10000000 <_entry-0x70000000>
    uart_write_reg(DLM, 0x00);
    80000124:	100007b7          	lui	a5,0x10000
    80000128:	0785                	addi	a5,a5,1
    8000012a:	00078023          	sb	zero,0(a5) # 10000000 <_entry-0x70000000>
     * - number of stop bits：1 bit when word length is 8 bits
     * - no parity
     * - no break control
     * - disabled baud latch
     */
    lcr = 0;
    8000012e:	fe0407a3          	sb	zero,-17(s0)
    uart_write_reg(LCR, lcr | (3 << 0));
    80000132:	100007b7          	lui	a5,0x10000
    80000136:	078d                	addi	a5,a5,3
    80000138:	fef44703          	lbu	a4,-17(s0)
    8000013c:	00376713          	ori	a4,a4,3
    80000140:	0ff77713          	andi	a4,a4,255
    80000144:	00e78023          	sb	a4,0(a5) # 10000000 <_entry-0x70000000>
}
    80000148:	0001                	nop
    8000014a:	6462                	ld	s0,24(sp)
    8000014c:	6105                	addi	sp,sp,32
    8000014e:	8082                	ret

0000000080000150 <uart_putc>:

int uart_putc(char ch) {
    80000150:	1101                	addi	sp,sp,-32
    80000152:	ec22                	sd	s0,24(sp)
    80000154:	1000                	addi	s0,sp,32
    80000156:	87aa                	mv	a5,a0
    80000158:	fef407a3          	sb	a5,-17(s0)
    while ((uart_read_reg(LSR) & LSR_TX_IDLE) == 0);
    8000015c:	0001                	nop
    8000015e:	100007b7          	lui	a5,0x10000
    80000162:	0795                	addi	a5,a5,5
    80000164:	0007c783          	lbu	a5,0(a5) # 10000000 <_entry-0x70000000>
    80000168:	0ff7f793          	andi	a5,a5,255
    8000016c:	2781                	sext.w	a5,a5
    8000016e:	0207f793          	andi	a5,a5,32
    80000172:	2781                	sext.w	a5,a5
    80000174:	d7ed                	beqz	a5,8000015e <uart_putc+0xe>
    return uart_write_reg(THR, ch);
    80000176:	10000737          	lui	a4,0x10000
    8000017a:	fef44783          	lbu	a5,-17(s0)
    8000017e:	00f70023          	sb	a5,0(a4) # 10000000 <_entry-0x70000000>
    80000182:	2781                	sext.w	a5,a5
}
    80000184:	853e                	mv	a0,a5
    80000186:	6462                	ld	s0,24(sp)
    80000188:	6105                	addi	sp,sp,32
    8000018a:	8082                	ret

000000008000018c <uart_puts>:

void uart_puts(char *s) {
    8000018c:	1101                	addi	sp,sp,-32
    8000018e:	ec06                	sd	ra,24(sp)
    80000190:	e822                	sd	s0,16(sp)
    80000192:	1000                	addi	s0,sp,32
    80000194:	fea43423          	sd	a0,-24(s0)
    while (*s) { uart_putc(*s++); }
    80000198:	a831                	j	800001b4 <uart_puts+0x28>
    8000019a:	fe843783          	ld	a5,-24(s0)
    8000019e:	00178713          	addi	a4,a5,1
    800001a2:	fee43423          	sd	a4,-24(s0)
    800001a6:	0007c783          	lbu	a5,0(a5)
    800001aa:	853e                	mv	a0,a5
    800001ac:	00000097          	auipc	ra,0x0
    800001b0:	fa4080e7          	jalr	-92(ra) # 80000150 <uart_putc>
    800001b4:	fe843783          	ld	a5,-24(s0)
    800001b8:	0007c783          	lbu	a5,0(a5)
    800001bc:	fff9                	bnez	a5,8000019a <uart_puts+0xe>
}
    800001be:	0001                	nop
    800001c0:	0001                	nop
    800001c2:	60e2                	ld	ra,24(sp)
    800001c4:	6442                	ld	s0,16(sp)
    800001c6:	6105                	addi	sp,sp,32
    800001c8:	8082                	ret

00000000800001ca <cputch>:
#include "uart.h"
#include "stdio.h"
#include "printf.h"

static void cputch(int c, int *cnt){
    800001ca:	1101                	addi	sp,sp,-32
    800001cc:	ec06                	sd	ra,24(sp)
    800001ce:	e822                	sd	s0,16(sp)
    800001d0:	1000                	addi	s0,sp,32
    800001d2:	87aa                	mv	a5,a0
    800001d4:	feb43023          	sd	a1,-32(s0)
    800001d8:	fef42623          	sw	a5,-20(s0)
    uart_putc(c);
    800001dc:	fec42783          	lw	a5,-20(s0)
    800001e0:	0ff7f793          	andi	a5,a5,255
    800001e4:	853e                	mv	a0,a5
    800001e6:	00000097          	auipc	ra,0x0
    800001ea:	f6a080e7          	jalr	-150(ra) # 80000150 <uart_putc>
    (*cnt)++;
    800001ee:	fe043783          	ld	a5,-32(s0)
    800001f2:	439c                	lw	a5,0(a5)
    800001f4:	2785                	addiw	a5,a5,1
    800001f6:	0007871b          	sext.w	a4,a5
    800001fa:	fe043783          	ld	a5,-32(s0)
    800001fe:	c398                	sw	a4,0(a5)
}
    80000200:	0001                	nop
    80000202:	60e2                	ld	ra,24(sp)
    80000204:	6442                	ld	s0,16(sp)
    80000206:	6105                	addi	sp,sp,32
    80000208:	8082                	ret

000000008000020a <vcprintf>:

int vcprintf(const char *fmt, va_list ap){
    8000020a:	7179                	addi	sp,sp,-48
    8000020c:	f406                	sd	ra,40(sp)
    8000020e:	f022                	sd	s0,32(sp)
    80000210:	1800                	addi	s0,sp,48
    80000212:	fca43c23          	sd	a0,-40(s0)
    80000216:	fcb43823          	sd	a1,-48(s0)
    int cnt = 0;
    8000021a:	fe042623          	sw	zero,-20(s0)
    vprintfmt((void (*)(int, void *))cputch, &cnt, fmt, ap);
    8000021e:	fec40793          	addi	a5,s0,-20
    80000222:	fd043683          	ld	a3,-48(s0)
    80000226:	fd843603          	ld	a2,-40(s0)
    8000022a:	85be                	mv	a1,a5
    8000022c:	00000517          	auipc	a0,0x0
    80000230:	f9e50513          	addi	a0,a0,-98 # 800001ca <cputch>
    80000234:	00000097          	auipc	ra,0x0
    80000238:	2f8080e7          	jalr	760(ra) # 8000052c <vprintfmt>
    return cnt;
    8000023c:	fec42783          	lw	a5,-20(s0)
}
    80000240:	853e                	mv	a0,a5
    80000242:	70a2                	ld	ra,40(sp)
    80000244:	7402                	ld	s0,32(sp)
    80000246:	6145                	addi	sp,sp,48
    80000248:	8082                	ret

000000008000024a <cprintf>:

int cprintf(const char *fmt, ...){
    8000024a:	7159                	addi	sp,sp,-112
    8000024c:	f406                	sd	ra,40(sp)
    8000024e:	f022                	sd	s0,32(sp)
    80000250:	1800                	addi	s0,sp,48
    80000252:	fca43c23          	sd	a0,-40(s0)
    80000256:	e40c                	sd	a1,8(s0)
    80000258:	e810                	sd	a2,16(s0)
    8000025a:	ec14                	sd	a3,24(s0)
    8000025c:	f018                	sd	a4,32(s0)
    8000025e:	f41c                	sd	a5,40(s0)
    80000260:	03043823          	sd	a6,48(s0)
    80000264:	03143c23          	sd	a7,56(s0)
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    80000268:	04040793          	addi	a5,s0,64
    8000026c:	fcf43823          	sd	a5,-48(s0)
    80000270:	fd043783          	ld	a5,-48(s0)
    80000274:	fc878793          	addi	a5,a5,-56
    80000278:	fef43023          	sd	a5,-32(s0)
    cnt = vcprintf(fmt, ap);
    8000027c:	fe043783          	ld	a5,-32(s0)
    80000280:	85be                	mv	a1,a5
    80000282:	fd843503          	ld	a0,-40(s0)
    80000286:	00000097          	auipc	ra,0x0
    8000028a:	f84080e7          	jalr	-124(ra) # 8000020a <vcprintf>
    8000028e:	87aa                	mv	a5,a0
    80000290:	fef42623          	sw	a5,-20(s0)
    va_end(ap);
    return cnt;
    80000294:	fec42783          	lw	a5,-20(s0)
}
    80000298:	853e                	mv	a0,a5
    8000029a:	70a2                	ld	ra,40(sp)
    8000029c:	7402                	ld	s0,32(sp)
    8000029e:	6165                	addi	sp,sp,112
    800002a0:	8082                	ret

00000000800002a2 <cputchar>:

void cputchar(int c){
    800002a2:	1101                	addi	sp,sp,-32
    800002a4:	ec06                	sd	ra,24(sp)
    800002a6:	e822                	sd	s0,16(sp)
    800002a8:	1000                	addi	s0,sp,32
    800002aa:	87aa                	mv	a5,a0
    800002ac:	fef42623          	sw	a5,-20(s0)
    uart_putc(c);
    800002b0:	fec42783          	lw	a5,-20(s0)
    800002b4:	0ff7f793          	andi	a5,a5,255
    800002b8:	853e                	mv	a0,a5
    800002ba:	00000097          	auipc	ra,0x0
    800002be:	e96080e7          	jalr	-362(ra) # 80000150 <uart_putc>
}
    800002c2:	0001                	nop
    800002c4:	60e2                	ld	ra,24(sp)
    800002c6:	6442                	ld	s0,16(sp)
    800002c8:	6105                	addi	sp,sp,32
    800002ca:	8082                	ret

00000000800002cc <cputs>:

int cputs(const char *str){
    800002cc:	7179                	addi	sp,sp,-48
    800002ce:	f406                	sd	ra,40(sp)
    800002d0:	f022                	sd	s0,32(sp)
    800002d2:	1800                	addi	s0,sp,48
    800002d4:	fca43c23          	sd	a0,-40(s0)
    int cnt = 0;
    800002d8:	fe042423          	sw	zero,-24(s0)
    char c;
    while((c = *str++) != '\0')
    800002dc:	a821                	j	800002f4 <cputs+0x28>
        cputch(c, &cnt);
    800002de:	fef44783          	lbu	a5,-17(s0)
    800002e2:	2781                	sext.w	a5,a5
    800002e4:	fe840713          	addi	a4,s0,-24
    800002e8:	85ba                	mv	a1,a4
    800002ea:	853e                	mv	a0,a5
    800002ec:	00000097          	auipc	ra,0x0
    800002f0:	ede080e7          	jalr	-290(ra) # 800001ca <cputch>
    while((c = *str++) != '\0')
    800002f4:	fd843783          	ld	a5,-40(s0)
    800002f8:	00178713          	addi	a4,a5,1
    800002fc:	fce43c23          	sd	a4,-40(s0)
    80000300:	0007c783          	lbu	a5,0(a5)
    80000304:	fef407a3          	sb	a5,-17(s0)
    80000308:	fef44783          	lbu	a5,-17(s0)
    8000030c:	0ff7f793          	andi	a5,a5,255
    80000310:	f7f9                	bnez	a5,800002de <cputs+0x12>
    cputch('\n', &cnt);
    80000312:	fe840793          	addi	a5,s0,-24
    80000316:	85be                	mv	a1,a5
    80000318:	4529                	li	a0,10
    8000031a:	00000097          	auipc	ra,0x0
    8000031e:	eb0080e7          	jalr	-336(ra) # 800001ca <cputch>
    return cnt;
    80000322:	fe842783          	lw	a5,-24(s0)
    80000326:	853e                	mv	a0,a5
    80000328:	70a2                	ld	ra,40(sp)
    8000032a:	7402                	ld	s0,32(sp)
    8000032c:	6145                	addi	sp,sp,48
    8000032e:	8082                	ret

0000000080000330 <printnum>:
    [E_NO_FREE_PROC] = "out of processes",
    [E_FAULT] = "segmentation fault",
};

static void printnum(void (*putch)(int, void *), void *putdat, uint64_t num, unsigned base, int width,
                     int padc) {
    80000330:	715d                	addi	sp,sp,-80
    80000332:	e486                	sd	ra,72(sp)
    80000334:	e0a2                	sd	s0,64(sp)
    80000336:	0880                	addi	s0,sp,80
    80000338:	fca43c23          	sd	a0,-40(s0)
    8000033c:	fcb43823          	sd	a1,-48(s0)
    80000340:	fcc43423          	sd	a2,-56(s0)
    80000344:	8636                	mv	a2,a3
    80000346:	86ba                	mv	a3,a4
    80000348:	873e                	mv	a4,a5
    8000034a:	87b2                	mv	a5,a2
    8000034c:	fcf42223          	sw	a5,-60(s0)
    80000350:	87b6                	mv	a5,a3
    80000352:	fcf42023          	sw	a5,-64(s0)
    80000356:	87ba                	mv	a5,a4
    80000358:	faf42e23          	sw	a5,-68(s0)
    uint64_t result = num / base;
    8000035c:	fc446783          	lwu	a5,-60(s0)
    80000360:	fc843703          	ld	a4,-56(s0)
    80000364:	02f757b3          	divu	a5,a4,a5
    80000368:	fef43423          	sd	a5,-24(s0)
    unsigned mod = num % base;
    8000036c:	fc446783          	lwu	a5,-60(s0)
    80000370:	fc843703          	ld	a4,-56(s0)
    80000374:	02f777b3          	remu	a5,a4,a5
    80000378:	fef42223          	sw	a5,-28(s0)
    if (num >= base)
    8000037c:	fc446783          	lwu	a5,-60(s0)
    80000380:	fc843703          	ld	a4,-56(s0)
    80000384:	02f76e63          	bltu	a4,a5,800003c0 <printnum+0x90>
        printnum(putch, putdat, result, base, width - 1, padc);
    80000388:	fc042783          	lw	a5,-64(s0)
    8000038c:	37fd                	addiw	a5,a5,-1
    8000038e:	0007871b          	sext.w	a4,a5
    80000392:	fbc42783          	lw	a5,-68(s0)
    80000396:	fc442683          	lw	a3,-60(s0)
    8000039a:	fe843603          	ld	a2,-24(s0)
    8000039e:	fd043583          	ld	a1,-48(s0)
    800003a2:	fd843503          	ld	a0,-40(s0)
    800003a6:	00000097          	auipc	ra,0x0
    800003aa:	f8a080e7          	jalr	-118(ra) # 80000330 <printnum>
    800003ae:	a01d                	j	800003d4 <printnum+0xa4>
    else {
        while (--width > 0) putch(padc, putdat);
    800003b0:	fbc42783          	lw	a5,-68(s0)
    800003b4:	fd843703          	ld	a4,-40(s0)
    800003b8:	fd043583          	ld	a1,-48(s0)
    800003bc:	853e                	mv	a0,a5
    800003be:	9702                	jalr	a4
    800003c0:	fc042783          	lw	a5,-64(s0)
    800003c4:	37fd                	addiw	a5,a5,-1
    800003c6:	fcf42023          	sw	a5,-64(s0)
    800003ca:	fc042783          	lw	a5,-64(s0)
    800003ce:	2781                	sext.w	a5,a5
    800003d0:	fef040e3          	bgtz	a5,800003b0 <printnum+0x80>
    }
    putch("0123456789abcdef"[mod], putdat);
    800003d4:	00001717          	auipc	a4,0x1
    800003d8:	c1c70713          	addi	a4,a4,-996 # 80000ff0 <error_string+0x38>
    800003dc:	fe446783          	lwu	a5,-28(s0)
    800003e0:	97ba                	add	a5,a5,a4
    800003e2:	0007c783          	lbu	a5,0(a5)
    800003e6:	2781                	sext.w	a5,a5
    800003e8:	fd843703          	ld	a4,-40(s0)
    800003ec:	fd043583          	ld	a1,-48(s0)
    800003f0:	853e                	mv	a0,a5
    800003f2:	9702                	jalr	a4
}
    800003f4:	0001                	nop
    800003f6:	60a6                	ld	ra,72(sp)
    800003f8:	6406                	ld	s0,64(sp)
    800003fa:	6161                	addi	sp,sp,80
    800003fc:	8082                	ret

00000000800003fe <getuint>:

static uint64_t getuint(va_list *ap, int lflag) {
    800003fe:	1101                	addi	sp,sp,-32
    80000400:	ec22                	sd	s0,24(sp)
    80000402:	1000                	addi	s0,sp,32
    80000404:	fea43423          	sd	a0,-24(s0)
    80000408:	87ae                	mv	a5,a1
    8000040a:	fef42223          	sw	a5,-28(s0)
    if (lflag >= 2)
    8000040e:	fe442783          	lw	a5,-28(s0)
    80000412:	0007871b          	sext.w	a4,a5
    80000416:	4785                	li	a5,1
    80000418:	00e7dc63          	bge	a5,a4,80000430 <getuint+0x32>
        return va_arg(*ap, uint64_t);
    8000041c:	fe843783          	ld	a5,-24(s0)
    80000420:	639c                	ld	a5,0(a5)
    80000422:	00878693          	addi	a3,a5,8
    80000426:	fe843703          	ld	a4,-24(s0)
    8000042a:	e314                	sd	a3,0(a4)
    8000042c:	639c                	ld	a5,0(a5)
    8000042e:	a815                	j	80000462 <getuint+0x64>
    else if (lflag)
    80000430:	fe442783          	lw	a5,-28(s0)
    80000434:	2781                	sext.w	a5,a5
    80000436:	cb99                	beqz	a5,8000044c <getuint+0x4e>
        return va_arg(*ap, unsigned long);
    80000438:	fe843783          	ld	a5,-24(s0)
    8000043c:	639c                	ld	a5,0(a5)
    8000043e:	00878693          	addi	a3,a5,8
    80000442:	fe843703          	ld	a4,-24(s0)
    80000446:	e314                	sd	a3,0(a4)
    80000448:	639c                	ld	a5,0(a5)
    8000044a:	a821                	j	80000462 <getuint+0x64>
    else
        return va_arg(*ap, uint);
    8000044c:	fe843783          	ld	a5,-24(s0)
    80000450:	639c                	ld	a5,0(a5)
    80000452:	00878693          	addi	a3,a5,8
    80000456:	fe843703          	ld	a4,-24(s0)
    8000045a:	e314                	sd	a3,0(a4)
    8000045c:	439c                	lw	a5,0(a5)
    8000045e:	1782                	slli	a5,a5,0x20
    80000460:	9381                	srli	a5,a5,0x20
}
    80000462:	853e                	mv	a0,a5
    80000464:	6462                	ld	s0,24(sp)
    80000466:	6105                	addi	sp,sp,32
    80000468:	8082                	ret

000000008000046a <getint>:

static int64_t getint(va_list *ap, int lflag) {
    8000046a:	1101                	addi	sp,sp,-32
    8000046c:	ec22                	sd	s0,24(sp)
    8000046e:	1000                	addi	s0,sp,32
    80000470:	fea43423          	sd	a0,-24(s0)
    80000474:	87ae                	mv	a5,a1
    80000476:	fef42223          	sw	a5,-28(s0)
    if (lflag >= 2)
    8000047a:	fe442783          	lw	a5,-28(s0)
    8000047e:	0007871b          	sext.w	a4,a5
    80000482:	4785                	li	a5,1
    80000484:	00e7dc63          	bge	a5,a4,8000049c <getint+0x32>
        return va_arg(*ap, int64_t);
    80000488:	fe843783          	ld	a5,-24(s0)
    8000048c:	639c                	ld	a5,0(a5)
    8000048e:	00878693          	addi	a3,a5,8
    80000492:	fe843703          	ld	a4,-24(s0)
    80000496:	e314                	sd	a3,0(a4)
    80000498:	639c                	ld	a5,0(a5)
    8000049a:	a805                	j	800004ca <getint+0x60>
    else if (lflag)
    8000049c:	fe442783          	lw	a5,-28(s0)
    800004a0:	2781                	sext.w	a5,a5
    800004a2:	cb99                	beqz	a5,800004b8 <getint+0x4e>
        return va_arg(*ap, long);
    800004a4:	fe843783          	ld	a5,-24(s0)
    800004a8:	639c                	ld	a5,0(a5)
    800004aa:	00878693          	addi	a3,a5,8
    800004ae:	fe843703          	ld	a4,-24(s0)
    800004b2:	e314                	sd	a3,0(a4)
    800004b4:	639c                	ld	a5,0(a5)
    800004b6:	a811                	j	800004ca <getint+0x60>
    else
        return va_arg(*ap, int);
    800004b8:	fe843783          	ld	a5,-24(s0)
    800004bc:	639c                	ld	a5,0(a5)
    800004be:	00878693          	addi	a3,a5,8
    800004c2:	fe843703          	ld	a4,-24(s0)
    800004c6:	e314                	sd	a3,0(a4)
    800004c8:	439c                	lw	a5,0(a5)
}
    800004ca:	853e                	mv	a0,a5
    800004cc:	6462                	ld	s0,24(sp)
    800004ce:	6105                	addi	sp,sp,32
    800004d0:	8082                	ret

00000000800004d2 <printfmt>:

void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...) {
    800004d2:	7159                	addi	sp,sp,-112
    800004d4:	fc06                	sd	ra,56(sp)
    800004d6:	f822                	sd	s0,48(sp)
    800004d8:	0080                	addi	s0,sp,64
    800004da:	fca43c23          	sd	a0,-40(s0)
    800004de:	fcb43823          	sd	a1,-48(s0)
    800004e2:	fcc43423          	sd	a2,-56(s0)
    800004e6:	e414                	sd	a3,8(s0)
    800004e8:	e818                	sd	a4,16(s0)
    800004ea:	ec1c                	sd	a5,24(s0)
    800004ec:	03043023          	sd	a6,32(s0)
    800004f0:	03143423          	sd	a7,40(s0)
    va_list ap;
    va_start(ap, fmt);
    800004f4:	03040793          	addi	a5,s0,48
    800004f8:	fcf43023          	sd	a5,-64(s0)
    800004fc:	fc043783          	ld	a5,-64(s0)
    80000500:	fd878793          	addi	a5,a5,-40
    80000504:	fef43423          	sd	a5,-24(s0)
    vprintfmt(putch, putdat, fmt, ap);
    80000508:	fe843783          	ld	a5,-24(s0)
    8000050c:	86be                	mv	a3,a5
    8000050e:	fc843603          	ld	a2,-56(s0)
    80000512:	fd043583          	ld	a1,-48(s0)
    80000516:	fd843503          	ld	a0,-40(s0)
    8000051a:	00000097          	auipc	ra,0x0
    8000051e:	012080e7          	jalr	18(ra) # 8000052c <vprintfmt>
    va_end(ap);
}
    80000522:	0001                	nop
    80000524:	70e2                	ld	ra,56(sp)
    80000526:	7442                	ld	s0,48(sp)
    80000528:	6165                	addi	sp,sp,112
    8000052a:	8082                	ret

000000008000052c <vprintfmt>:

void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap) {
    8000052c:	711d                	addi	sp,sp,-96
    8000052e:	ec86                	sd	ra,88(sp)
    80000530:	e8a2                	sd	s0,80(sp)
    80000532:	1080                	addi	s0,sp,96
    80000534:	faa43c23          	sd	a0,-72(s0)
    80000538:	fab43823          	sd	a1,-80(s0)
    8000053c:	fac43423          	sd	a2,-88(s0)
    80000540:	fad43023          	sd	a3,-96(s0)
    const char *p;
    int ch, err;
    uint64_t num;
    int base, width, precision, lflag, altflag;
    while (1) {
        while ((ch = *(uint8_t *)fmt++) != '%') {
    80000544:	a831                	j	80000560 <vprintfmt+0x34>
            if (ch == '\0') return;
    80000546:	fe442783          	lw	a5,-28(s0)
    8000054a:	2781                	sext.w	a5,a5
    8000054c:	48078463          	beqz	a5,800009d4 <vprintfmt+0x4a8>
            putch(ch, putdat);
    80000550:	fe442783          	lw	a5,-28(s0)
    80000554:	fb843703          	ld	a4,-72(s0)
    80000558:	fb043583          	ld	a1,-80(s0)
    8000055c:	853e                	mv	a0,a5
    8000055e:	9702                	jalr	a4
        while ((ch = *(uint8_t *)fmt++) != '%') {
    80000560:	fa843783          	ld	a5,-88(s0)
    80000564:	00178713          	addi	a4,a5,1
    80000568:	fae43423          	sd	a4,-88(s0)
    8000056c:	0007c783          	lbu	a5,0(a5)
    80000570:	fef42223          	sw	a5,-28(s0)
    80000574:	fe442783          	lw	a5,-28(s0)
    80000578:	0007871b          	sext.w	a4,a5
    8000057c:	02500793          	li	a5,37
    80000580:	fcf713e3          	bne	a4,a5,80000546 <vprintfmt+0x1a>
        }
        char padc = ' ';
    80000584:	02000793          	li	a5,32
    80000588:	fcf401a3          	sb	a5,-61(s0)
        width = precision = -1;
    8000058c:	57fd                	li	a5,-1
    8000058e:	fcf42623          	sw	a5,-52(s0)
    80000592:	fcc42783          	lw	a5,-52(s0)
    80000596:	fcf42823          	sw	a5,-48(s0)
        lflag = altflag = 0;
    8000059a:	fc042223          	sw	zero,-60(s0)
    8000059e:	fc442783          	lw	a5,-60(s0)
    800005a2:	fcf42423          	sw	a5,-56(s0)

    reswitch:
        switch (ch = *(uint8_t *)fmt++) {
    800005a6:	fa843783          	ld	a5,-88(s0)
    800005aa:	00178713          	addi	a4,a5,1
    800005ae:	fae43423          	sd	a4,-88(s0)
    800005b2:	0007c783          	lbu	a5,0(a5)
    800005b6:	fef42223          	sw	a5,-28(s0)
    800005ba:	fe442783          	lw	a5,-28(s0)
    800005be:	fdd7869b          	addiw	a3,a5,-35
    800005c2:	0006871b          	sext.w	a4,a3
    800005c6:	05500793          	li	a5,85
    800005ca:	3ce7e763          	bltu	a5,a4,80000998 <vprintfmt+0x46c>
    800005ce:	02069793          	slli	a5,a3,0x20
    800005d2:	9381                	srli	a5,a5,0x20
    800005d4:	00279713          	slli	a4,a5,0x2
    800005d8:	00001797          	auipc	a5,0x1
    800005dc:	a5478793          	addi	a5,a5,-1452 # 8000102c <error_string+0x74>
    800005e0:	97ba                	add	a5,a5,a4
    800005e2:	439c                	lw	a5,0(a5)
    800005e4:	0007871b          	sext.w	a4,a5
    800005e8:	00001797          	auipc	a5,0x1
    800005ec:	a4478793          	addi	a5,a5,-1468 # 8000102c <error_string+0x74>
    800005f0:	97ba                	add	a5,a5,a4
    800005f2:	8782                	jr	a5
            case '-': padc = '-'; goto reswitch;
    800005f4:	02d00793          	li	a5,45
    800005f8:	fcf401a3          	sb	a5,-61(s0)
    800005fc:	b76d                	j	800005a6 <vprintfmt+0x7a>
            case '0': padc = '0'; goto reswitch;
    800005fe:	03000793          	li	a5,48
    80000602:	fcf401a3          	sb	a5,-61(s0)
    80000606:	b745                	j	800005a6 <vprintfmt+0x7a>
            case '1' ... '9':
                for (precision = 0;; ++fmt) {
    80000608:	fc042623          	sw	zero,-52(s0)
                    precision = precision * 10 + ch - '0';
    8000060c:	fcc42703          	lw	a4,-52(s0)
    80000610:	87ba                	mv	a5,a4
    80000612:	0027979b          	slliw	a5,a5,0x2
    80000616:	9fb9                	addw	a5,a5,a4
    80000618:	0017979b          	slliw	a5,a5,0x1
    8000061c:	2781                	sext.w	a5,a5
    8000061e:	fe442703          	lw	a4,-28(s0)
    80000622:	9fb9                	addw	a5,a5,a4
    80000624:	2781                	sext.w	a5,a5
    80000626:	fd07879b          	addiw	a5,a5,-48
    8000062a:	fcf42623          	sw	a5,-52(s0)
                    ch = *fmt;
    8000062e:	fa843783          	ld	a5,-88(s0)
    80000632:	0007c783          	lbu	a5,0(a5)
    80000636:	fef42223          	sw	a5,-28(s0)
                    if (ch < '0' || ch > '9') break;
    8000063a:	fe442783          	lw	a5,-28(s0)
    8000063e:	0007871b          	sext.w	a4,a5
    80000642:	02f00793          	li	a5,47
    80000646:	04e7d863          	bge	a5,a4,80000696 <vprintfmt+0x16a>
    8000064a:	fe442783          	lw	a5,-28(s0)
    8000064e:	0007871b          	sext.w	a4,a5
    80000652:	03900793          	li	a5,57
    80000656:	04e7c063          	blt	a5,a4,80000696 <vprintfmt+0x16a>
                for (precision = 0;; ++fmt) {
    8000065a:	fa843783          	ld	a5,-88(s0)
    8000065e:	0785                	addi	a5,a5,1
    80000660:	faf43423          	sd	a5,-88(s0)
                    precision = precision * 10 + ch - '0';
    80000664:	b765                	j	8000060c <vprintfmt+0xe0>
                }
                goto process_precision;
            case '*': precision = va_arg(ap, int); goto process_precision;
    80000666:	fa043783          	ld	a5,-96(s0)
    8000066a:	00878713          	addi	a4,a5,8
    8000066e:	fae43023          	sd	a4,-96(s0)
    80000672:	439c                	lw	a5,0(a5)
    80000674:	fcf42623          	sw	a5,-52(s0)
    80000678:	a005                	j	80000698 <vprintfmt+0x16c>
            case '.': width = va_arg(ap, int); goto process_precision;
    8000067a:	fa043783          	ld	a5,-96(s0)
    8000067e:	00878713          	addi	a4,a5,8
    80000682:	fae43023          	sd	a4,-96(s0)
    80000686:	439c                	lw	a5,0(a5)
    80000688:	fcf42823          	sw	a5,-48(s0)
    8000068c:	a031                	j	80000698 <vprintfmt+0x16c>
            case '#':
                altflag = 1;
    8000068e:	4785                	li	a5,1
    80000690:	fcf42223          	sw	a5,-60(s0)
                goto reswitch;
    80000694:	bf09                	j	800005a6 <vprintfmt+0x7a>
                goto process_precision;
    80000696:	0001                	nop
            process_precision:
                if (width < 0) {
    80000698:	fd042783          	lw	a5,-48(s0)
    8000069c:	2781                	sext.w	a5,a5
    8000069e:	f007d4e3          	bgez	a5,800005a6 <vprintfmt+0x7a>
                    width = precision;
    800006a2:	fcc42783          	lw	a5,-52(s0)
    800006a6:	fcf42823          	sw	a5,-48(s0)
                    precision = -1;
    800006aa:	57fd                	li	a5,-1
    800006ac:	fcf42623          	sw	a5,-52(s0)
                }
                goto reswitch;
    800006b0:	bddd                	j	800005a6 <vprintfmt+0x7a>
            case 'l': lflag++; goto reswitch;
    800006b2:	fc842783          	lw	a5,-56(s0)
    800006b6:	2785                	addiw	a5,a5,1
    800006b8:	fcf42423          	sw	a5,-56(s0)
    800006bc:	b5ed                	j	800005a6 <vprintfmt+0x7a>
            case 'c':
                err = va_arg(ap, int);
    800006be:	fa043783          	ld	a5,-96(s0)
    800006c2:	00878713          	addi	a4,a5,8
    800006c6:	fae43023          	sd	a4,-96(s0)
    800006ca:	439c                	lw	a5,0(a5)
    800006cc:	fef42023          	sw	a5,-32(s0)
                if (err < 0) err = -err;
    800006d0:	fe042783          	lw	a5,-32(s0)
    800006d4:	2781                	sext.w	a5,a5
    800006d6:	0007d863          	bgez	a5,800006e6 <vprintfmt+0x1ba>
    800006da:	fe042783          	lw	a5,-32(s0)
    800006de:	40f007bb          	negw	a5,a5
    800006e2:	fef42023          	sw	a5,-32(s0)
                if (err > MAXERROR || (p = error_string[err]) == nullptr)
    800006e6:	fe042783          	lw	a5,-32(s0)
    800006ea:	0007871b          	sext.w	a4,a5
    800006ee:	4799                	li	a5,6
    800006f0:	02e7c063          	blt	a5,a4,80000710 <vprintfmt+0x1e4>
    800006f4:	00001717          	auipc	a4,0x1
    800006f8:	8c470713          	addi	a4,a4,-1852 # 80000fb8 <error_string>
    800006fc:	fe042783          	lw	a5,-32(s0)
    80000700:	078e                	slli	a5,a5,0x3
    80000702:	97ba                	add	a5,a5,a4
    80000704:	639c                	ld	a5,0(a5)
    80000706:	fef43423          	sd	a5,-24(s0)
    8000070a:	fe843783          	ld	a5,-24(s0)
    8000070e:	e38d                	bnez	a5,80000730 <vprintfmt+0x204>
                    printfmt(putch, putdat, "error %d", err);
    80000710:	fe042783          	lw	a5,-32(s0)
    80000714:	86be                	mv	a3,a5
    80000716:	00001617          	auipc	a2,0x1
    8000071a:	8f260613          	addi	a2,a2,-1806 # 80001008 <error_string+0x50>
    8000071e:	fb043583          	ld	a1,-80(s0)
    80000722:	fb843503          	ld	a0,-72(s0)
    80000726:	00000097          	auipc	ra,0x0
    8000072a:	dac080e7          	jalr	-596(ra) # 800004d2 <printfmt>
                else
                    printfmt(putch, putdat, "%s", p);
                break;
    8000072e:	a455                	j	800009d2 <vprintfmt+0x4a6>
                    printfmt(putch, putdat, "%s", p);
    80000730:	fe843683          	ld	a3,-24(s0)
    80000734:	00001617          	auipc	a2,0x1
    80000738:	8e460613          	addi	a2,a2,-1820 # 80001018 <error_string+0x60>
    8000073c:	fb043583          	ld	a1,-80(s0)
    80000740:	fb843503          	ld	a0,-72(s0)
    80000744:	00000097          	auipc	ra,0x0
    80000748:	d8e080e7          	jalr	-626(ra) # 800004d2 <printfmt>
                break;
    8000074c:	a459                	j	800009d2 <vprintfmt+0x4a6>
            case 's':
                if((p = va_arg(ap, char *)) == nullptr)
    8000074e:	fa043783          	ld	a5,-96(s0)
    80000752:	00878713          	addi	a4,a5,8
    80000756:	fae43023          	sd	a4,-96(s0)
    8000075a:	639c                	ld	a5,0(a5)
    8000075c:	fef43423          	sd	a5,-24(s0)
    80000760:	fe843783          	ld	a5,-24(s0)
    80000764:	e799                	bnez	a5,80000772 <vprintfmt+0x246>
                    p = "(nullptr)";
    80000766:	00001797          	auipc	a5,0x1
    8000076a:	8ba78793          	addi	a5,a5,-1862 # 80001020 <error_string+0x68>
    8000076e:	fef43423          	sd	a5,-24(s0)
                if(width > 0 && padc != '-'){
    80000772:	fd042783          	lw	a5,-48(s0)
    80000776:	2781                	sext.w	a5,a5
    80000778:	0af05963          	blez	a5,8000082a <vprintfmt+0x2fe>
    8000077c:	fc344783          	lbu	a5,-61(s0)
    80000780:	0ff7f713          	andi	a4,a5,255
    80000784:	02d00793          	li	a5,45
    80000788:	0af70163          	beq	a4,a5,8000082a <vprintfmt+0x2fe>
                    for(width -= strnlen(p, precision); width > 0; width--)
    8000078c:	fcc42783          	lw	a5,-52(s0)
    80000790:	85be                	mv	a1,a5
    80000792:	fe843503          	ld	a0,-24(s0)
    80000796:	00000097          	auipc	ra,0x0
    8000079a:	692080e7          	jalr	1682(ra) # 80000e28 <strnlen>
    8000079e:	87aa                	mv	a5,a0
    800007a0:	fd042703          	lw	a4,-48(s0)
    800007a4:	2781                	sext.w	a5,a5
    800007a6:	40f707bb          	subw	a5,a4,a5
    800007aa:	2781                	sext.w	a5,a5
    800007ac:	fcf42823          	sw	a5,-48(s0)
    800007b0:	a839                	j	800007ce <vprintfmt+0x2a2>
                        putch(padc, putdat);
    800007b2:	fc344783          	lbu	a5,-61(s0)
    800007b6:	2781                	sext.w	a5,a5
    800007b8:	fb843703          	ld	a4,-72(s0)
    800007bc:	fb043583          	ld	a1,-80(s0)
    800007c0:	853e                	mv	a0,a5
    800007c2:	9702                	jalr	a4
                    for(width -= strnlen(p, precision); width > 0; width--)
    800007c4:	fd042783          	lw	a5,-48(s0)
    800007c8:	37fd                	addiw	a5,a5,-1
    800007ca:	fcf42823          	sw	a5,-48(s0)
    800007ce:	fd042783          	lw	a5,-48(s0)
    800007d2:	2781                	sext.w	a5,a5
    800007d4:	fcf04fe3          	bgtz	a5,800007b2 <vprintfmt+0x286>
                }
                for(; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--){
    800007d8:	a889                	j	8000082a <vprintfmt+0x2fe>
                    if(altflag && (ch < ' ' || ch > '~'))
    800007da:	fc442783          	lw	a5,-60(s0)
    800007de:	2781                	sext.w	a5,a5
    800007e0:	cb85                	beqz	a5,80000810 <vprintfmt+0x2e4>
    800007e2:	fe442783          	lw	a5,-28(s0)
    800007e6:	0007871b          	sext.w	a4,a5
    800007ea:	47fd                	li	a5,31
    800007ec:	00e7da63          	bge	a5,a4,80000800 <vprintfmt+0x2d4>
    800007f0:	fe442783          	lw	a5,-28(s0)
    800007f4:	0007871b          	sext.w	a4,a5
    800007f8:	07e00793          	li	a5,126
    800007fc:	00e7da63          	bge	a5,a4,80000810 <vprintfmt+0x2e4>
                        putch('?', putdat);
    80000800:	fb843783          	ld	a5,-72(s0)
    80000804:	fb043583          	ld	a1,-80(s0)
    80000808:	03f00513          	li	a0,63
    8000080c:	9782                	jalr	a5
    8000080e:	a809                	j	80000820 <vprintfmt+0x2f4>
                    else
                        putch(ch, putdat);
    80000810:	fe442783          	lw	a5,-28(s0)
    80000814:	fb843703          	ld	a4,-72(s0)
    80000818:	fb043583          	ld	a1,-80(s0)
    8000081c:	853e                	mv	a0,a5
    8000081e:	9702                	jalr	a4
                for(; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--){
    80000820:	fd042783          	lw	a5,-48(s0)
    80000824:	37fd                	addiw	a5,a5,-1
    80000826:	fcf42823          	sw	a5,-48(s0)
    8000082a:	fe843783          	ld	a5,-24(s0)
    8000082e:	00178713          	addi	a4,a5,1
    80000832:	fee43423          	sd	a4,-24(s0)
    80000836:	0007c783          	lbu	a5,0(a5)
    8000083a:	fef42223          	sw	a5,-28(s0)
    8000083e:	fe442783          	lw	a5,-28(s0)
    80000842:	2781                	sext.w	a5,a5
    80000844:	cf8d                	beqz	a5,8000087e <vprintfmt+0x352>
    80000846:	fcc42783          	lw	a5,-52(s0)
    8000084a:	2781                	sext.w	a5,a5
    8000084c:	f807c7e3          	bltz	a5,800007da <vprintfmt+0x2ae>
    80000850:	fcc42783          	lw	a5,-52(s0)
    80000854:	37fd                	addiw	a5,a5,-1
    80000856:	fcf42623          	sw	a5,-52(s0)
    8000085a:	fcc42783          	lw	a5,-52(s0)
    8000085e:	2781                	sext.w	a5,a5
    80000860:	f607dde3          	bgez	a5,800007da <vprintfmt+0x2ae>
                }
                for(; width > 0; width--)
    80000864:	a829                	j	8000087e <vprintfmt+0x352>
                    putch(' ', putdat);
    80000866:	fb843783          	ld	a5,-72(s0)
    8000086a:	fb043583          	ld	a1,-80(s0)
    8000086e:	02000513          	li	a0,32
    80000872:	9782                	jalr	a5
                for(; width > 0; width--)
    80000874:	fd042783          	lw	a5,-48(s0)
    80000878:	37fd                	addiw	a5,a5,-1
    8000087a:	fcf42823          	sw	a5,-48(s0)
    8000087e:	fd042783          	lw	a5,-48(s0)
    80000882:	2781                	sext.w	a5,a5
    80000884:	fef041e3          	bgtz	a5,80000866 <vprintfmt+0x33a>
                break;
    80000888:	a2a9                	j	800009d2 <vprintfmt+0x4a6>
            case 'd':
                num = getint(&ap, lflag);
    8000088a:	fc842703          	lw	a4,-56(s0)
    8000088e:	fa040793          	addi	a5,s0,-96
    80000892:	85ba                	mv	a1,a4
    80000894:	853e                	mv	a0,a5
    80000896:	00000097          	auipc	ra,0x0
    8000089a:	bd4080e7          	jalr	-1068(ra) # 8000046a <getint>
    8000089e:	87aa                	mv	a5,a0
    800008a0:	fcf43c23          	sd	a5,-40(s0)
                if((int64_t)num < 0){
    800008a4:	fd843783          	ld	a5,-40(s0)
    800008a8:	0007df63          	bgez	a5,800008c6 <vprintfmt+0x39a>
                    putch('-', putdat);
    800008ac:	fb843783          	ld	a5,-72(s0)
    800008b0:	fb043583          	ld	a1,-80(s0)
    800008b4:	02d00513          	li	a0,45
    800008b8:	9782                	jalr	a5
                    num = -(int64_t)num;
    800008ba:	fd843783          	ld	a5,-40(s0)
    800008be:	40f007b3          	neg	a5,a5
    800008c2:	fcf43c23          	sd	a5,-40(s0)
                }
                base = 10;
    800008c6:	47a9                	li	a5,10
    800008c8:	fcf42a23          	sw	a5,-44(s0)
                goto number;
    800008cc:	a859                	j	80000962 <vprintfmt+0x436>
            case 'u':
                num = getuint(&ap, lflag);
    800008ce:	fc842703          	lw	a4,-56(s0)
    800008d2:	fa040793          	addi	a5,s0,-96
    800008d6:	85ba                	mv	a1,a4
    800008d8:	853e                	mv	a0,a5
    800008da:	00000097          	auipc	ra,0x0
    800008de:	b24080e7          	jalr	-1244(ra) # 800003fe <getuint>
    800008e2:	fca43c23          	sd	a0,-40(s0)
                base = 10;
    800008e6:	47a9                	li	a5,10
    800008e8:	fcf42a23          	sw	a5,-44(s0)
                goto number;
    800008ec:	a89d                	j	80000962 <vprintfmt+0x436>

            // (unsigned) octal
            case 'o':
                num = getuint(&ap, lflag);
    800008ee:	fc842703          	lw	a4,-56(s0)
    800008f2:	fa040793          	addi	a5,s0,-96
    800008f6:	85ba                	mv	a1,a4
    800008f8:	853e                	mv	a0,a5
    800008fa:	00000097          	auipc	ra,0x0
    800008fe:	b04080e7          	jalr	-1276(ra) # 800003fe <getuint>
    80000902:	fca43c23          	sd	a0,-40(s0)
                base = 8;
    80000906:	47a1                	li	a5,8
    80000908:	fcf42a23          	sw	a5,-44(s0)
                goto number;
    8000090c:	a899                	j	80000962 <vprintfmt+0x436>
            case 'p':
                putch('0', putdat);
    8000090e:	fb843783          	ld	a5,-72(s0)
    80000912:	fb043583          	ld	a1,-80(s0)
    80000916:	03000513          	li	a0,48
    8000091a:	9782                	jalr	a5
                putch('x', putdat);
    8000091c:	fb843783          	ld	a5,-72(s0)
    80000920:	fb043583          	ld	a1,-80(s0)
    80000924:	07800513          	li	a0,120
    80000928:	9782                	jalr	a5
                num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
    8000092a:	fa043783          	ld	a5,-96(s0)
    8000092e:	00878713          	addi	a4,a5,8
    80000932:	fae43023          	sd	a4,-96(s0)
    80000936:	639c                	ld	a5,0(a5)
    80000938:	fcf43c23          	sd	a5,-40(s0)
                base = 16;
    8000093c:	47c1                	li	a5,16
    8000093e:	fcf42a23          	sw	a5,-44(s0)
                goto number;
    80000942:	a005                	j	80000962 <vprintfmt+0x436>

            // (unsigned) hexadecimal
            case 'x': num = getuint(&ap, lflag); base = 16;
    80000944:	fc842703          	lw	a4,-56(s0)
    80000948:	fa040793          	addi	a5,s0,-96
    8000094c:	85ba                	mv	a1,a4
    8000094e:	853e                	mv	a0,a5
    80000950:	00000097          	auipc	ra,0x0
    80000954:	aae080e7          	jalr	-1362(ra) # 800003fe <getuint>
    80000958:	fca43c23          	sd	a0,-40(s0)
    8000095c:	47c1                	li	a5,16
    8000095e:	fcf42a23          	sw	a5,-44(s0)
            number:
                printnum(putch, putdat, num, base, width, padc);
    80000962:	fd442683          	lw	a3,-44(s0)
    80000966:	fc344783          	lbu	a5,-61(s0)
    8000096a:	2781                	sext.w	a5,a5
    8000096c:	fd042703          	lw	a4,-48(s0)
    80000970:	fd843603          	ld	a2,-40(s0)
    80000974:	fb043583          	ld	a1,-80(s0)
    80000978:	fb843503          	ld	a0,-72(s0)
    8000097c:	00000097          	auipc	ra,0x0
    80000980:	9b4080e7          	jalr	-1612(ra) # 80000330 <printnum>
                break;
    80000984:	a0b9                	j	800009d2 <vprintfmt+0x4a6>
            case '%': putch(ch, putdat); break;
    80000986:	fe442783          	lw	a5,-28(s0)
    8000098a:	fb843703          	ld	a4,-72(s0)
    8000098e:	fb043583          	ld	a1,-80(s0)
    80000992:	853e                	mv	a0,a5
    80000994:	9702                	jalr	a4
    80000996:	a835                	j	800009d2 <vprintfmt+0x4a6>
            default:
                putch('%', putdat);
    80000998:	fb843783          	ld	a5,-72(s0)
    8000099c:	fb043583          	ld	a1,-80(s0)
    800009a0:	02500513          	li	a0,37
    800009a4:	9782                	jalr	a5
                for (fmt--; fmt[-1] != '%'; fmt--) /* do nothing */;
    800009a6:	fa843783          	ld	a5,-88(s0)
    800009aa:	17fd                	addi	a5,a5,-1
    800009ac:	faf43423          	sd	a5,-88(s0)
    800009b0:	a031                	j	800009bc <vprintfmt+0x490>
    800009b2:	fa843783          	ld	a5,-88(s0)
    800009b6:	17fd                	addi	a5,a5,-1
    800009b8:	faf43423          	sd	a5,-88(s0)
    800009bc:	fa843783          	ld	a5,-88(s0)
    800009c0:	17fd                	addi	a5,a5,-1
    800009c2:	0007c783          	lbu	a5,0(a5)
    800009c6:	873e                	mv	a4,a5
    800009c8:	02500793          	li	a5,37
    800009cc:	fef713e3          	bne	a4,a5,800009b2 <vprintfmt+0x486>
                break;
    800009d0:	0001                	nop
    while (1) {
    800009d2:	be8d                	j	80000544 <vprintfmt+0x18>
            if (ch == '\0') return;
    800009d4:	0001                	nop
        }
    }
}
    800009d6:	60e6                	ld	ra,88(sp)
    800009d8:	6446                	ld	s0,80(sp)
    800009da:	6125                	addi	sp,sp,96
    800009dc:	8082                	ret

00000000800009de <sprintputch>:

static void sprintputch(int ch, Sprintbuf *b){
    800009de:	1101                	addi	sp,sp,-32
    800009e0:	ec22                	sd	s0,24(sp)
    800009e2:	1000                	addi	s0,sp,32
    800009e4:	87aa                	mv	a5,a0
    800009e6:	feb43023          	sd	a1,-32(s0)
    800009ea:	fef42623          	sw	a5,-20(s0)
    b->cnt++;
    800009ee:	fe043783          	ld	a5,-32(s0)
    800009f2:	4b9c                	lw	a5,16(a5)
    800009f4:	2785                	addiw	a5,a5,1
    800009f6:	0007871b          	sext.w	a4,a5
    800009fa:	fe043783          	ld	a5,-32(s0)
    800009fe:	cb98                	sw	a4,16(a5)
    if(b->buf < b->ebuf)
    80000a00:	fe043783          	ld	a5,-32(s0)
    80000a04:	6398                	ld	a4,0(a5)
    80000a06:	fe043783          	ld	a5,-32(s0)
    80000a0a:	679c                	ld	a5,8(a5)
    80000a0c:	02f77063          	bgeu	a4,a5,80000a2c <sprintputch+0x4e>
        *b->buf++ = ch;
    80000a10:	fe043783          	ld	a5,-32(s0)
    80000a14:	639c                	ld	a5,0(a5)
    80000a16:	00178693          	addi	a3,a5,1
    80000a1a:	fe043703          	ld	a4,-32(s0)
    80000a1e:	e314                	sd	a3,0(a4)
    80000a20:	fec42703          	lw	a4,-20(s0)
    80000a24:	0ff77713          	andi	a4,a4,255
    80000a28:	00e78023          	sb	a4,0(a5)
}
    80000a2c:	0001                	nop
    80000a2e:	6462                	ld	s0,24(sp)
    80000a30:	6105                	addi	sp,sp,32
    80000a32:	8082                	ret

0000000080000a34 <snprintf>:

int snprintf(char *str, size_t size, const char *fmt, ...){
    80000a34:	7159                	addi	sp,sp,-112
    80000a36:	fc06                	sd	ra,56(sp)
    80000a38:	f822                	sd	s0,48(sp)
    80000a3a:	0080                	addi	s0,sp,64
    80000a3c:	fca43c23          	sd	a0,-40(s0)
    80000a40:	fcb43823          	sd	a1,-48(s0)
    80000a44:	fcc43423          	sd	a2,-56(s0)
    80000a48:	e414                	sd	a3,8(s0)
    80000a4a:	e818                	sd	a4,16(s0)
    80000a4c:	ec1c                	sd	a5,24(s0)
    80000a4e:	03043023          	sd	a6,32(s0)
    80000a52:	03143423          	sd	a7,40(s0)
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    80000a56:	03040793          	addi	a5,s0,48
    80000a5a:	fcf43023          	sd	a5,-64(s0)
    80000a5e:	fc043783          	ld	a5,-64(s0)
    80000a62:	fd878793          	addi	a5,a5,-40
    80000a66:	fef43023          	sd	a5,-32(s0)
    cnt = vsnprintf(str, size, fmt, ap);
    80000a6a:	fe043783          	ld	a5,-32(s0)
    80000a6e:	86be                	mv	a3,a5
    80000a70:	fc843603          	ld	a2,-56(s0)
    80000a74:	fd043583          	ld	a1,-48(s0)
    80000a78:	fd843503          	ld	a0,-40(s0)
    80000a7c:	00000097          	auipc	ra,0x0
    80000a80:	01c080e7          	jalr	28(ra) # 80000a98 <vsnprintf>
    80000a84:	87aa                	mv	a5,a0
    80000a86:	fef42623          	sw	a5,-20(s0)
    va_end(ap);
    return cnt;
    80000a8a:	fec42783          	lw	a5,-20(s0)
}
    80000a8e:	853e                	mv	a0,a5
    80000a90:	70e2                	ld	ra,56(sp)
    80000a92:	7442                	ld	s0,48(sp)
    80000a94:	6165                	addi	sp,sp,112
    80000a96:	8082                	ret

0000000080000a98 <vsnprintf>:

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap){
    80000a98:	715d                	addi	sp,sp,-80
    80000a9a:	e486                	sd	ra,72(sp)
    80000a9c:	e0a2                	sd	s0,64(sp)
    80000a9e:	0880                	addi	s0,sp,80
    80000aa0:	fca43423          	sd	a0,-56(s0)
    80000aa4:	fcb43023          	sd	a1,-64(s0)
    80000aa8:	fac43c23          	sd	a2,-72(s0)
    80000aac:	fad43823          	sd	a3,-80(s0)
    Sprintbuf b = {str, str + size - 1, 0};
    80000ab0:	fc843783          	ld	a5,-56(s0)
    80000ab4:	fcf43c23          	sd	a5,-40(s0)
    80000ab8:	fc043783          	ld	a5,-64(s0)
    80000abc:	17fd                	addi	a5,a5,-1
    80000abe:	fc843703          	ld	a4,-56(s0)
    80000ac2:	97ba                	add	a5,a5,a4
    80000ac4:	fef43023          	sd	a5,-32(s0)
    80000ac8:	fe042423          	sw	zero,-24(s0)
    if(str == nullptr || b.buf > b.ebuf)
    80000acc:	fc843783          	ld	a5,-56(s0)
    80000ad0:	c799                	beqz	a5,80000ade <vsnprintf+0x46>
    80000ad2:	fd843703          	ld	a4,-40(s0)
    80000ad6:	fe043783          	ld	a5,-32(s0)
    80000ada:	00e7f463          	bgeu	a5,a4,80000ae2 <vsnprintf+0x4a>
        return -E_INVAL;
    80000ade:	57f5                	li	a5,-3
    80000ae0:	a035                	j	80000b0c <vsnprintf+0x74>
    vprintfmt((void (*)(int, void *))sprintputch, &b, fmt, ap);
    80000ae2:	fd840793          	addi	a5,s0,-40
    80000ae6:	fb043683          	ld	a3,-80(s0)
    80000aea:	fb843603          	ld	a2,-72(s0)
    80000aee:	85be                	mv	a1,a5
    80000af0:	00000517          	auipc	a0,0x0
    80000af4:	eee50513          	addi	a0,a0,-274 # 800009de <sprintputch>
    80000af8:	00000097          	auipc	ra,0x0
    80000afc:	a34080e7          	jalr	-1484(ra) # 8000052c <vprintfmt>
    *b.buf = '\0';
    80000b00:	fd843783          	ld	a5,-40(s0)
    80000b04:	00078023          	sb	zero,0(a5)
    return b.cnt;
    80000b08:	fe842783          	lw	a5,-24(s0)
    80000b0c:	853e                	mv	a0,a5
    80000b0e:	60a6                	ld	ra,72(sp)
    80000b10:	6406                	ld	s0,64(sp)
    80000b12:	6161                	addi	sp,sp,80
    80000b14:	8082                	ret

0000000080000b16 <memset>:
#include "string.h"

void *memset(void *dst, int c, size_t n) {
    80000b16:	7139                	addi	sp,sp,-64
    80000b18:	fc22                	sd	s0,56(sp)
    80000b1a:	0080                	addi	s0,sp,64
    80000b1c:	fca43c23          	sd	a0,-40(s0)
    80000b20:	87ae                	mv	a5,a1
    80000b22:	fcc43423          	sd	a2,-56(s0)
    80000b26:	fcf42a23          	sw	a5,-44(s0)
    char *cdst = (char *)dst;
    80000b2a:	fd843783          	ld	a5,-40(s0)
    80000b2e:	fef43023          	sd	a5,-32(s0)
    for (size_t i = 0; i < n; i++) cdst[i] = c;
    80000b32:	fe043423          	sd	zero,-24(s0)
    80000b36:	a00d                	j	80000b58 <memset+0x42>
    80000b38:	fe043703          	ld	a4,-32(s0)
    80000b3c:	fe843783          	ld	a5,-24(s0)
    80000b40:	97ba                	add	a5,a5,a4
    80000b42:	fd442703          	lw	a4,-44(s0)
    80000b46:	0ff77713          	andi	a4,a4,255
    80000b4a:	00e78023          	sb	a4,0(a5)
    80000b4e:	fe843783          	ld	a5,-24(s0)
    80000b52:	0785                	addi	a5,a5,1
    80000b54:	fef43423          	sd	a5,-24(s0)
    80000b58:	fe843703          	ld	a4,-24(s0)
    80000b5c:	fc843783          	ld	a5,-56(s0)
    80000b60:	fcf76ce3          	bltu	a4,a5,80000b38 <memset+0x22>
    return dst;
    80000b64:	fd843783          	ld	a5,-40(s0)
}
    80000b68:	853e                	mv	a0,a5
    80000b6a:	7462                	ld	s0,56(sp)
    80000b6c:	6121                	addi	sp,sp,64
    80000b6e:	8082                	ret

0000000080000b70 <memcmp>:

int memcmp(const void *v1, const void *v2, size_t n) {
    80000b70:	7139                	addi	sp,sp,-64
    80000b72:	fc22                	sd	s0,56(sp)
    80000b74:	0080                	addi	s0,sp,64
    80000b76:	fca43c23          	sd	a0,-40(s0)
    80000b7a:	fcb43823          	sd	a1,-48(s0)
    80000b7e:	fcc43423          	sd	a2,-56(s0)
    const uchar *s1, *s2;
    s1 = v1;
    80000b82:	fd843783          	ld	a5,-40(s0)
    80000b86:	fef43423          	sd	a5,-24(s0)
    s2 = v2;
    80000b8a:	fd043783          	ld	a5,-48(s0)
    80000b8e:	fef43023          	sd	a5,-32(s0)
    while (n-- > 0) {
    80000b92:	a0a1                	j	80000bda <memcmp+0x6a>
        if (*s1 != *s2) return *s1 - *s2;
    80000b94:	fe843783          	ld	a5,-24(s0)
    80000b98:	0007c703          	lbu	a4,0(a5)
    80000b9c:	fe043783          	ld	a5,-32(s0)
    80000ba0:	0007c783          	lbu	a5,0(a5)
    80000ba4:	02f70163          	beq	a4,a5,80000bc6 <memcmp+0x56>
    80000ba8:	fe843783          	ld	a5,-24(s0)
    80000bac:	0007c783          	lbu	a5,0(a5)
    80000bb0:	0007871b          	sext.w	a4,a5
    80000bb4:	fe043783          	ld	a5,-32(s0)
    80000bb8:	0007c783          	lbu	a5,0(a5)
    80000bbc:	2781                	sext.w	a5,a5
    80000bbe:	40f707bb          	subw	a5,a4,a5
    80000bc2:	2781                	sext.w	a5,a5
    80000bc4:	a01d                	j	80000bea <memcmp+0x7a>
        s1++;
    80000bc6:	fe843783          	ld	a5,-24(s0)
    80000bca:	0785                	addi	a5,a5,1
    80000bcc:	fef43423          	sd	a5,-24(s0)
        s2++;
    80000bd0:	fe043783          	ld	a5,-32(s0)
    80000bd4:	0785                	addi	a5,a5,1
    80000bd6:	fef43023          	sd	a5,-32(s0)
    while (n-- > 0) {
    80000bda:	fc843783          	ld	a5,-56(s0)
    80000bde:	fff78713          	addi	a4,a5,-1
    80000be2:	fce43423          	sd	a4,-56(s0)
    80000be6:	f7dd                	bnez	a5,80000b94 <memcmp+0x24>
    }
    return 0;
    80000be8:	4781                	li	a5,0
}
    80000bea:	853e                	mv	a0,a5
    80000bec:	7462                	ld	s0,56(sp)
    80000bee:	6121                	addi	sp,sp,64
    80000bf0:	8082                	ret

0000000080000bf2 <memmove>:

void *memmove(void *dst, const void *src, size_t n) {
    80000bf2:	7139                	addi	sp,sp,-64
    80000bf4:	fc22                	sd	s0,56(sp)
    80000bf6:	0080                	addi	s0,sp,64
    80000bf8:	fca43c23          	sd	a0,-40(s0)
    80000bfc:	fcb43823          	sd	a1,-48(s0)
    80000c00:	fcc43423          	sd	a2,-56(s0)
    const char *s;
    char *d;
    s = src;
    80000c04:	fd043783          	ld	a5,-48(s0)
    80000c08:	fef43423          	sd	a5,-24(s0)
    d = dst;
    80000c0c:	fd843783          	ld	a5,-40(s0)
    80000c10:	fef43023          	sd	a5,-32(s0)
    if (s < d && s + n > d) {
    80000c14:	fe843703          	ld	a4,-24(s0)
    80000c18:	fe043783          	ld	a5,-32(s0)
    80000c1c:	08f77463          	bgeu	a4,a5,80000ca4 <memmove+0xb2>
    80000c20:	fe843703          	ld	a4,-24(s0)
    80000c24:	fc843783          	ld	a5,-56(s0)
    80000c28:	97ba                	add	a5,a5,a4
    80000c2a:	fe043703          	ld	a4,-32(s0)
    80000c2e:	06f77b63          	bgeu	a4,a5,80000ca4 <memmove+0xb2>
        s += n;
    80000c32:	fe843703          	ld	a4,-24(s0)
    80000c36:	fc843783          	ld	a5,-56(s0)
    80000c3a:	97ba                	add	a5,a5,a4
    80000c3c:	fef43423          	sd	a5,-24(s0)
        d += n;
    80000c40:	fe043703          	ld	a4,-32(s0)
    80000c44:	fc843783          	ld	a5,-56(s0)
    80000c48:	97ba                	add	a5,a5,a4
    80000c4a:	fef43023          	sd	a5,-32(s0)
        while (n-- > 0) *--d = *--s;
    80000c4e:	a01d                	j	80000c74 <memmove+0x82>
    80000c50:	fe843783          	ld	a5,-24(s0)
    80000c54:	17fd                	addi	a5,a5,-1
    80000c56:	fef43423          	sd	a5,-24(s0)
    80000c5a:	fe043783          	ld	a5,-32(s0)
    80000c5e:	17fd                	addi	a5,a5,-1
    80000c60:	fef43023          	sd	a5,-32(s0)
    80000c64:	fe843783          	ld	a5,-24(s0)
    80000c68:	0007c703          	lbu	a4,0(a5)
    80000c6c:	fe043783          	ld	a5,-32(s0)
    80000c70:	00e78023          	sb	a4,0(a5)
    80000c74:	fc843783          	ld	a5,-56(s0)
    80000c78:	fff78713          	addi	a4,a5,-1
    80000c7c:	fce43423          	sd	a4,-56(s0)
    80000c80:	fbe1                	bnez	a5,80000c50 <memmove+0x5e>
    if (s < d && s + n > d) {
    80000c82:	a805                	j	80000cb2 <memmove+0xc0>
    } else {
        while (n-- > 0) *d++ = *s++;
    80000c84:	fe843703          	ld	a4,-24(s0)
    80000c88:	00170793          	addi	a5,a4,1
    80000c8c:	fef43423          	sd	a5,-24(s0)
    80000c90:	fe043783          	ld	a5,-32(s0)
    80000c94:	00178693          	addi	a3,a5,1
    80000c98:	fed43023          	sd	a3,-32(s0)
    80000c9c:	00074703          	lbu	a4,0(a4)
    80000ca0:	00e78023          	sb	a4,0(a5)
    80000ca4:	fc843783          	ld	a5,-56(s0)
    80000ca8:	fff78713          	addi	a4,a5,-1
    80000cac:	fce43423          	sd	a4,-56(s0)
    80000cb0:	fbf1                	bnez	a5,80000c84 <memmove+0x92>
    }
    return dst;
    80000cb2:	fd843783          	ld	a5,-40(s0)
}
    80000cb6:	853e                	mv	a0,a5
    80000cb8:	7462                	ld	s0,56(sp)
    80000cba:	6121                	addi	sp,sp,64
    80000cbc:	8082                	ret

0000000080000cbe <memcpy>:

void *memcpy(void *dst, const void *src, size_t n) { return memmove(dst, src, n); }
    80000cbe:	7179                	addi	sp,sp,-48
    80000cc0:	f406                	sd	ra,40(sp)
    80000cc2:	f022                	sd	s0,32(sp)
    80000cc4:	1800                	addi	s0,sp,48
    80000cc6:	fea43423          	sd	a0,-24(s0)
    80000cca:	feb43023          	sd	a1,-32(s0)
    80000cce:	fcc43c23          	sd	a2,-40(s0)
    80000cd2:	fd843603          	ld	a2,-40(s0)
    80000cd6:	fe043583          	ld	a1,-32(s0)
    80000cda:	fe843503          	ld	a0,-24(s0)
    80000cde:	00000097          	auipc	ra,0x0
    80000ce2:	f14080e7          	jalr	-236(ra) # 80000bf2 <memmove>
    80000ce6:	87aa                	mv	a5,a0
    80000ce8:	853e                	mv	a0,a5
    80000cea:	70a2                	ld	ra,40(sp)
    80000cec:	7402                	ld	s0,32(sp)
    80000cee:	6145                	addi	sp,sp,48
    80000cf0:	8082                	ret

0000000080000cf2 <strncmp>:

int strncmp(const char *p, const char *q, size_t n) {
    80000cf2:	7179                	addi	sp,sp,-48
    80000cf4:	f422                	sd	s0,40(sp)
    80000cf6:	1800                	addi	s0,sp,48
    80000cf8:	fea43423          	sd	a0,-24(s0)
    80000cfc:	feb43023          	sd	a1,-32(s0)
    80000d00:	fcc43c23          	sd	a2,-40(s0)
    while (n > 0 && *p && *p == *q) {
    80000d04:	a005                	j	80000d24 <strncmp+0x32>
        n--;
    80000d06:	fd843783          	ld	a5,-40(s0)
    80000d0a:	17fd                	addi	a5,a5,-1
    80000d0c:	fcf43c23          	sd	a5,-40(s0)
        p++;
    80000d10:	fe843783          	ld	a5,-24(s0)
    80000d14:	0785                	addi	a5,a5,1
    80000d16:	fef43423          	sd	a5,-24(s0)
        q++;
    80000d1a:	fe043783          	ld	a5,-32(s0)
    80000d1e:	0785                	addi	a5,a5,1
    80000d20:	fef43023          	sd	a5,-32(s0)
    while (n > 0 && *p && *p == *q) {
    80000d24:	fd843783          	ld	a5,-40(s0)
    80000d28:	c385                	beqz	a5,80000d48 <strncmp+0x56>
    80000d2a:	fe843783          	ld	a5,-24(s0)
    80000d2e:	0007c783          	lbu	a5,0(a5)
    80000d32:	cb99                	beqz	a5,80000d48 <strncmp+0x56>
    80000d34:	fe843783          	ld	a5,-24(s0)
    80000d38:	0007c703          	lbu	a4,0(a5)
    80000d3c:	fe043783          	ld	a5,-32(s0)
    80000d40:	0007c783          	lbu	a5,0(a5)
    80000d44:	fcf701e3          	beq	a4,a5,80000d06 <strncmp+0x14>
    }
    if (n == 0) return 0;
    80000d48:	fd843783          	ld	a5,-40(s0)
    80000d4c:	e399                	bnez	a5,80000d52 <strncmp+0x60>
    80000d4e:	4781                	li	a5,0
    80000d50:	a839                	j	80000d6e <strncmp+0x7c>
    return (uchar)*p - (uchar)*q;
    80000d52:	fe843783          	ld	a5,-24(s0)
    80000d56:	0007c783          	lbu	a5,0(a5)
    80000d5a:	0007871b          	sext.w	a4,a5
    80000d5e:	fe043783          	ld	a5,-32(s0)
    80000d62:	0007c783          	lbu	a5,0(a5)
    80000d66:	2781                	sext.w	a5,a5
    80000d68:	40f707bb          	subw	a5,a4,a5
    80000d6c:	2781                	sext.w	a5,a5
}
    80000d6e:	853e                	mv	a0,a5
    80000d70:	7422                	ld	s0,40(sp)
    80000d72:	6145                	addi	sp,sp,48
    80000d74:	8082                	ret

0000000080000d76 <strncpy>:

char *strncpy(char *s, const char *t, size_t n) {
    80000d76:	7139                	addi	sp,sp,-64
    80000d78:	fc22                	sd	s0,56(sp)
    80000d7a:	0080                	addi	s0,sp,64
    80000d7c:	fca43c23          	sd	a0,-40(s0)
    80000d80:	fcb43823          	sd	a1,-48(s0)
    80000d84:	fcc43423          	sd	a2,-56(s0)
    char *os;
    os = s;
    80000d88:	fd843783          	ld	a5,-40(s0)
    80000d8c:	fef43423          	sd	a5,-24(s0)
    while (n-- > 0 && (*s++ = *t++) != 0);
    80000d90:	0001                	nop
    80000d92:	fc843783          	ld	a5,-56(s0)
    80000d96:	fff78713          	addi	a4,a5,-1
    80000d9a:	fce43423          	sd	a4,-56(s0)
    80000d9e:	cf8d                	beqz	a5,80000dd8 <strncpy+0x62>
    80000da0:	fd043703          	ld	a4,-48(s0)
    80000da4:	00170793          	addi	a5,a4,1
    80000da8:	fcf43823          	sd	a5,-48(s0)
    80000dac:	fd843783          	ld	a5,-40(s0)
    80000db0:	00178693          	addi	a3,a5,1
    80000db4:	fcd43c23          	sd	a3,-40(s0)
    80000db8:	00074703          	lbu	a4,0(a4)
    80000dbc:	00e78023          	sb	a4,0(a5)
    80000dc0:	0007c783          	lbu	a5,0(a5)
    80000dc4:	f7f9                	bnez	a5,80000d92 <strncpy+0x1c>
    while (n-- > 0) *s++ = 0;
    80000dc6:	a809                	j	80000dd8 <strncpy+0x62>
    80000dc8:	fd843783          	ld	a5,-40(s0)
    80000dcc:	00178713          	addi	a4,a5,1
    80000dd0:	fce43c23          	sd	a4,-40(s0)
    80000dd4:	00078023          	sb	zero,0(a5)
    80000dd8:	fc843783          	ld	a5,-56(s0)
    80000ddc:	fff78713          	addi	a4,a5,-1
    80000de0:	fce43423          	sd	a4,-56(s0)
    80000de4:	f3f5                	bnez	a5,80000dc8 <strncpy+0x52>
    return os;
    80000de6:	fe843783          	ld	a5,-24(s0)
}
    80000dea:	853e                	mv	a0,a5
    80000dec:	7462                	ld	s0,56(sp)
    80000dee:	6121                	addi	sp,sp,64
    80000df0:	8082                	ret

0000000080000df2 <strlen>:

size_t strlen(const char *s) {
    80000df2:	7179                	addi	sp,sp,-48
    80000df4:	f422                	sd	s0,40(sp)
    80000df6:	1800                	addi	s0,sp,48
    80000df8:	fca43c23          	sd	a0,-40(s0)
    size_t n;
    for (n = 0; s[n]; n++);
    80000dfc:	fe043423          	sd	zero,-24(s0)
    80000e00:	a031                	j	80000e0c <strlen+0x1a>
    80000e02:	fe843783          	ld	a5,-24(s0)
    80000e06:	0785                	addi	a5,a5,1
    80000e08:	fef43423          	sd	a5,-24(s0)
    80000e0c:	fd843703          	ld	a4,-40(s0)
    80000e10:	fe843783          	ld	a5,-24(s0)
    80000e14:	97ba                	add	a5,a5,a4
    80000e16:	0007c783          	lbu	a5,0(a5)
    80000e1a:	f7e5                	bnez	a5,80000e02 <strlen+0x10>
    return n;
    80000e1c:	fe843783          	ld	a5,-24(s0)
}
    80000e20:	853e                	mv	a0,a5
    80000e22:	7422                	ld	s0,40(sp)
    80000e24:	6145                	addi	sp,sp,48
    80000e26:	8082                	ret

0000000080000e28 <strnlen>:

size_t strnlen(const char *s, size_t len) {
    80000e28:	7179                	addi	sp,sp,-48
    80000e2a:	f422                	sd	s0,40(sp)
    80000e2c:	1800                	addi	s0,sp,48
    80000e2e:	fca43c23          	sd	a0,-40(s0)
    80000e32:	fcb43823          	sd	a1,-48(s0)
    size_t cnt = 0;
    80000e36:	fe043423          	sd	zero,-24(s0)
    while (cnt < len && *s++ != '\0') cnt++;
    80000e3a:	a031                	j	80000e46 <strnlen+0x1e>
    80000e3c:	fe843783          	ld	a5,-24(s0)
    80000e40:	0785                	addi	a5,a5,1
    80000e42:	fef43423          	sd	a5,-24(s0)
    80000e46:	fe843703          	ld	a4,-24(s0)
    80000e4a:	fd043783          	ld	a5,-48(s0)
    80000e4e:	00f77b63          	bgeu	a4,a5,80000e64 <strnlen+0x3c>
    80000e52:	fd843783          	ld	a5,-40(s0)
    80000e56:	00178713          	addi	a4,a5,1
    80000e5a:	fce43c23          	sd	a4,-40(s0)
    80000e5e:	0007c783          	lbu	a5,0(a5)
    80000e62:	ffe9                	bnez	a5,80000e3c <strnlen+0x14>
    return cnt;
    80000e64:	fe843783          	ld	a5,-24(s0)
    80000e68:	853e                	mv	a0,a5
    80000e6a:	7422                	ld	s0,40(sp)
    80000e6c:	6145                	addi	sp,sp,48
    80000e6e:	8082                	ret
