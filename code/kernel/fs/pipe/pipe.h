#ifndef __FS_PIPE_PIPE_H__
#define __FS_PIPE_PIPE_H__

#include "types.h"
#include "list.h"
#include "sem.h"
#include "pipe_state.h"

typedef struct fs Fs;
typedef struct inode Inode;

typedef struct pipe_fs Pipe_fs;
struct pipe_fs{
    Inode *root;
    Sem pipe_sem;
    List_entry pipe_list;
};

void pipe_init(void);
void lock_pipe(Pipe_fs *pipe);
void unlock_pipe(struct pipe_fs *pipe);

typedef struct pipe_root Pipe_root;
struct pipe_root{

};

typedef struct pipe_inode Pipe_inode;
struct pipe_inode {
    enum{
        PIN_RDONLY, PIN_WRONLY,
    }pin_type;
    char *name;
    Pipe_state *state;
    List_entry pipe_link;
};

#define le2pin(le)  to_struct((le), Pipe_inode, pipe_link);

Inode *pipe_create_root(Fs *fs);
struct inode *pipe_create_inode(Fs *fs, const char *__name, Pipe_state *state, bool readonly);
int pipe_open(Inode **rnode_store, Inode **wnode_store);
#endif