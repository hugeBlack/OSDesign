#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int findPrime(int* primePipe){
    //从pipe头部读取一个，输出
    char first;
    read(primePipe[0],&first,1);
    if(!first) exit(0);
    printf("prime %d\n", first);

    //然后检查其他的是是否能被这个值整除，能的话丢弃，否则加入到另一个pipe中
    int primePipeNew[2];
    pipe(primePipeNew);

    char now;
    while(read(primePipe[0],&now,1)){
        if(now%first==0) continue;
        write(primePipeNew[1],&now,1);
    }

    close(primePipe[0]);
    close(primePipeNew[1]);

    //fork，交给子进程处理新的pipe，父进程等待子进程结束
    int childPid = fork();
    if(!childPid){
        findPrime(primePipeNew);
    }else{
        wait(&childPid);
        exit(0);
    }
}

int main(int argc, char *argv[]){
    int primePipe[2];
    int nowPipe = 1;
    pipe(primePipe);
    for(char i=2;i<36;i++){
        write(primePipe[1],&i,1);
    }
    close(primePipe[1]);
    findPrime(primePipe);
}