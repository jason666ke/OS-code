#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[]) {
    int parent[2], child[2];
    char buf[8];
    int parent_id, child_id;

    pipe(parent);
    pipe(child);

    // 父进程从parent[1]写，child[0]读
    // 子进程从parent[0]读，child[1]写
    if (fork() == 0) {
        
        child_id = getpid();    
        read(parent[0], buf, 4);
        printf("%d: received %s\n", child_id, buf);
        
        write(child[1], "pong", strlen("pong"));   // 向父进程写入pong
    }else {

        parent_id = getpid();
        
        write(parent[1], "ping", strlen("ping"));   // 向子进程写入ping

        read(child[0], buf, 4);
        printf("%d: received %s\n", parent_id, buf);
        
    }
    exit(0);
}