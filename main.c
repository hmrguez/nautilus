#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include "include/util.h"

#define MAXLINE 1024
#define LIMIT 256

void init(){
    // See if we are running interactively
    NTL_PID = getpid();
    // The shell is interactive if STDIN is the terminal
    NTL_IS_INTERACTIVE = isatty(STDIN_FILENO);

    if (NTL_IS_INTERACTIVE) {
        // Loop until we are in the foreground
        while (tcgetpgrp(STDIN_FILENO) != (NTL_PGID = getpgrp()))
            kill(NTL_PID, SIGTTIN);


        // Set the signal handlers for SIGCHILD and SIGINT
        act_child.sa_handler = signalHandler_child;
        act_int.sa_handler = signalHandler_int;


        sigaction(SIGCHLD, &act_child, 0);
        sigaction(SIGINT, &act_int, 0);

        // Put ourselves in our own process group
        setpgid(NTL_PID, NTL_PID); // we make the shell process the new process group leader
        NTL_PGID = getpgrp();
        if (NTL_PID != NTL_PGID) {
            printf("Error, the shell is not process group leader");
            exit(EXIT_FAILURE);
        }
        // Grab control of the terminal
        tcsetpgrp(STDIN_FILENO, NTL_PGID);

        // Save default terminal attributes for shell
        tcgetattr(STDIN_FILENO, &NTL_TMODES);

        // Get the current directory that will be used in different methods
        currentDirectory = (char*) calloc(1024, sizeof(char));
    } else {
        printf("Could not make the shell interactive.\n");
        exit(EXIT_FAILURE);
    }
}

void signalHandler_child(int p){
    /* Wait for all dead processes.
     * We use a non-blocking call (WNOHANG) to be sure this signal handler will not
     * block if a child was cleaned up in another part of the program. */
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    printf("\n");
}

void signalHandler_int(int p){
    // We send a SIGTERM signal to the child process
    if (kill(pid,SIGTERM) == 0){
        printf("\nProcess %d received a SIGINT signal\n",pid);
        no_reprint_prmpt = 1;
    }else{
        printf("\n");
    }
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

#define READ_END 0
#define WRITE_END 1

int ntl_pipe(char **args, char **pipedArgs){
    int pipefd[2];

    // Create pipe
    if (pipe(pipefd) == -1) {
        perror("Failed to create pipe");
        exit(EXIT_FAILURE);
    }

    // Fork first child
    pid = fork();
    if (pid == -1) {
        perror("Failed to fork first child");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // First child process: redirect stdout to pipe write end
        close(pipefd[READ_END]);
        dup2(pipefd[WRITE_END], STDOUT_FILENO);
        close(pipefd[WRITE_END]);

        // Execute first command
        if (execvp(args[0], args) == -1) {
            perror("Failed to execute first command");
            exit(EXIT_FAILURE);
        }
    }

    // Fork second child
    pid = fork();
    if (pid == -1) {
        perror("Failed to fork second child");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Second child process: redirect stdin to pipe read end
        close(pipefd[WRITE_END]);
        dup2(pipefd[READ_END], STDIN_FILENO);
        close(pipefd[READ_END]);

        // Execute second command
        if (execvp(pipedArgs[0], pipedArgs) == -1) {
            perror("Failed to execute second command");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process: wait for both children to finish
    close(pipefd[READ_END]);
    close(pipefd[WRITE_END]);
    wait(NULL);
    wait(NULL);

    return 1;
}

int ntl_launch(char **args, char* inputFile, char* outputFile, char **pipeArgs, int option){
    int err = -1;
    int fileDescriptor;
    int pipeFd[2];
    pid_t pid2;

    if (option == 4 && pipe(pipeFd) < 0) {
        perror("Pipe error");
        return -1;
    }

    if((pid=fork())==-1){
        printf("Child process could not be created\n");
        return -1;
    }

    if(pid==0){
        if (option == 1){
            fileDescriptor = open(outputFile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
            dup2(fileDescriptor, STDOUT_FILENO);
            close(fileDescriptor);
        }
        else if (option == 2){
            fileDescriptor = open(inputFile, O_RDONLY, 0600);
            dup2(fileDescriptor, STDIN_FILENO);
            close(fileDescriptor);
        }
        else if(option == 3){
            fileDescriptor = open(outputFile, O_CREAT | O_APPEND | O_WRONLY, 0600);
            dup2(fileDescriptor, STDOUT_FILENO);
            close(fileDescriptor);
        }

        if (option == 4) {
            // redirect output to pipe
            dup2(pipeFd[1], STDOUT_FILENO);
            close(pipeFd[0]);
            close(pipeFd[1]);
        }

        if (execvp(args[0],args)==err){
            printf("err");
            kill(getpid(),SIGTERM);
            return 1;
        }
    }

    if (option == 4) {
        // execute the piped command
        if((pid2=fork())==-1){
            printf("Child process could not be created\n");
            return -1;
        }

        if (pid2 == 0) {
            // redirect input from pipe
            dup2(pipeFd[0], STDIN_FILENO);
            close(pipeFd[0]);
            close(pipeFd[1]);

            if (execvp(pipeArgs[0], pipeArgs) == err){
                printf("err");
                kill(getpid(),SIGTERM);
                return 1;
            }
        }

        close(pipeFd[0]);
        close(pipeFd[1]);
        waitpid(pid2,NULL,0);
    }

    waitpid(pid,NULL,0);

    return 1;
}

int ntl_parsing(char **commands, char **separators, int numCommands, int numSeparators){
    int executed = 0;
    int currSeparator = 0;
    int start = 0;

    for (int i = 0; i < numCommands; i++) {
        if(executed == 0){
            start = i;
            if(separators[currSeparator] != NULL && (strcmp(separators[currSeparator], ">") == 0 || strcmp(separators[currSeparator], ">>") == 0)){
                while (commands[i++] != NULL);
                if(commands[i] == NULL){
                    printf("Not enough arguments");
                    return 1;
                }

                if(strcmp(separators[currSeparator], ">") == 0){
                    // Option 1 for >
                    ntl_launch(commands + start, NULL, commands[i], NULL, 1);
                }
                else{
                    //Option 3 for >>
                    ntl_launch(commands + start, NULL, commands[i], NULL, 3);
                }
                currSeparator++;
            }
            else if(separators[currSeparator] != NULL && strcmp(separators[currSeparator], "<") == 0){
                if(separators[currSeparator+1] != NULL && strcmp(separators[currSeparator+1], ">") == 0){
                    printf("WIP!");
                }
                else if(separators[currSeparator+1] != NULL && strcmp(separators[currSeparator + 1], "|") == 0){
                    printf("WIP!");
                }
                else if(separators[currSeparator+1] != NULL && strcmp(separators[currSeparator + 1], ">>") == 0){
                    printf("WIP!");
                }
                else{
                    while (commands[i++] != NULL);
                    if(commands[i] == NULL){
                        printf("Not enough arguments");
                        return 1;
                    }

                    // Option 2 for <
                    ntl_launch(commands + start, commands[i], NULL, NULL, 2);
                }

                currSeparator++;
            }
            else if(separators[currSeparator] != NULL && strcmp(separators[currSeparator], "|") == 0){
                while (commands[i++] != NULL);
                if(commands[i] == NULL){
                    printf("Not enough arguments");
                    return 1;
                }

                ntl_launch(commands + start, NULL, NULL, commands + i, 4);
            }
            else{
                ntl_launch(commands + start, NULL, NULL, NULL, 0);
            }


            // Mark as executed
            executed = 1;
        }
        else if(commands[i] == NULL){
            //Unmark as executed
            executed = 0;
        }
    }
}

int ntl_execute(char **args){

    char **commands = malloc(sizeof(char*) * (LIMIT + 1));
    char **separators = malloc(sizeof(char*) * (LIMIT + 1));
    int numCommands = 0;
    int numSeparators = 0;

    for (int j = 0; j < ntl_num_builtins(); ++j) {
        if (strcmp(args[0], builtin_str[j]) == 0) {
            return (*builtin_func[j])(args);
        }
    }

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], "|") == 0 || strcmp(args[i], ">>") == 0) {
            separators[numSeparators++] = args[i];
            commands[numCommands++] = NULL; // mark end of previous command
        } else {
            commands[numCommands++] = args[i];
        }
    }
    commands[numCommands] = NULL; // mark end of last command

    ntl_parsing(commands, separators, numCommands, numSeparators);

    free(commands);
    free(separators);
    return 1;
}

void ntl_loop() {
    char line[MAXLINE];
    char * tokens[LIMIT];
    int numTokens;
    int status = 1;
    pid = -10;
    init();


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

