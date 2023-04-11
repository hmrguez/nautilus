#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXLINE 1024
#define LIMIT 256

int ntl_launch(char **args){
    pid_t pid, wpid;
    int status;

    pid = fork();
    if(pid == 0){
        if(execvp(args[0], args) == -1){
            perror("ntl");
        }
        exit(EXIT_FAILURE);
    } else if(pid < 0){
        perror("ntl");
    } else {
        do{
            wpid = waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int ntl_cd(char **args);
int ntl_help(char **args);
int ntl_exit(char **args);

char *builtin_str[] = {
        "cd",
        "help",
        "exit"
};

int (*builtin_func[]) (char **) = {
        &ntl_cd,
        &ntl_help,
        &ntl_exit
};

int ntl_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

int ntl_cd(char **args){
    if(args[1] == NULL){
        fprintf(stderr, "ntl: expected argument to \"cd\"\n");
    } else {
        if(chdir(args[1]) != 0){
            perror("ntl");
        }
    }
    return 1;
}

int ntl_help(char **args) {
    int i;
    printf("Ntl shell");

    for (int j = 0; j < ntl_num_builtins(); ++j) {
        printf(" %s", builtin_str[j]);
    }

    printf("If you need any help please help me too");
    return 1;
}

int ntl_exit(char **args) {
    return 0;
}

int ntl_execute(char **args){
    int i;

    if(args[0] == NULL){
        return 1;
    }

    for (int j = 0; j < ntl_num_builtins(); ++j) {
        if(strcmp(args[0], builtin_str[j]) == 0){
            return (*builtin_func[j])(args);
        }
    }

    return ntl_launch(args);
}

void ntl_loop() {
    char line[MAXLINE];
    char * tokens[LIMIT];
    int numTokens;
    int status = 1;

    do{
        printf("ntl> ");

        memset ( line, '\0', MAXLINE );

        fgets(line, MAXLINE, stdin);
        if((tokens[0] = strtok(line," \n\t")) == NULL) continue;

        numTokens = 1;
        while((tokens[numTokens] = strtok(NULL, " \n\t")) != NULL) numTokens++;

        status = ntl_execute(tokens);

    } while(status);
}

int main() {
    ntl_loop();
    return EXIT_SUCCESS;
}

