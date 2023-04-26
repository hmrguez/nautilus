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
#define PIPE_FILE "pipe_file"
#define MAX_HISTORY_SIZE 10

char history[MAX_HISTORY_SIZE][MAXLINE];
int history_count = 0;

void init() {
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
        currentDirectory = (char *) calloc(1024, sizeof(char));
    } else {
        printf("Could not make the shell interactive.\n");
        exit(EXIT_FAILURE);
    }
}

void signalHandler_child(int p) {
    /* Wait for all dead processes.
     * We use a non-blocking call (WNOHANG) to be sure this signal handler will not
     * block if a child was cleaned up in another part of the program. */
    while (waitpid(-1, NULL, WNOHANG) > 0) {}

}

void signalHandler_int(int p) {
    // We send a SIGTERM signal to the child process
    if (kill(pid, SIGTERM) == 0) {
        printf("\nProcess %d received a SIGINT signal\n", pid);
        no_reprint_prmpt = 1;
    } else {
        printf("\n");
    }
}

int ntl_cd(char **args);

int ntl_help(char **args);

int ntl_exit(char **args);

int ntl_history(char **args);

int ntl_again(char **args);

int ntl_execute(char **args);

char *builtin_str[] = {
        "cd",
        "help",
        "exit",
        "history",
        "again"
};

int (*builtin_func[])(char **) = {
        &ntl_cd,
        &ntl_help,
        &ntl_exit,
        &ntl_history,
        &ntl_again
};

int ntl_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

int ntl_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "ntl: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
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

#define HISTORY_FILE "history"

int ntl_history(char **args) {
    FILE* file = fopen(HISTORY_FILE, "r");
    if (file == NULL) {
        printf("Error: could not open history file for reading.\n");
        return 0;
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int index = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        // Remove newline character at the end of the line.
        line[strcspn(line, "\n")] = '\0';

        printf("%d: %s\n", index, line);
        index++;
    }

    if (line) {
        free(line);
    }

    fclose(file);

    return 1;
}

#define MAX_HISTORY_LINES 10

void append_to_history(char** tokens, int num_tokens) {
    if(strcmp(tokens[0], "again") == 0) return;

    // Read the current history from the file into a buffer.
    char history_buffer[MAX_HISTORY_LINES][BUFSIZ];
    int history_count = 0;
    FILE* file = fopen(HISTORY_FILE, "r");
    if (file != NULL) {
        char line[BUFSIZ];
        while (fgets(line, sizeof(line), file) != NULL) {
            if (history_count < MAX_HISTORY_LINES) {
                strcpy(history_buffer[history_count], line);
                history_count++;
            }
        }
        fclose(file);
    }

    // Convert tokens to text and add the new line to the history buffer.
    char text[BUFSIZ] = "";
    for (int i = 0; i < num_tokens; i++) {
        strcat(text, tokens[i]);
        strcat(text, " ");
    }
    if (history_count < MAX_HISTORY_LINES) {
        sprintf(history_buffer[history_count], "%s\n", text);
        history_count++;
    } else {
        // If the history buffer is full, remove the first line.
        for (int i = 1; i < history_count; i++) {
            strcpy(history_buffer[i-1], history_buffer[i]);
        }
        sprintf(history_buffer[history_count-1], "%s\n", text);
    }

    // Write the history buffer back to the file.
    file = fopen(HISTORY_FILE, "w");
    if (file == NULL) {
        printf("Error: could not open file for writing.\n");
        return;
    }
    for (int i = 0; i < history_count; i++) {
        fprintf(file, "%s", history_buffer[i]);
    }
    fclose(file);
}

char* read_command_from_history(int index) {
    FILE* file = fopen(HISTORY_FILE, "r");
    if (file == NULL) {
        printf("Error: could not open history file for reading.\n");
        return NULL;
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    for (int i = 0; i <= index; i++) {
        read = getline(&line, &len, file);
        if (read == -1) {
            printf("Error: history file does not have enough commands.\n");
            fclose(file);
            return NULL;
        }
    }

    // Remove newline character at the end of the line.
    line[strcspn(line, "\n")] = '\0';

    fclose(file);
    return line;
}

int ntl_again(char** command_parts) {
    int index = atoi(command_parts[1]);
    int numTokens = 0;

    if (index >= 0 && index <= MAX_HISTORY_LINES) {
        char* command = read_command_from_history(index);
        if (command == NULL) return 1;

        char* tokens[LIMIT];

        printf("Executing command: %s\n", command);

        if ((tokens[0] = strtok(command, " \n\t")) == NULL) {
            free(command);
            return 1;
        }

        numTokens = 1;
        while ((tokens[numTokens] = strtok(NULL, " \n\t")) != NULL) {
            numTokens++;
        }

        int status = ntl_execute(tokens);
        append_to_history(tokens, numTokens);
        free(command);
    } else {
        printf("Invalid command index\n");
    }

    return 1;
}

/* ============================================================================================ */

void RedirectOutput(char *outputFile, int fileDescriptor) {
    fileDescriptor = open(outputFile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    dup2(fileDescriptor, STDOUT_FILENO);
    close(fileDescriptor);
}

void RedirectInput(char *inputFile, int fileDescriptor) {
    fileDescriptor = open(inputFile, O_RDONLY, 0600);
    dup2(fileDescriptor, STDIN_FILENO);
    close(fileDescriptor);
}

void RedirectAppendOutput(char *outputFile, int fileDescriptor) {
    fileDescriptor = open(outputFile, O_CREAT | O_APPEND | O_WRONLY, 0600);
    dup2(fileDescriptor, STDOUT_FILENO);
    close(fileDescriptor);
}

void RedirectPipeOutput(int *pipeFd) {
    dup2(pipeFd[1], STDOUT_FILENO);
    close(pipeFd[0]);
    close(pipeFd[1]);
    no_reprint_prmpt = 1;
}

int HandlePipeAndPipeOutput(int *pipeFd, char **pipeArgs, int option, char *outputFile, int fileDescriptor) {
    int err = -1;
    pid_t pid2;

    // execute the piped command
    if ((pid2 = fork()) == -1) {
        printf("Child process could not be created\n");
        return -1;
    }

    if (pid2 == 0) {
        // redirect input from pipe
        dup2(pipeFd[0], STDIN_FILENO);
        close(pipeFd[0]);
        close(pipeFd[1]);

        if (option == 1) {
            RedirectOutput(outputFile, fileDescriptor);
//            printf("Got to option 1");
        }

        if (execvp(pipeArgs[0], pipeArgs) == -1) {
            printf("err");
            kill(getpid(), SIGTERM);
            return -1;
        }
    }

    no_reprint_prmpt = 0;

    close(pipeFd[0]);
    close(pipeFd[1]);
    waitpid(pid2, NULL, 0);

    return 1;
}

int ntl_launch(char **args, char *inputFile, char *outputFile, int option) {
    int err = -1;
    int fileDescriptor;
    int status = 0;
    int wasBuiltin = 0;

    if ((pid = fork()) == -1) {
        printf("Child process could not be created\n");
        return -1;
    }

    if (pid == 0) {
        if (isPiping == 1) {
            RedirectInput(PIPE_FILE, fileDescriptor);
            isPiping = 0;
        }

        //
        if (option == 1) {
            RedirectOutput(outputFile, fileDescriptor);
        } else if (option == 2) {
            RedirectInput(inputFile, fileDescriptor);
        } else if (option == 3) {
            RedirectAppendOutput(outputFile, fileDescriptor);
        } else if (option == 5) {
            RedirectInput(inputFile, fileDescriptor);
            RedirectOutput(outputFile, fileDescriptor);
        } else if (option == 7) {
            RedirectInput(inputFile, fileDescriptor);
            RedirectAppendOutput(outputFile, fileDescriptor);
        }

        for (int j = 0; j < ntl_num_builtins(); ++j) {
            if (strcmp(args[0], builtin_str[j]) == 0) {
                wasBuiltin = 1;
                status = (*builtin_func[j])(args);
                if(status == 0) return 0;
            }
        }

        if (wasBuiltin != 1 && execvp(args[0], args) == err) {
            printf("err");
            kill(getpid(), SIGTERM);
            return -1;
        }
    }

    waitpid(pid, NULL, 0);

    return status;
}

int ntl_parsing(char **commands, char **separators, int numCommands, int numSeparators) {
    int executed = 0;
    int currSeparator = 0;
    int start = 0;
    int buffer = 0;
    char *buffer_files[] = {"buffer_file1", "buffer_file2"};

    if(strcmp(commands[0], "exit") == 0) return 0;

    for (int i = 0; i < numCommands; i++) {
        if (executed == 0) {
            start = i;
            if (separators[currSeparator] != NULL &&
                (strcmp(separators[currSeparator], ">") == 0 || strcmp(separators[currSeparator], ">>") == 0)) {
                while (commands[i++] != NULL);
                if (commands[i] == NULL) {
                    printf("Not enough arguments");
                    return 1;
                }

                if (strcmp(separators[currSeparator], ">") == 0) {
                    ntl_launch(commands + start, NULL, commands[i], 1);
                } else {
                    ntl_launch(commands + start, NULL, commands[i], 3);
                }

                currSeparator++;
            } else if (separators[currSeparator] != NULL && strcmp(separators[currSeparator], "<") == 0) {
                while (commands[i++] != NULL);
                if (commands[i] == NULL) {
                    printf("Not enough arguments");
                    return 1;
                }

                if (separators[currSeparator + 1] != NULL && strcmp(separators[currSeparator + 1], ">") == 0) {
                    ntl_launch(commands + start, commands[i], commands[i + 2], 5);
                    i += 2;
                    currSeparator++;
                } else if (separators[currSeparator + 1] != NULL && strcmp(separators[currSeparator + 1], "|") == 0) {
                    ntl_launch(commands + start, commands[i], PIPE_FILE, 5);
                    isPiping = 1;
                    currSeparator++;
                } else if (separators[currSeparator + 1] != NULL && strcmp(separators[currSeparator + 1], ">>") == 0) {
                    ntl_launch(commands + start, commands[i], commands[i + 2], 7);
                    i += 2;
                    currSeparator++;
                } else {
                    ntl_launch(commands + start, commands[i], NULL, 2);
                }

                currSeparator++;
            } else if (separators[currSeparator] != NULL && strcmp(separators[currSeparator], "|") == 0) {
                while (commands[i++] != NULL);
                if (commands[i] == NULL) {
                    printf("Not enough arguments");
                    return 1;
                }

                do {
                    if(currSeparator > 0 && strcmp(separators[currSeparator - 1], "|") == 0){
                        ntl_launch(commands + start, buffer_files[buffer ^ 1], buffer_files[buffer], 5);
                    }
                    else{
                        ntl_launch(commands + start, NULL, buffer_files[buffer], 1);
                    }
//                    ntl_launch(commands + start,
//                               (currSeparator > 0 && strcmp(separators[currSeparator - 1], "|") == 0) ? buffer_files[
//                                       buffer ^ 1] : NULL,
//                               buffer_files[buffer], 1);

                    // Advance to the next command
                    start = i++;
                    while (commands[i++] != NULL);

                    // Swap the buffer index
                    buffer ^= 1;
                    currSeparator++;


                } while (separators[currSeparator] != NULL && strcmp(separators[currSeparator], "|") == 0);

//                printf("%s", separators[currSeparator]);

                // Execute the last command in the pipe chain

                if(separators[currSeparator] != NULL &&
                    (strcmp(separators[currSeparator], ">") == 0 || strcmp(separators[currSeparator], ">>") == 0)){

                    while (commands[i++] != NULL);
                    i-=2;

                    if (commands[i] == NULL) {
                        printf("Not enough arguments");
                        return 1;
                    }

                    if (strcmp(separators[currSeparator], ">") == 0) {
                        ntl_launch(commands + start, buffer_files[buffer ^ 1], commands[i++], 5);
                    } else {
                        ntl_launch(commands + start, buffer_files[buffer ^ 1], commands[i++], 7);
                    }

                    currSeparator++;
                }
                else{
                    ntl_launch(commands + start, buffer_files[buffer ^ 1], NULL, 2);
                }

                currSeparator++;
                i -= 2;
            } else {
                ntl_launch(commands + start, NULL, NULL, 0);
            }

            // Mark as executed
            executed = 1;
        } else if (commands[i] == NULL) {
            // Unmark as executed
            executed = 0;
        }
    }

    // Remove the buffer files after the execution
    remove(buffer_files[0]);
    remove(buffer_files[1]);
    remove(PIPE_FILE);
}

int ntl_execute(char **args) {

    char **commands = malloc(sizeof(char *) * (LIMIT + 1));
    char **separators = malloc(sizeof(char *) * (LIMIT + 1));
    int numCommands = 0;
    int numSeparators = 0;

    if(strcmp(args[0], "exit") == 0) return ntl_exit(args);

    if(strcmp(args[0], ">") == 0 && args[1] != NULL){
        args[0] = "touch";
    }


    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], "|") == 0 ||
            strcmp(args[i], ">>") == 0) {
            separators[numSeparators++] = args[i];
            commands[numCommands++] = NULL; // mark end of previous command
        } else if (strcmp(args[i], "#") == 0) {
            break;
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
    char *tokens[LIMIT];
    int numTokens;
    int status = 1;
    pid = -10;
    init();


    do {
        printf("nautilus $ ");
        memset(line, '\0', MAXLINE);

        fgets(line, MAXLINE, stdin);
        line[strcspn(line, "\n")] = 0;
        if ((tokens[0] = strtok(line, " \n\t")) == NULL) continue;


        numTokens = 1;
        while ((tokens[numTokens] = strtok(NULL, " \n\t")) != NULL) numTokens++;

        append_to_history(tokens, numTokens);
        status = ntl_execute(tokens);

    } while (status);
}

int main() {
    ntl_loop();
    return EXIT_SUCCESS;
}