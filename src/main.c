#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/wait.h>

#define MAX_LINE 4096
#define MAX_ARGS 64

void printUser(void);
void inputParser(char* cmdLine, char** options);
void pipelineFunc(char* cmdline);
int execute_command(char* cmdLine);

int main(void) {
    char cmdLine[MAX_LINE];

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

        if (strlen(cmdLine) == 0) continue;

        char* rest = cmdLine;
        char* token;
        int last_status = 0;
        char* op = NULL;

        while ((token = strsep(&rest, ";&|")) != NULL) {
            while (*token == ' ') token++; // 앞 공백 제거
            if (strlen(token) == 0) continue;

            int run = 1;
            if (op) {
                if (strcmp(op, "&&") == 0 && last_status != 0) run = 0;
                if (strcmp(op, "||") == 0 && last_status == 0) run = 0;
            }

            if (run) {
                last_status = execute_command(token);
            }

            if (rest) {
                op = rest;
                if (strncmp(op, "&&", 2) == 0 || strncmp(op, "||", 2) == 0) {
                    rest += 2;
                } else {
                    rest += 1;
                }
            }
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

int execute_command(char* cmdLine) {
    char* options[MAX_ARGS];
    int background = 0;

    char* amp = strchr(cmdLine, '&');
    if (amp) {
        background = 1;
        *amp = '\0';
    }

    inputParser(cmdLine, options);
    if (options[0] == NULL) return 0;

    // 내장 명령어 처리
    if (strcmp(options[0], "cd") == 0) {
        const char* target = options[1];
        if (target == NULL || strcmp(target, "~") == 0) target = getenv("HOME");
        if (target && chdir(target) != 0) printf("Cannot execute cd");
        return 0;
    }
    if (strcmp(options[0], "pwd") == 0) {
        char pwd[PATH_MAX];
        if (getcwd(pwd, sizeof(pwd)) != NULL) printf("%s\n", pwd);
        else printf("Cannot execute pwd");
        return 0;
    }
    if (strcmp(options[0], "exit") == 0) exit(0);

    // 외부 명령 실행
    pid_t pid = fork();
    if (pid == 0) {
        execvp(options[0], options);
        printf("Cannot execute execvp");
        exit(1);
    } else {
        if (!background) waitpid(pid, NULL, 0);
        else printf("[BG PID %d started]\n", pid);
        return 0;
    }
}
