#include "kernel/types.h"
#include "user.h"

void prime(int input_fd);

int main(int argc, char* argv[]) {
    int parent_fd[2];
    pipe(parent_fd);

    // 子进程
    if (fork() == 0) {

        close(parent_fd[1]);    // 关闭管道写端
        prime(parent_fd[0]);    // 读取管道信息

    }else {
        close(parent_fd[0]);

        int i;
        for (i = 2; i < 36; i++) {
            write(parent_fd[1], &i, sizeof(int));
        }
        
        close(parent_fd[1]);
    }
    wait(0);    // 等待子进程结束
    exit(0);
}

void prime(int input_fd) {
    // 管道中的第一个数一定为质数
    int base;
    if (read(input_fd, &base, sizeof(int)) == 0) {
        exit(0);    // 如果是空的，则可以退出
    }
    printf("prime %d\n", base);

    int p[2];
    pipe(p);

    if (fork() == 0) {
        close(p[1]);
        prime(p[0]);
    }else {
        close(p[0]);

        int num;
        int eof;
        do
        {
            eof = read(input_fd, &num, sizeof(int));
            if (num % base != 0) {
                write(p[1], &num, sizeof(int));
            }
        } while (eof);
        
        close(p[1]);
    }
    wait(0);
    exit(0);
}