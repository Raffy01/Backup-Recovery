#include "backup.h"

// 리스트 관리를 위한 구조체 및 전역 변수
typedef struct {
    char path[MAX_PATHLENGTH];
    bool is_dir;
} ListEntry;

ListEntry list_entries[1000]; 
int entry_count = 0;

/*
 * 백업된 파일/디렉토리를 시각적인 트리 형태로 출력한다.
 * char *path: 탐색할 디렉토리 경로
 * char *prefix: 상위 레벨에서 전달된 트리 가지 접두사
 */
void print_backup_tree(char *path, char *prefix) {
    struct dirent **namelist;
    int count;

    // 예외 처리: 디렉토리 스캔 실패 시 종료 
    if ((count = scandir(path, &namelist, NULL, alphasort)) < 0) return;

    // 현재 디렉토리에서 실제로 출력할 '마지막 유효 항목'의 인덱스를 먼저 찾음
    int last_valid_idx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(namelist[i]->d_name, ".") == 0 || strcmp(namelist[i]->d_name, "..") == 0) continue;
        
        char full_path[MAX_PATHLENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, namelist[i]->d_name);
        struct stat st;
        if (stat(full_path, &st) < 0) continue;

        if (S_ISDIR(st.st_mode) ? has_backup_record_in_dir(full_path) : has_backup_record(full_path)) {
            last_valid_idx = i;
        }
    }

    for (int i = 0; i < count; i++) {
        // 불필요한 항목 스킵
        if (strcmp(namelist[i]->d_name, ".") == 0 || strcmp(namelist[i]->d_name, "..") == 0) {
            free(namelist[i]);
            continue;
        }

        char full_path[MAX_PATHLENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, namelist[i]->d_name);
        struct stat st;
        if (stat(full_path, &st) < 0) {
            free(namelist[i]);
            continue;
        }

        // 백업 기록이 있는 경우에만 출력 프로세스 진행
        bool backed_up = S_ISDIR(st.st_mode) ? has_backup_record_in_dir(full_path) : has_backup_record(full_path);
        if (!backed_up) {
            free(namelist[i]);
            continue;
        }

        bool is_last = (i == last_valid_idx);
        char *branch = is_last ? "└─ " : "├─ ";

        // 인덱스와 트리 구조 출력
        printf("%d. %s%s", entry_count, prefix, branch);

        if (S_ISDIR(st.st_mode)) {
            printf("%s/\n", namelist[i]->d_name);
            strcpy(list_entries[entry_count].path, full_path);
            list_entries[entry_count++].is_dir = true;

            // 재귀 호출을 위한 새로운 접두사 생성
            // 현재가 마지막 항목이면 공백을, 아니면 수직선(│)을 추가함
            char new_prefix[MAX_PATHLENGTH];
            snprintf(new_prefix, sizeof(new_prefix), "%s%s   ", prefix, is_last ? " " : "│");
            print_backup_tree(full_path, new_prefix);
        } else {
            // 파일 정보 출력 (최근 백업 시간 및 크기) 
            Logs *node = find_backup_node(full_path);
            int size = get_filesize(full_path);
            printf("%s \t %s \t %dbytes\n", namelist[i]->d_name, node ? node->time : "no_record", size);
            strcpy(list_entries[entry_count].path, full_path);
            list_entries[entry_count++].is_dir = false;
        }

        free(namelist[i]);
    }
    free(namelist);
}

/*
 * 사용자의 list 명령을 처리하고 상호작용 루프를 실행한다.
 * int argc : 프로그램 실행 시 입력된 인자의 개수
 * char **argv : 프로그램 실행 시 입력된 인자 배열
 */
void cmd_list(int argc, char *argv[]) {
    (void)argc;
    char *target_path = (argv[2] == NULL) ? get_absolute_path(".") : get_absolute_path(argv[2]);
    
    if (target_path == NULL) {
        fprintf(stderr, "invalid path\n");
        return;
    }
    validate_path(target_path);

    while (1) {
        entry_count = 0;
        memset(list_entries, 0, sizeof(list_entries));

        printf("Backup list for: %s\n", target_path);
        print_backup_tree(target_path, ""); 

        if (entry_count == 0) {
            printf("no backup file(s) in backup directory\n");
            break;
        }

        printf(">> ");
        char input[MAX_BUFSIZE];
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0; 

        char *cmd = strtok(input, " ");
        if (cmd == NULL) continue;
        if (strcmp(cmd, "exit") == 0) break;

        char *idx_str = strtok(NULL, " ");
        if (idx_str == NULL) continue;
        int idx = atoi(idx_str);

        if (idx < 0 || idx >= entry_count) {
            fprintf(stderr, "invalid index\n");
            continue;
        }

        char *path = list_entries[idx].path;
        char *sub_opts = strtok(NULL, ""); 

        if (strcmp(cmd, "rm") == 0) {
            char *new_argv[] = {"backup", "remove", path, sub_opts};
            cmd_remove(sub_opts ? 4 : 3, new_argv);
        } 
        else if (strcmp(cmd, "rc") == 0) {
            char *new_argv[] = {"backup", "recover", path, sub_opts};
            cmd_recover(sub_opts ? 4 : 3, new_argv);
        } 
        else if (strcmp(cmd, "vi") == 0 || strcmp(cmd, "vim") == 0) {
            if (list_entries[idx].is_dir) {
                fprintf(stderr, "cannot edit directory\n");
                continue;
            }

            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "fork error\n");
            } else if (pid == 0) {
                // 자식 프로세스: vim 실행
                execlp("vim", "vim", path, (char *)NULL);
                
                // execlp가 실패했을 경우에만 아래 코드가 실행됨
                fprintf(stderr, "execlp error\n");
                exit(1);
            } else {
                // 부모 프로세스: 자식이 종료될 때까지 대기
                wait(NULL);
            }
        }
    }

    free(target_path);
}
