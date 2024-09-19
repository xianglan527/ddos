#include "signal.h"

#include "assert.h"
#include "error.h"
#include "proc.h"
#include "slab.h"
#include "sysdef.h"

void signal_init(Signal *signal) {
    // atomic_set(&signal->count, 0);
    initlock(&signal->signal_lock, "signal_lock");
    for (int i = 0; i < NSIG; i++) {
        signal->action[i].sa_handler = SIG_DFL;
        signal->action[i].sa_mask = 0;
        signal->action[i].sa_flags = 0;
    }
}

void sigaction_copy(Signal *to, Signal *from) {
    acquire(&from->signal_lock);
    for (int i = 0; i < NSIG; i++) {
        to->action[i].sa_handler = from->action[i].sa_handler;
        to->action[i].sa_mask = from->action[i].sa_mask;
        to->action[i].sa_flags = from->action[i].sa_flags;
    }
    release(&from->signal_lock);
}

int ipc_set_sigaction(int sig, Sigaction *sa) {
    Proc *current = myproc();
    assert(current != nullptr);
    Mm_struct *mm = current->mm;
    Sigaction local_sigaction;
    acquire(&mm->mm_lock);
    { either_copy_user2kernel(&local_sigaction, 1, (uint64_t)sa, sizeof(*sa)); }
    release(&mm->mm_lock);
    acquire(&current->signal.signal_lock);
    current->signal.action[sig - 1].sa_handler = local_sigaction.sa_handler;
    current->signal.action[sig - 1].sa_flags = local_sigaction.sa_flags;
    current->signal.action[sig - 1].sa_mask = local_sigaction.sa_mask;
    release(&current->signal.signal_lock);
    return 0;
}

int ipc_send_signal(int pid, int sig) {
    int ret = -E_INVAL;
    Proc *proc = find_proc(pid);
    if (proc == nullptr) { return ret; }
    ret = -E_NO_MEM;
    Siginfo *siginfo = kmalloc(sizeof(Siginfo));
    if (siginfo == nullptr) { return ret; }
    siginfo->sig = sig;
    siginfo->pid = pid;
    acquire(&proc->lock);
    list_add_before(&proc->siginfo_list, &siginfo->siginfo_link);
    release(&proc->lock);
    return 0;
}

void do_signal() {
    Proc *current = myproc();
    assert(current != nullptr);
    Mm_struct *mm = current->mm;
    List_entry *le = list_next(&current->siginfo_list);
    assert(le != &current->siginfo_list);
    Siginfo *siginfo = le2siginfo(le, siginfo_link);
    int sig = siginfo->sig;
    list_del(le);
    kfree(le2siginfo(le, siginfo_link));
    assert(sig >= 0 && sig < NSIG);
    Sigaction *action = &current->signal.action[sig - 1];
    uint64_t flags = current->signal.action[sig - 1].sa_flags;
    sigset_t mask = current->signal.action[sig - 1].sa_mask;
    if (action->sa_handler == SIG_DFL) { return; }
    if (sig == SIGKILL) { return do_exit(E_KILLED); }
    if ((current->sig_blocked & (sig - 1)) == 0) {
        if (flags & SA_ONESHOT) {
            current->signal.action[sig - 1].sa_handler = SIG_DFL;
            current->signal.action[sig - 1].sa_mask = 0;
            current->signal.action[sig - 1].sa_flags = 0;
        }
        current->sig_blocked |= mask;
        if (!(flags & SA_NOMASK)) { current->sig_blocked |= (sig - 1); }
        Sigframe *sig_frame = (Sigframe *)((current->trapframe->sp - sizeof(Sigframe)) & ~0x7UL);
        // Construct 'li a7, SYS_sigreturn'
        //           'ecall'
        uint32_t addi_instruction;
        uint32_t ecall_instruction = 0x00000073;
        assert(SYS_sigreturn <= 0XFFF);
        uint32_t imm = SYS_sigreturn & 0xFFF;
        // li a7, imm  ==> addi a7, x0, imm
        // Construct 'addi a7, x0, imm' instruction
        // The encoding format of the addi instruction is I-Type:
        // [31:20]: Immediate value imm[11:0]
        // [19:15]: Source register rs1 (for li, it's x0, which is 00000)
        // [14:12]: Function code funct3 (for addi, it's 000)
        // [11:7]: Destination register rd (for a7, which is x17, binary 10001)
        // [6:0]: Opcode opcode (for addi, it's 0010011, i.e., 0x13)
        // machine_code = (imm & 0xFFF) << 20 | (x0 << 15) | (funct3 << 12) | (x17 << 7) | opcode
        addi_instruction = (imm << 20) | (0 << 15) | (0 << 12) | (17 << 7) | 0x13;
        acquire(&mm->mm_lock);
        copy_kernel2user(mm->pagetable, (uintptr_t)&sig_frame->saved_trapframe, (char *)current->trapframe,
                         sizeof(Trapframe));
        copy_kernel2user(mm->pagetable, (uintptr_t)sig_frame->ret_code, (char *)&addi_instruction,
                         sizeof(uint32_t));
        copy_kernel2user(mm->pagetable, (uintptr_t)(sig_frame->ret_code + 4), (char *)&ecall_instruction,
                         sizeof(uint32_t));
        copy_kernel2user(mm->pagetable, (uintptr_t)&sig_frame->sig, (char *)&sig, sizeof(sig));
        release(&mm->mm_lock);
        current->trapframe->ra = (uint64_t)sig_frame->ret_code;
        current->trapframe->epc = (uint64_t)action->sa_handler;
        current->trapframe->sp = (uint64_t)sig_frame;
    }
}

int ipc_sigreturn(void) {
    int sig;
    Proc *current = myproc();
    assert(current != nullptr);
    Mm_struct *mm = current->mm;
    Sigframe *sig_frame = (Sigframe *)current->trapframe->sp;
    acquire(&mm->mm_lock);
    either_copy_user2kernel(current->trapframe, 1, (uint64_t)&sig_frame->saved_trapframe, sizeof(Trapframe));
    either_copy_user2kernel(&sig, 1, (uint64_t)&sig_frame->sig, sizeof(sig));
    release(&mm->mm_lock);
    if (sig < 0 && sig >= NSIG) { return -E_INVAL; }
    current->sig_blocked &= ~(sig - 1);
    current->sig_blocked &= ~(current->signal.action[sig - 1].sa_mask);
    return 0;
}