#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int parentToChild[2];
    int childToParent[2];
    char result[512];
    
    char* parentWords = "ping";
    char* childWords = "pong";
    pipe(parentToChild);
    pipe(childToParent);
    int childPid = fork();
    //parent返回pid，child返回0
    if(childPid){
        close(parentToChild[0]);
        write(parentToChild[1],parentWords,strlen(parentWords));
        close(childToParent[1]);
        read(childToParent[0],result,sizeof(result));
        printf("%d: received %s\n", getpid(), result);
        exit(0);
    } else {
        close(parentToChild[1]);
        read(parentToChild[0],result,sizeof(result));
        close(childToParent[0]);
        printf("%d: received %s\n", getpid(), result);
        write(childToParent[1],childWords,strlen(childWords));
        exit(0);
    }
}