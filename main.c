#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NTL_RL_BUFSIZE 1024
char *ntl_read_line(void) {
    int bufsize = NTL_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "ntl: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1){
        c = getchar();

        if(c==EOF || c=='\n'){
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }

        position++;

        if(position >= bufsize){
            bufsize += NTL_RL_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if(!buffer){
                fprintf(stderr, "ntl: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

#define NTL_TOK_BUFSIZE 64
#define NTL_TOK_DELIM " \t\r\n\a"
char **ntl_split_line(char *line){
    int bufsize = NTL_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if(!tokens){
        fprintf(stderr, "ntl: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, NTL_TOK_DELIM);
    while (token != NULL){
        tokens[position] = token;
        position++;

        if (position >= bufsize){
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if(!tokens){
                fprintf(stderr, "ntl: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, NTL_TOK_DELIM);
    }

    tokens[position] = NULL;
    return tokens;
}

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
    char *line = NULL;
    char **args = NULL;
    int status = 1;
    do{
        printf("ntl> ");
        line = ntl_read_line();
        args = ntl_split_line(line);
        status = ntl_execute(args);
        free(line);
        free(args);
    } while(status);
}

int main() {
    ntl_loop();
    return EXIT_SUCCESS;
}

