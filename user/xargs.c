#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int runCommand(int argc, char *argv[], int argcParam, char* param,int paramLength){
    int totalArgc = argc+argcParam;
    char** execArgs = malloc(sizeof(char*)*totalArgc);
    int i = 0;
    //前面一半是xargs的参数
    for(;i<argc-1;i++){
        execArgs[i] = argv[i+1];
    }
    //后面一半是管道左边指令产生的参数
    int k=0;
    int isInWord = 0;
    for(int k=0;k<paramLength;k++){
        //不在单词中且不是空格就记录，且记录为在单词中
        if(param[k]!=' ' && !isInWord){
            execArgs[i] = param+k;
            isInWord = 1;
            i++;
        //是空格就转换为nul，且记录为不在单词中
        }else if(param[k]==' '){
            isInWord = 0;
            param[k] = 0;
        }
    }
    execArgs[totalArgc-1] = 0;
    //exec以后就不返回了，需要fork
    int childId = fork();
    if(!childId)
        exec(execArgs[0],execArgs);
    else
        //等待子进程退出
        wait(0);
}

int main(int argc, char *argv[]){
    char param[512];
    int i = 0;
    char ch;
    int isInWord = 0;
    int argcParam = 0;
    while (read(0, &ch, 1) > 0) {
        if(ch!='\n'){
            if(ch!=' ' && !isInWord){
                isInWord = 1;
                argcParam++;
            }else if(ch==' '){
                isInWord = 0;
            }
            param[i] = ch;
            i++;
        }else{
            param[i+1] = 0;
            runCommand(argc,argv,argcParam,param,i+1);
            isInWord = 0;
            argcParam = 0;
            i = 0;
        }
    }
    exit(0);
}