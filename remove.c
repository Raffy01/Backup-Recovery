#include "backup.h"

/*
 * 경로 문자열이 특정 디렉토리 내부에 속하는지 검사한다.
 * const char *origin: 검사할 대상의 전체 경로
 * const char *dir_path: 기준 디렉토리 경로
 * bool recursive: 하위 디렉토리 전체 포함 여부 (true: -r 옵션, false: -d 옵션)
 * return : bool, 조건에 부합하면 true, 아니면 false
 */
static bool is_in_dir(const char *origin, const char *dir_path, bool recursive) {
    int dir_len = strlen(dir_path);
    
    // 예외 처리: 기준 경로로 시작하지 않으면 거짓
    if (strncmp(origin, dir_path, dir_len) != 0) return false;
    
    // 예외 처리: 정확히 하위 항목인지 검사 (ex: /home/a 와 /home/abc 구분)
    if (origin[dir_len -1 ] != '/') {
		if (origin[dir_len] != '/') return false; 
    }

    // 재귀 옵션(-r)이 없을 경우의 예외 처리
    if (!recursive) {
        // 바로 아래 계층의 파일만 허용하므로, 추가적인 '/' 가 발견되면 거짓 반환
        if (strchr(origin + dir_len + 1, '/') != NULL) {
            return false;
        }
    }
    return true;
}

/*
 * 특정 원본 파일에 대한 백업본을 찾아 삭제한다.
 * char *path: 삭제할 원본 파일의 절대 경로
 * bool a_flag: 모든 백업본 강제 삭제 여부 플래그 (-a)
 */
void do_remove_file(char *path, bool a_flag) {
    int count = 0;
    Logs *curr = head;
    
    // 1. 해당 경로의 백업본 개수 파악
    while (curr != NULL) {
        if (strcmp(curr->origin, path) == 0) count++;
        curr = curr->next;
    }

    // 예외 처리: 백업본이 하나도 존재하지 않을 경우 안내 후 종료
    if (count == 0) { 
        printf("\"%s\" is not backuped\n", path);
        return;
    }

    // 2. 정확한 크기로 포인터 배열 동적 할당 (버퍼 오버플로우 버그 완벽 수정)
    Logs **data = (Logs **)malloc(sizeof(Logs *) * count);
    int idx = 0;
    curr = head;
    while (curr != NULL) {
        if (strcmp(curr->origin, path) == 0) {
            data[idx++] = curr;
        }
        curr = curr->next;
    }

    // 3. 삭제 로직 처리 (백업본이 여러 개인 경우와 한 개인 경우 분기)
    if (count > 1) {
        if (!a_flag) { 
            // -a 옵션이 없을 때: 목록 출력 후 사용자에게 입력받음
            printf("backup files of %s\n", path);
            printf("0. exit\n");
            for(int i = 0; i < count; i++) {
                int filesize = get_filesize(data[i]->backup);
                printf("%d. %s \t %dbytes\n", i + 1, data[i]->time, filesize);
            }
            
            printf(">> ");
            int input;
            // 예외 처리: 정수가 아닌 잘못된 값이 입력되었을 경우 에러 처리
            if (scanf("%d", &input) != 1) {
                fprintf(stderr, "invalid input\n");
                free(data);
                exit(1);
            }
            
            // 입력 버퍼 비우기
            while (getchar() != '\n');

            if (input == 0) {
                free(data);
                return;
            } else if (input > 0 && input <= count) {
                char *curtime = get_current_time();
                Logs *target = data[input - 1];
                
                remove(target->backup);
                printf("\"%s\" removed by \"%s\"\n", target->backup, target->origin);
                log_write(curtime, target->backup, target->origin, "removed by");
                log_delete_node(target);
                
                free(curtime);
            } else {
                // 예외 처리: 범위를 벗어난 번호 입력 시
                fprintf(stderr, "invalid input\n");
                free(data);
                exit(1);
            }
        } else { 
            // -a 옵션이 있을 때: 묻지 않고 모두 삭제
            for(int i = 0; i < count; i++) {
                char *curtime = get_current_time();
                Logs *target = data[i];
                
                remove(target->backup);
                printf("\"%s\" removed by \"%s\"\n", target->backup, target->origin);
                log_write(curtime, target->backup, target->origin, "removed by");
                log_delete_node(target);
                
                free(curtime);
            }
        }
    } else { 
        // 백업본이 1개일 때는 옵션에 상관없이 바로 삭제
        char *curtime = get_current_time();
        Logs *target = data[0];
        
        remove(target->backup);
        printf("\"%s\" removed by \"%s\"\n", target->backup, target->origin);
        log_write(curtime, target->backup, target->origin, "removed by");
        log_delete_node(target);
        
        free(curtime);
    }

    free(data); 
}

/*
 * 디렉토리 하위를 탐색하며 조건에 맞는 백업 파일들을 삭제한다.
 * char *path: 탐색을 시작할 디렉토리 경로
 * bool a_flag: 모든 백업본 강제 삭제 여부 플래그
 * bool r_flag: 하위 디렉토리 재귀 탐색 플래그
 */
void do_remove_dir(char *path, bool a_flag, bool r_flag) {
    struct dirent **namelist;
    int count;
    char tmp[MAX_PATHLENGTH];
    struct stat statbuf;

    // 예외 처리: 디렉토리 스캔 실패 시 에러 출력 후 종료
    if ((count = scandir(path, &namelist, NULL, alphasort)) < 0) {
        fprintf(stderr, "scandir error for %s\n", path);
        exit(1);
    }

    // 파일과 디렉토리를 분리해서 처리하기 위한 플래그 배열
    bool *is_dir = (bool *)malloc(sizeof(bool) * count);

    // 1차 순회: 파일들을 먼저 처리 (BFS 패턴)
    for (int i = 0; i < count; i++) {
        if (strcmp(namelist[i]->d_name, ".") == 0 || strcmp(namelist[i]->d_name, "..") == 0) {
            is_dir[i] = false;
            continue;
        }
        
        snprintf(tmp, sizeof(tmp), "%s/%s", path, namelist[i]->d_name);
        
        // 예외 처리: stat 실패 시 해당 항목 건너뜀
        if (stat(tmp, &statbuf) < 0) {
            is_dir[i] = false;
            continue;
        }
        
        if (S_ISREG(statbuf.st_mode)) { 
            is_dir[i] = false;
            do_remove_file(tmp, a_flag);
        } else if (S_ISDIR(statbuf.st_mode)) {
            // 하위 디렉토리는 체크만 해두고 나중에 재귀로 처리
            is_dir[i] = true;
        }
    }

    // 2차 순회 (재귀): -r 옵션이 켜져 있을 경우 체크해둔 하위 디렉토리 진입
    if (r_flag) { 
        for (int i = 0; i < count; i++) {
            if (is_dir[i]) {
                snprintf(tmp, sizeof(tmp), "%s/%s", path, namelist[i]->d_name);
                // 재귀 호출: 하위 디렉토리 내부를 지우기 위해 다시 호출
                do_remove_dir(tmp, a_flag, r_flag);
            }
        }
    }

    for(int i = 0; i < count; i++) free(namelist[i]);
    free(namelist);
    free(is_dir);
}

/*
 * 사용자의 remove 명령을 처리한다. 옵션과 경로를 파싱하여 알맞은 삭제 함수를 호출한다.
 * int argc: 프로그램 실행 시 입력된 인자의 개수
 * char **argv: 프로그램 실행 시 입력된 인자 배열
 */
void cmd_remove(int argc, char *argv[]) {
    bool d_flag = false;
    bool r_flag = false;
    bool a_flag = false;

    // 예외 처리: 대상 경로 인자가 없을 경우 도움말 출력
    if (argv[2] == NULL) {
        print_help("remove");
        exit(1);
    }

    char *path = get_absolute_path(argv[2]);
    validate_path(path);
    
    char option;
    optind = 1;
    // 옵션 파싱 및 예외 처리
    while ((option = getopt(argc, argv, "dra")) != -1) {
        if (option == 'd') d_flag = true;
        else if (option == 'r') r_flag = true;
        else if (option == 'a') a_flag = true;
        else {
            print_help("remove");
            free(path);
            exit(1);
        }
    }
    
    struct stat statbuf;
    if (stat(path, &statbuf) < 0) {
        // 특이 예외 처리: 원본 파일/디렉토리가 이미 삭제되어 존재하지 않는(ENOENT) 경우
        if (errno == ENOENT) { 
            if (has_backup_record(path)) {
                // 원본 파일은 없지만 백업본 기록이 파일 단위로 존재할 때
                do_remove_file(path, a_flag);
            } else if (has_backup_record_in_dir(path)) {
                // 원본 경로는 없지만 해당 디렉토리 구조 아래에 백업본 기록이 존재할 때
                if (d_flag || r_flag) {
                    Logs *curr = head;
                    char **unique_origins = malloc(sizeof(char*) * 1000); 
                    int u_count = 0;

                    while (curr != NULL) {
                        if (is_in_dir(curr->origin, path, r_flag)) {
                            bool is_dup = false;
                            for (int i = 0; i < u_count; i++) {
                                if (strcmp(unique_origins[i], curr->origin) == 0) {
                                    is_dup = true;
                                    break;
                                }
                            }
                            if (!is_dup) {
                                unique_origins[u_count++] = curr->origin;
                            }
                        }
                        curr = curr->next;
                    }

                    if (u_count == 0) {
                        printf("\"%s\" is not backuped\n", path);
                    } else {
                        for (int i = 0; i < u_count; i++) {
                            do_remove_file(unique_origins[i], a_flag);
                        }
                    }
                    free(unique_origins);
                } else {
                    // 원본이 없는 디렉토리 구조인데 옵션(-d, -r)을 주지 않은 경우
                    fprintf(stderr, "input directory for -d, -r option\n");
                    free(path);
                    exit(1);
                }
            } else {
                printf("\"%s\" is not backuped\n", path);
            }
        } else {
            fprintf(stderr, "stat error for %s\n", path);
            free(path);
            exit(1);
        }
    } else { // 원본 파일/디렉토리가 존재하는 경우
        if (S_ISREG(statbuf.st_mode)) {
            // 예외 처리: 파일인데 디렉토리 전용 옵션을 사용한 경우
            if (d_flag || r_flag) {
                fprintf(stderr, "input directory for -d, -r option\n");
                free(path);
                exit(1);
            } else {
                do_remove_file(path, a_flag);
            }
        } else if (S_ISDIR(statbuf.st_mode)) {
            // 예외 처리: 디렉토리인데 옵션을 주지 않은 경우
            if (d_flag || r_flag) {
                do_remove_dir(path, a_flag, r_flag);
            } else {
                fprintf(stderr, "input directory without -d, -r option\n");
                free(path);
                exit(1);
            }
        }
    }

    // 작업 완료 후 /home/backup 디렉토리 내부의 빈 폴더들을 재귀적으로 정리
    remove_empty_dirs_recursive(backupdir);
    free(path);
}
