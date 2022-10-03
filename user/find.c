#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *dir, char *file) {
    // 文件名缓冲区和指针
    char buf[512], *p;
    // 文件描述符fd
    int fd;
    // 文件相关结构体
    struct dirent de;
    struct stat st;

    // 调用open()函数打开路径，返回对应的文件描述符
    // error则返回-1
    if ((fd = open(dir, 0)) < 0) {
        // 报错，对应路径无法打开
        fprintf(2, "find: cannot open %s\n", dir);
        return;
    }

    // fstat系统调用，与stat系统调用类似
    // 以文件描述符作为参数，将fd所指的文件状态复制到所指的结构中
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s \n", dir);
        // 关闭文件描述符fd ?? 不懂
        close(fd);
        return;
    }

    // 如果不是目录类型
    if (st.type != T_DIR) {
        // 报错，类型不是目录
        fprintf(2, "find: %s is not a directory\n", dir);
        // 关闭文件描述符
        close(fd);
        return;
    }

    // 如果路径过长无法放入缓冲区
    if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof buf) {
        fprintf(2, "find: directory too long\n");
        close(fd);
        return;
    }

    // 将dir所指向的字符串（绝对路径）复制到buf
    strcpy(buf, dir);
    // buf 是绝对路径，p是文件名，通过加上"/"这一前缀将p拼接在buf后
    p = buf + strlen(buf);
    *p++ = '/';
    // 读取fd， 如果read返回字节数与de长度相等，则循环 (为什么呢？？)
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) {
            continue;
        }

        // 不递归"." 和 ".."
        if (!strcmp(de.name, ".") || !strcmp(de.name, "..")) {
            continue;
        }

        // de.name代表文件名，将de.name信息复制到p上
        memmove(p, de.name, DIRSIZ);

        // 设置文件结束符
        p[DIRSIZ] = 0;

        // int stat(char *, struct stat *)
        // stat 系统调用，可获得一个已经存在的文件的模式，并将此模式赋值给他的副本
        // stat 以文件名作为参数
        if (stat(buf, &st) < 0) {
            fprintf(2, "find: cannot stat %s\n", buf);
            continue;
        }

        // 是目录类型，则递归查找
        if (st.type == T_DIR) {
            find(buf, file);
        }
        // 如果是文件类型且名字与要查找的文件名相同
        else if (st.type == T_FILE && !strcmp(de.name, file)) {
            // 打印缓冲区存放的路径
            printf("%s\n", buf);
        }
    }
}

int main (int argc, char *argv[]) {
    
    // 若参数个数不为3则报错
    if (argc != 3) {
        // 输出提示
        fprintf(2, "usage: find dirName fileName\n");
        exit(1);
    }

    // 调用find函数查找指定目录下的文件
    find(argv[1], argv[2]);
    exit(0);
}