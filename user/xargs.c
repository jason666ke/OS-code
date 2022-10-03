#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXN 1024

int main(int argc, char *argv[]) {
    
    // 参数小于2则需要报错
    if (argc < 2) {
        fprintf(2, "usage: xargs command\n");
        // 异常退出
        exit(1);
    }

    // 字符数组用以存放子进程参数
    char *argvs[MAXARG];
    // 数组索引
    int index = 0;
    
    // 读取 xargs 后的参数， 跳过xargs
    for (int i = 1; i < argc; ++i) {
        argvs[index++] = argv[i];
    }

    // 定义缓冲区存放从管道中读取的数据
    char buf[MAXN] = {"\0"};

    int n;
    // 循环从管道读取数据
    while ((n = read(0, buf, MAXN)) > 0) {
        // 临时缓冲区存放追加的参数
        char temp[MAXN] = {"\0"};
        // xargs 命令参数后继续追加参数
        argvs[index] = temp;
        // 内循环获取追加的参数，并创建子进程执行命令
        for (int i = 0; i < strlen(buf); ++i) {
            // 读取单个输入行
            // 当遇到换行符时，创建子进程进行执行
            if (buf[i] == '\n') {
                // 创建子进程
                if (fork() == 0) {
                    // argv[1] 为函数名，argvs为参数
                    exec(argv[1], argvs);
                }
                // 需要等待子线程自行完毕
                wait(0); 
            }
            else {
                // 否则，读取管道的输出作为输入
                temp[i] = buf[i];
            }
            
        }
    }
    exit(0);
}