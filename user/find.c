#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void pathConcat(char* a, char* b,char* result){
    strcpy(result,a);
    int lenA = strlen(a);
    result[lenA] = '/';
    strcpy(result+lenA+1,b);
}

void find(char* nowPath, char* target){
    int dirFd = open(nowPath,0);
    if(dirFd<0){
        fprintf(2,"Dir not found.\n");
        exit(1);
    }
    struct dirent de;
    struct stat st;
    while(read(dirFd,&de,sizeof(de)) == sizeof(de)){
        if(de.inum == 0)
            continue;
        char newPath[512];
        //拼接目录
        pathConcat(nowPath,de.name,newPath);

        if(strcmp(de.name, target)==0){
            printf("%s\n",newPath);
        }
        if(stat(newPath, &st) < 0){
            fprintf(2,"Can not get stat.\n");
            continue;
        }
        //递归搜索，过滤.和..
        if(st.type==T_DIR && strcmp(de.name, ".") && strcmp(de.name, "..")){
            find(newPath,target);
        }

    }
}

int main(int argc, char *argv[]){
    if(argc!=3){
        fprintf(2,"Nope.\n");
        exit(1);
    }
    find(argv[1],argv[2]);
    exit(0);
}

