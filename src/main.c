#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX_LINE 4096
#define MAX_ARGS 64

void printUser(void);
void inputParser(char* cmdLine, char** options);
void pipelineFunc(char* cmdline);

int main(void) {
    char cmdLine[MAX_LINE];
    char* options[MAX_ARGS];

    while (1) {
        printUser();

        if (!fgets(cmdLine, MAX_LINE, stdin)) {
            break;
        }

        cmdLine[strcspn(cmdLine, "\n")] = '\0';
        
        if (strchr(cmdLine, '|') != NULL) {
            pipelineFunc(cmdLine);
            continue;
        }

        inputParser(cmdLine, options);
        
        if (strlen(cmdLine) == 0) continue;
        if (options[0] == NULL) continue;

        
        if (strcmp(options[0], "exit") == 0) {
            break;
        }

        
        if (strcmp(options[0], "cd") == 0) {
            const char* target = options[1];

            if (target == NULL) {
                target = getenv("HOME");
            } else if (strcmp(target, "~") == 0) {
                target = getenv("HOME");
            }

            if (target && chdir(target) != 0) {
                printf("Failed to move directory");
            }

            continue;
        }
        
        if (strcmp(options[0], "pwd") == 0) {
            char pwd[PATH_MAX];
            if (getcwd(pwd, sizeof(pwd)) != NULL) {
                printf("%s\n", pwd);
            } else {
                printf("Failed to load location of current directory");
            }
            continue;
        }
        
        if (strchr(cmdLine, '|') != NULL) {
            pipelineFunc(cmdLine);
            continue;
        }


        pid_t pid = fork();
        if (pid < 0) {
            printf("Cannot execute fork");
            continue;
        } else if (pid == 0) {
            execvp(options[0], options);
            printf("Cannot execute execvp");
            exit(1);
        } else {
            wait(NULL);
        }
    }

    return 0;
}

void printUser(void) {
    char cwd[PATH_MAX];
    char hostname[_SC_HOST_NAME_MAX];
    char* username = getpwuid(getuid())->pw_name;

    gethostname(hostname, sizeof(hostname));
    getcwd(cwd, sizeof(cwd));

    const char* home = getenv("HOME");
    if (home && strstr(cwd, home) == cwd) {
        printf("[%s@%s:%s]$ ", username, hostname, cwd);
    } else {
        printf("[%s@%s:%s]$ ", username, hostname, cwd);
    }
}

void inputParser(char* cmdLine, char** options) {
    int i = 0;
    char* token = strtok(cmdLine, " ");
    while (token != NULL && i < MAX_ARGS - 1) {
        options[i++] = token;
        token = strtok(NULL, " ");
    }
    options[i] = NULL;
}

void pipelineFunc(char* cmdLine) {
    char* commands[16];
    int num_cmds = 0;

    char* token = strtok(cmdLine, "|");
    while (token != NULL && num_cmds < 16) {
        while (*token == ' ') token++;
        commands[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int prev_pipe[2] = {-1, -1};

    for (int i = 0; i < num_cmds; i++) {
        int pipefd[2];
        if (i < num_cmds - 1) pipe(pipefd);

        pid_t pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(prev_pipe[0], 0);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }

            if (i < num_cmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], 1);
                close(pipefd[1]);
            }

            char* args[MAX_ARGS];
            inputParser(commands[i], args);
            execvp(args[0], args);
            printf("Cannot execute execvp");
            exit(1);
        }

        
        if (i > 0) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }

        if (i < num_cmds - 1) {
            prev_pipe[0] = pipefd[0];
            prev_pipe[1] = pipefd[1];
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
}
 
