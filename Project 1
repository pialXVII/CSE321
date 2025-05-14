#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT 2048
#define MAX_ARGS 1000
#define MAX_CMDS 50
#define MAX_HISTORY 100

char *history[MAX_HISTORY];
int history_count = 0;


void add_to_history(const char *cmd) {
    if (history_count < MAX_HISTORY) {
        history[history_count++] = strdup(cmd);
    }
}

void sigint_handler(int signo) {
    printf("\nsh> ");
    fflush(stdout);
}

void setup_signal_handling() {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
}

void save_as_string(char *input_text, char **args) {
    int i = 0;
    args[i] = strtok(input_text, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        args[i] = strtok(NULL, " ");
    }
    args[i] = NULL;
}

void redirectOutput(const char* file, int append) {
    int fd = open(file, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
    if (fd < 0) { perror("Output file error"); exit(1); }
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

void handleRedirection(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            int fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) {
                perror("Input file error");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
        } else if (strcmp(args[i], ">") == 0) {
            redirectOutput(args[i + 1], 0);
            args[i] = NULL;
        } else if (strcmp(args[i], ">>") == 0) {
            redirectOutput(args[i + 1], 1);
            args[i] = NULL;
        }
    }
}

void execute_piped_commands(char ***commands, int n) {
    int i, pipefd[2], in_fd = 0;

    for (i = 0; i < n; i++) {
        pipe(pipefd);
        pid_t child2 = fork();
        if (child2 == 0) {
            dup2(in_fd, 0);
            if (i < n - 1)
                dup2(pipefd[1], 1);
            close(pipefd[0]);
            handleRedirection(commands[i]);
            execvp(commands[i][0], commands[i]);
            perror("exec");
            exit(1);
        } else {
            wait(NULL);
            close(pipefd[1]);
            in_fd = pipefd[0];
        }
    }
}

void executeCommand(char *cmd) {
    char *args[MAX_ARGS];
    save_as_string(cmd, args);
    handleRedirection(args);
    execvp(args[0], args);
    perror("exec");
    exit(1);
}
void executeConditional(char *line) ;

void execute_sequential(char *line) {
    char *token = strtok(line, ";");
    while (token != NULL) {
        if (strstr(token, "&&") != NULL) {
            executeConditional(token);
        } else {
            pid_t child3 = fork();
            if (child3 == 0) {
                executeCommand(token);
            } else {
                wait(NULL);
            }
        }
        token = strtok(NULL, ";");
    }
}

void executeConditional(char *line) {
    char *cmd1 = strtok(line, "&&");
    char *cmd2 = strtok(NULL, "");

    if (cmd1 == NULL) return;

    pid_t child1 = fork();
    int status;
    if (child1 == 0) {
        executeCommand(cmd1);
    } else {
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0 && cmd2 != NULL) {
            execute_sequential(cmd2);
        }
    }
}

int main() {
    char input_text[MAX_INPUT];

    setup_signal_handling();

    while (1) {
        printf("sh> ");
        fflush(stdout);

        if (fgets(input_text, MAX_INPUT, stdin) == NULL) {
            printf("\n");
            break;
        }

        input_text[strcspn(input_text, "\n")] = 0;

        if (strcmp(input_text, "exit") == 0) break;

        if (strcmp(input_text, "history") == 0) {
            for (int i = 0; i < history_count; i++)
                printf("%d: %s\n", i + 1, history[i]);
            continue;
        }

        add_to_history(input_text);

        if (strchr(input_text, '|')) {
            char *pipe_parts[MAX_CMDS];
            int num_cmds = 0;
            pipe_parts[num_cmds] = strtok(input_text, "|");
            while (pipe_parts[num_cmds] != NULL && num_cmds < MAX_CMDS - 1) {
                num_cmds++;
                pipe_parts[num_cmds] = strtok(NULL, "|");
            }

            char **commands[MAX_CMDS];
            for (int i = 0; i < num_cmds; i++) {
                commands[i] = malloc(MAX_ARGS * sizeof(char *));
                save_as_string(pipe_parts[i], commands[i]);
            }

            execute_piped_commands(commands, num_cmds);

            for (int i = 0; i < num_cmds; i++)
                free(commands[i]);

        } else {
            execute_sequential(input_text);
        }
    }

    return 0;
}
