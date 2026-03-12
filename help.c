#include "backup.h"

/*
 * 프로그램의 모든 내장 명령어에 대한 Usage를 출력한다.
 */
void print_usage_all(void) {
    printf("Usage:\n");
    printf(" > backup <PATH> [OPTION]... : backup file if <PATH> is file\n");
    printf("  -d: backup files in directory if <PATH> is directory\n");
    printf("  -r: backup files in directory recursive if <PATH> is directory\n");
    printf("  -y: backup file although already backuped\n");
    printf(" > remove <PATH> [OPTION]... : remove backuped file if <PATH> is file\n");
    printf("  -d: remove backuped files in directory if <PATH> is directory\n");
    printf("  -r: remove backuped files in directory recursive if <PATH> is directory\n");
    printf("  -a: remove all backuped files\n");
    printf(" > recover <PATH> [OPTION]... : recover backuped file if <PATH> is file\n");
    printf("  -d: recover backuped files in directory if <PATH> is directory\n");
    printf("  -r: recover backuped files in directory recursive if <PATH> is directory\n");
    printf("  -n <NEW_PATH>: recover backuped file with new path\n");
    printf(" > list [PATH]: show backup list by directory structure\n");
    printf("  >> rm <INDEX> [OPTION]...: remove backuped files of <INDEX> with <OPTION>\n");
    printf("  >> rc <INDEX> [OPTION]...: recover backuped files of <INDEX> with <OPTION>\n");
    printf("  >> vi(m) <INDEX>: edit original file of <INDEX>\n");
    printf("  >> exit: exit program\n");
    printf(" > help [COMMAND]: show commands for program\n");
}

/*
 * 특정 명령어에 대한 상세 Usage를 출력한다.
 * char *command: 사용법을 확인할 명령어 문자열
 */
void print_usage_detail(char *command) {
    if (strcmp(command, "backup") == 0) {
        printf("Usage: backup <PATH> [OPTION]...\n");
        printf(" -d: backup files in directory if <PATH> is directory\n");
        printf(" -r: backup files in directory recursive if <PATH> is directory\n");
        printf(" -y: backup file although already backuped\n");
        return;
    }

    if (strcmp(command, "remove") == 0) {
        printf("Usage: remove <PATH> [OPTION]...\n");
        printf(" -d: remove backuped files in directory if <PATH> is directory\n");
        printf(" -r: remove backuped files in directory recursive if <PATH> is directory\n");
        printf(" -a: remove all backuped files\n");
        return;
    }

    if (strcmp(command, "recover") == 0) {
        printf("Usage: recover <PATH> [OPTION]...\n");
        printf(" -d: recover backuped files in directory if <PATH> is directory\n");
        printf(" -r: recover backuped files in directory recursive if <PATH> is directory\n");
        printf(" -n <NEW_PATH>: recover backuped file with new path\n");
        return;
    }

    if (strcmp(command, "list") == 0) {
        printf("Usage: list [PATH]\n");
        printf(" >> rm <INDEX> [OPTION]...: remove backuped files of <INDEX> with <OPTION>\n");
        printf(" >> rc <INDEX> [OPTION]...: recover backuped files of <INDEX> with <OPTION>\n");
        printf(" >> vi(m) <INDEX>: edit original file of <INDEX>\n");
        printf(" >> exit: exit program\n");
        return;
    }

    if (strcmp(command, "help") == 0) {
        printf("Usage: help [COMMAND]\n");
        return;
    }

    // 예외 처리: 존재하지 않는 명령어를 입력했을 경우
    fprintf(stderr, "Invalid command: %s\n", command);
}

/*
 * 사용자의 help 명령을 처리한다.
 * char *input: 상세 설명을 보고 싶은 명령어 (생략 시 NULL)
 */
void print_help(char *input) {
    if (input == NULL) {
        print_usage_all();
        return;
    }

    // 인자가 있으면 해당 명령어 상세 출력
    print_usage_detail(input);
}
