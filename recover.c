#include "backup.h"

/*
 * 특정 원본 파일의 백업본을 찾아 복구 작업을 수행한다.
 * char *path: 원본 파일의 절대 경로
 * char *new_path: 복구될 새로운 경로 (NULL인 경우 원본 경로로 복구)
 */
void do_recover_file(char *path, char *new_path) {
    int count = 0;
    Logs *curr = head;
    char target_path[MAX_PATHLENGTH + 1];

    // 해당 경로의 백업본 개수 확인
    while (curr != NULL) {
        if (strcmp(curr->origin, path) == 0) count++;
        curr = curr->next;
    }

    // 예외 처리: 백업 기록이 전혀 없는 경우
    if (count == 0) {
        printf("\"%s\" is not backuped\n", path);
        return;
    }

    // 백업 노드들을 포인터 배열에 저장
    Logs **data = (Logs **)malloc(sizeof(Logs *) * count);
    int idx = 0;
    curr = head;
    while (curr != NULL) {
        if (strcmp(curr->origin, path) == 0) {
            data[idx++] = curr;
        }
        curr = curr->next;
    }

    Logs *target_node = NULL;
    // 복구 대상 선택
    if (count > 1) {
        // 백업본이 여러 개인 경우 선택 창 출력
        printf("backup files of %s\n", path);
        printf("0. exit\n");
        for (int i = 0; i < count; i++) {
            int filesize = get_filesize(data[i]->backup);
            printf("%d. %s \t %dbytes\n", i + 1, data[i]->time, filesize);
        }
        printf(">> ");
        int input;
        
        // 예외 처리: 정수가 아닌 잘못된 입력
        if (scanf("%d", &input) != 1) {
            fprintf(stderr, "invalid input\n");
            free(data);
            exit(1);
        }
        while (getchar() != '\n'); // 입력 버퍼 비우기

        if (input == 0) {
            free(data);
            return;
        } else if (input > 0 && input <= count) {
            target_node = data[input - 1];
        } else {
            // 예외 처리: 리스트 범위를 벗어난 번호 입력 시
            fprintf(stderr, "invalid input\n");
            free(data);
            exit(1);
        }
    } else {
        // 백업본이 하나뿐인 경우
        target_node = data[0];
    }

    // 복구될 최종 경로 결정
    if (new_path != NULL) {
        // 예외 처리: -n 옵션 사용 시, 지정된 경로가 디렉토리면 원본 파일명을 붙여준다.
        struct stat st;
        if (stat(new_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            char *fname = strrchr(path, '/') + 1;
            snprintf(target_path, sizeof(target_path), "%s/%s", new_path, fname);
        } else {
            strncpy(target_path, new_path, MAX_PATHLENGTH);
        }
    } else {
        // 일반 복구의 경우 원본 경로를 그대로 사용
        strncpy(target_path, path, MAX_PATHLENGTH);
    }

    // 실제 파일 복사 작업 (백업본 -> 복구 위치)
    int fd_bak, fd_res;
    if ((fd_bak = open(target_node->backup, O_RDONLY)) < 0) {
        fprintf(stderr, "open error for backup file %s\n", target_node->backup);
        free(data);
        return;
    }

    char *dir_part = extract_dir_path(target_path);
    if (access(dir_part, F_OK) != 0) {
        make_recursive_dir(dir_part);
    }
    free(dir_part);

    // 예외 처리: 복구 위치에 파일 생성 실패 시
    if ((fd_res = open(target_path, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
        fprintf(stderr, "create error for recovery path %s\n", target_path);
        close(fd_bak);
        free(data);
        return;
    }

    int length;
    char buf[MAX_BUFSIZE];
    while ((length = read(fd_bak, buf, MAX_BUFSIZE)) > 0) {
        if (write(fd_res, buf, length) < 0)
		    fprintf(stderr, "unable to write log");
    }

    close(fd_bak);
    close(fd_res);

    // 작업 결과 로깅 및 출력
    char *curtime = get_current_time();
    log_write(curtime, target_node->backup, target_path, "recovered to");
    printf("\"%s\" recovered to \"%s\"\n", target_node->backup, target_path);

    free(curtime);
    free(data); // 동적 할당 배열 해제
}

/*
 * 디렉토리 하위를 탐색하며 조건에 맞는 파일들을 복구한다.
 * char *path: 복구할 대상 원본 디렉토리 경로
 * char *new_path: 복구될 새로운 기본 디렉토리 경로
 * bool r_flag: 하위 디렉토리 재귀 복구 여부 플래그
 */
void do_recover_dir(char *path, char *new_path, bool r_flag) {
    Logs *curr = head;
    
    char clean_path[MAX_PATHLENGTH + 1];
    strncpy(clean_path, path, MAX_PATHLENGTH);
    int path_len = strlen(clean_path);
    // 맨 끝의 / 제거
    if (clean_path[path_len - 1] == '/') {
        clean_path[path_len - 1] = '\0';
        path_len--;
    }

    // 새로운 경로가 주어진 경우 최상위 디렉토리 생성(시도)
    if (new_path != NULL && access(new_path, F_OK) != 0) {
        make_recursive_dir(new_path);
    }

    // 백업 노드를 순회하며 복구 대상 검색
    while (curr != NULL) {
        // origin 경로가 복구 대상 디렉토리(clean_path) 하위에 존재하는지 확인
        if (strncmp(curr->origin, clean_path, path_len) == 0 && curr->origin[path_len] == '/') {
            char *relative_path = curr->origin + path_len; 
            
            // -r 플래그가 없으면 직속 파일만 복구 (상대 경로에 '/'가 또 있으면 하위 폴더임)
            if (!r_flag && strchr(relative_path + 1, '/') != NULL) {
                curr = curr->next;
                continue;
            }

            // 동일한 파일의 백업본이 여러 개일 수 있으므로, 이미 처리한 원본 경로인지 확인
            Logs *tmp = head;
            bool already_processed = false;
            while (tmp != curr) {
                if (strcmp(tmp->origin, curr->origin) == 0) {
                    already_processed = true;
                    break;
                }
                tmp = tmp->next;
            }

            // 처음 발견한 원본 파일이라면 do_recover_file 호출 (버전 선택은 저기서 알아서 함)
            if (!already_processed) {
                if (new_path != NULL) {
                    char tmp_new[MAX_PATHLENGTH];
                    // 새로운 베이스 경로에 파일의 상대 경로를 결합
                    snprintf(tmp_new, sizeof(tmp_new), "%s%s", new_path, relative_path);
                    do_recover_file(curr->origin, tmp_new);
                } else {
                    do_recover_file(curr->origin, NULL);
                }
            }
        }
        curr = curr->next;
    }
}

/*
 * 사용자의 recover 명령을 처리한다. 옵션과 경로를 파싱하여 알맞은 복구 함수를 호출한다.
 * int argc: 프로그램 실행 시 입력된 인자의 개수
 * char *argv[]: 프로그램 실행 시 입력된 인자 배열
 */
void cmd_recover(int argc, char *argv[]) {
    bool d_flag = false;
    bool r_flag = false;
    char *new_path = NULL;

    // 예외 처리: 복구 대상 경로 인자 누락 시 도움말 출력
    if (argv[2] == NULL) {
        print_help("recover");
        exit(1);
    }

    char *path = get_absolute_path(argv[2]);
    validate_path(path);

    int option;
    optind = 1;
    // 옵션 파싱 및 예외 처리
    while ((option = getopt(argc, argv, "drn:")) != -1) {
        switch (option) {
            case 'd': d_flag = true; break;
            case 'r': r_flag = true; break;
            case 'n':
                // -n 옵션은 인자(optarg)가 필수임
                new_path = get_absolute_path(optarg);
                break;
            default:
                print_help("recover");
                free(path);
                exit(1);
        }
    }

    struct stat st;
    int stat_res = stat(path, &st);

    // stat 에러 (ENOENT 제외) 
    if (stat_res < 0 && errno != ENOENT) {
        fprintf(stderr, "stat error for %s\n", path);
        if (new_path != NULL) free(new_path);
        free(path);
        return;
    }

    // 원본 파일/디렉토리가 존재하지 않는 경우 (삭제된 상태에서 복구)
    if (stat_res < 0) {
        if (has_backup_record(path)) {	// 백업 레코드가 있는 경우
            do_recover_file(path, new_path);
        } else if (has_backup_record_in_dir(path)) {
            
            // 하위 디렉토리에 백업 레코드가 있으면서 -d 또는 -r 옵션이 입력된 경우
            if (d_flag || r_flag) 
                do_recover_dir(path, new_path, r_flag);
           
            else fprintf(stderr, "directory recovery needs -d or -r option\n");
        } else {
            printf("\"%s\" is not backuped\n", path);
        }

        if (new_path != NULL) free(new_path);
        free(path);
        return;
    }

    // 원본이 일반 파일인 경우
    if (S_ISREG(st.st_mode)) {
        if (d_flag || r_flag) fprintf(stderr, "cannot use -d, -r on regular file\n");
        else do_recover_file(path, new_path);

        if (new_path != NULL) free(new_path);
        free(path);
        return;
    }

    // 원본이 디렉토리인 경우
    if (S_ISDIR(st.st_mode)) {
        if (d_flag || r_flag) do_recover_dir(path, new_path, r_flag);
        else fprintf(stderr, "directory recovery needs -d or -r option\n");

        if (new_path != NULL) free(new_path);
        free(path);
        return;
    }

    // 기타 예외 케이스 처리 후 자원 해제 
    if (new_path != NULL) free(new_path);
    free(path);
}
