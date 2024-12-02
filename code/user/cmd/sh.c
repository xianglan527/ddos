#include "assert.h"
#include "dir.h"
#include "malloc.h"
#include "panic.h"
#include "printf.h"
#include "string.h"
#include "sysdef.h"
#include "user.h"

#define EXEC 1   // 执行命令
#define REDIR 2  // 重定向命令
#define PIPE 3   // 管道命令
#define LIST 4   // 命令列表
#define BACK 5   // 后台命令

#define CMD_MAXARGS 10

typedef struct cmd {
    int type;
} Cmd;

typedef struct exec_cmd {
    int type;
    char *agrv[CMD_MAXARGS + 1];
    char *eargv[CMD_MAXARGS + 1];
} Exec_cmd;

typedef struct redir_cmd {
    int type;
    Cmd *cmd;
    char *file;
    char *efile;
    int mode;
    int fd;
} Redir_cmd;

typedef struct pipe_cmd {
    int type;
    Cmd *left;
    Cmd *right;
} Pipe_cmd;

typedef struct back_cmd {
    int type;
    Cmd *cmd;
} Back_cmd;

typedef struct list_cmd {
    int type;
    Cmd *left;
    Cmd *right;
} List_cmd;

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

long get_cmd(char *buf, size_t nbuf) {
    fprintf(1, "$ ");
    memset(buf, 0, nbuf);
    long ret = read(0, buf, nbuf);
    if (ret < 0) fprintf(2, "getcmd: read error %d", ret);
    return ret;
}

int safe_fork(void) {
    int pid;
    pid = fork();
    if (pid < 0) { panic("fork"); }
    return pid;
}

int peek(char **ps, char *es, char *toks) {
    char *s;
    s = *ps;
    while (s < es && strchr(whitespace, *s)) { s++; }
    *ps = s;
    return *s && strchr(toks, *s);
}

Cmd *exec_cmd(void) {
    Exec_cmd *cmd;
    cmd = malloc(sizeof(*cmd));
    assert(cmd != nullptr);
    cmd->type = EXEC;
    return (Cmd *)cmd;
}

int gettoken(char **ps, char *es, char **q, char **eq) {
    char *s;
    int ret;
    s = *ps;
    while (s < es && strchr(whitespace, *s)) s++;
    if (q) *q = s;
    ret = *s;
    switch (*s) {
        case 0: break;
        case '|':
        case '(':
        case ')':
        case ';':
        case '&':
        case '<': s++; break;
        case '>':
            s++;
            if (*s == '>') {
                ret = '+';
                s++;
            }
            break;
        default:
            ret = 'a';
            while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)) s++;
            break;
    }
    if (eq) *eq = s;
    while (s < es && strchr(whitespace, *s)) s++;
    *ps = s;
    return ret;
}

Cmd *redir_cmd(Cmd *subcmd, char *file, char *efile, int mode, int fd) {
    Redir_cmd *cmd;
    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->mode = mode;
    cmd->fd = fd;
    return (Cmd *)cmd;
}

Cmd *parse_redirs(Cmd *cmd, char **ps, char *es) {
    int tok;
    char *q, *eq;
    while (peek(ps, es, "<>")) {
        tok = gettoken(ps, es, 0, 0);
        if (gettoken(ps, es, &q, &eq) != 'a') { panic("missing file for redirection"); }
        switch (tok) {
            case '<': cmd = redir_cmd(cmd, q, eq, O_RDONLY, 0); break;
            case '>': cmd = redir_cmd(cmd, q, eq, O_WRONLY | O_CREAT | O_TRUNC, 1); break;
            case '+': cmd = redir_cmd(cmd, q, eq, O_WRONLY | O_CREAT | O_APPEND, 1); break;
        }
    }
    return cmd;
}

Cmd *parse_line(char **ps, char *es);

Cmd *parse_block(char **ps, char *es) {
    Cmd *cmd;
    if (!peek(ps, es, "(")) { panic("parse_block"); }
    gettoken(ps, es, 0, 0);
    cmd = parse_line(ps, es);
    if (!peek(ps, es, ")")) panic("syntax - missing )");
    gettoken(ps, es, 0, 0);
    cmd = parse_redirs(cmd, ps, es);
    return cmd;
}

Cmd *parse_exec(char **ps, char *es) {
    char *q, *eq;
    int tok, argc;
    Exec_cmd *cmd;
    Cmd *ret;
    if (peek(ps, es, "(")) { return parse_block(ps, es); }
    ret = exec_cmd();
    cmd = (Exec_cmd *)ret;
    argc = 0;
    ret = parse_redirs(ret, ps, es);
    while (!peek(ps, es, "|)&;")) {
        if ((tok = gettoken(ps, es, &q, &eq)) == 0) break;
        if (tok != 'a') panic("systax");
        cmd->agrv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;
        if (argc > CMD_MAXARGS) panic("too many args");
        ret = parse_redirs(ret, ps, es);
    }
    cmd->agrv[argc] = nullptr;
    cmd->eargv[argc] = nullptr;
    return ret;
}

Cmd *pipe_cmd(Cmd *left, Cmd *right) {
    Pipe_cmd *cmd;
    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    return (Cmd *)cmd;
}

Cmd *parse_pipe(char **ps, char *es) {
    Cmd *cmd;
    cmd = parse_exec(ps, es);
    if (peek(ps, es, "|")) {
        gettoken(ps, es, 0, 0);
        cmd = pipe_cmd(cmd, parse_pipe(ps, es));
    }
    return cmd;
}

Cmd *back_cmd(Cmd *subcmd) {
    Back_cmd *cmd;
    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;
    return (Cmd *)cmd;
}

Cmd *list_cmd(Cmd *left, Cmd *right) {
    List_cmd *cmd;
    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;
    return (Cmd *)cmd;
}

Cmd *parse_line(char **ps, char *es) {
    Cmd *cmd;
    cmd = parse_pipe(ps, es);
    while (peek(ps, es, "&")) {
        gettoken(ps, es, 0, 0);
        cmd = back_cmd(cmd);
    }
    if (peek(ps, es, ";")) {
        gettoken(ps, es, 0, 0);
        cmd = list_cmd(cmd, parse_line(ps, es));
    }
    return cmd;
}

Cmd *null_terminate(Cmd *cmd) {
    if (cmd == nullptr) return nullptr;
    Exec_cmd *ecmd;
    Redir_cmd *rcmd;
    Pipe_cmd *pcmd;
    Back_cmd *bcmd;
    List_cmd *lcmd;
    switch (cmd->type) {
        case EXEC:
            ecmd = (Exec_cmd *)cmd;
            for (int i = 0; ecmd->agrv[i]; i++) { *ecmd->eargv[i] = 0; }
            break;
        case REDIR:
            rcmd = (Redir_cmd *)cmd;
            null_terminate(rcmd->cmd);
            *rcmd->efile = 0;
            break;
        case PIPE:
            pcmd = (Pipe_cmd *)cmd;
            null_terminate(pcmd->left);
            null_terminate(pcmd->right);
            break;
        case BACK:
            bcmd = (Back_cmd *)cmd;
            null_terminate(bcmd->cmd);
            break;
        case LIST:
            lcmd = (List_cmd *)cmd;
            null_terminate(lcmd->left);
            null_terminate(lcmd->right);
            break;
        default: panic("wrong type");
    }
    return cmd;
}

Cmd *parse_cmd(char *s) {
    char *es;
    Cmd *cmd;
    es = s + strlen(s);
    cmd = parse_line(&s, es);
    peek(&s, es, "");
    if (s != es) { panic("systax leftovers : %s", s); }
    null_terminate(cmd);
    return cmd;
}

void runcmd(Cmd *cmd) {
    Exec_cmd *ecmd;
    Redir_cmd *rcmd;
    Pipe_cmd *pcmd;
    Back_cmd *bcmd;
    List_cmd *lcmd;
    int ret;
    int p[2], fd;
    if (cmd == nullptr) { exit(0); }
    switch (cmd->type) {
        case EXEC:
            ecmd = (Exec_cmd *)cmd;
            if (ecmd->agrv[0] == 0) { exit(0); }
            exec(ecmd->agrv[0], ecmd->agrv);
            fprintf(2, "init: exec %s failed\n", ecmd->agrv[0]);
            exit(1);
            break;
        case REDIR:
            rcmd = (Redir_cmd *)cmd;
            close(rcmd->fd);
            if ((ret = open(rcmd->file, rcmd->mode)) < 0) {
                fprintf(2, "open %s failed\n", rcmd->file);
                exit(ret);
            }
            runcmd(rcmd->cmd);
            break;
        case PIPE:
            pcmd = (Pipe_cmd *)cmd;
            if (mkpipe(p) < 0) panic("pipe");
            if (safe_fork() == 0) {
                if (p[1] != 1) {
                    close(1);
                    dup2(p[1], 1);
                    close(p[0]);
                    close(p[1]);
                    runcmd(pcmd->left);
                }
            }
            if (safe_fork() == 0) {
                if (p[0] != 0) {
                    close(0);
                    dup2(p[0], 0);
                    close(p[0]);
                    close(p[1]);
                    runcmd(pcmd->right);
                }
            }
            close(p[0]);
            close(p[1]);
            wait();
            wait();
            exit(0);
            break;
        case BACK:
            bcmd = (Back_cmd *)cmd;
            if (safe_fork() == 0) { runcmd(bcmd->cmd); }
            wait();
            exit(0);
            break;
        case LIST:
            lcmd = (List_cmd *)cmd;
            if (safe_fork() == 0) { runcmd(lcmd->left); }
            wait();
            runcmd(lcmd->right);
            break;
        default: panic("runcmd");
    }
    panic("should not get here");
}

int main(int argc, char *argv[]) {
    prework();
    // for (int i = 0; i < argc; i++) { printf("arg %d is %s\n", i, argv[i]); }
    printf("welcome shell\n");
    static char buf[512];
    int ret, xstatus;
    while ((ret = get_cmd(buf, sizeof(buf))) >= 0) {
        prework();
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
            buf[strlen(buf)] = 0;
            if (chdir(buf + 3) < 0) {
                printf("cannot cd %s\n", buf + 3);
                print_current_dir();
            }
            continue;
        }
        if (safe_fork() == 0) { runcmd(parse_cmd(buf)); }
        ret = waitpid(0, &xstatus);
        if (ret == 0) {
            if (xstatus != 0) { warn("sh: cmd exit status is %d", xstatus); }
        } else {
            panic("sh: cmd returned an %d error", ret);
        }
    }
    exit(ret);
    return 0;
}