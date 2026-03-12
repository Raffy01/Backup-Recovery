#include "backup.h"

// 전역 변수 실제 정의
Logs *head = NULL;
int log_fd;
char *homedir = NULL;
char *backupdir = "/home/backup";
int tree_count = 0;
char **paths = NULL;

/*
 * 현재 시간을 YYMMDDHHMMSS 형태의 문자열로 반환한다.
 * return : char*, 현재 시간이 담긴 동적 할당된 문자열
 */
char* get_current_time(void) {
    time_t timer = time(NULL);
    struct tm *t = localtime(&timer);
    char *time_str = (char *)malloc(sizeof(char) * 64); 
    // 예외 처리: 동적 할당 실패 시 에러 출력 후 종료
    if (time_str == NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    snprintf(time_str, 64, "%02d%02d%02d%02d%02d%02d", 
             t->tm_year % 100, t->tm_mon + 1, t->tm_mday, 
             t->tm_hour, t->tm_min, t->tm_sec);
    return time_str;
}

/*
 * 주어진 파일의 내용을 읽어 MD5 해시값을 계산하여 반환한다.
 * char *fname: 해시값을 구할 대상 파일의 경로
 * return : unsigned char*, 계산된 해시값이 담긴 동적 할당된 문자열. 파일 열기 실패 시 NULL 반환
 */
unsigned char* calc_md5_hash(char *fname) {
    int fd;
    MD5_CTX ctx;
    unsigned char *hash = NULL;
    unsigned char buf[1024]; 
    int length;

    // 예외 처리: 파일 열기 실패 시 해시 계산을 수행하지 않고 NULL 반환
    if ((fd = open(fname, O_RDONLY)) < 0) return NULL;
    
    hash = (unsigned char *)malloc(sizeof(unsigned char) * MD5_DIGEST_LENGTH);
    // 예외 처리: 동적 할당 실패 시 열어둔 파일 디스크립터를 닫고 NULL 반환
    if (!hash) {
        close(fd);
        return NULL;
    }

    MD5_Init(&ctx);
    while ((length = read(fd, buf, sizeof(buf))) > 0) {
        MD5_Update(&ctx, buf, length);
    }
    MD5_Final(hash, &ctx);
    close(fd);
    return hash;
}

/*
 * 현재 실행 중인 프로세스의 경로를 분석하여 전역 변수 homedir(/home/<username>)을 설정한다.
 */
void init_homedir(void) {
    char process_path[MAX_PATHLENGTH];
    ssize_t len = readlink("/proc/self/exe", process_path, sizeof(process_path) - 1);
    if (len != -1) process_path[len] = '\0';
    
    homedir = (char *)malloc(MAX_PATHLENGTH);
    
    char tmp_path[MAX_PATHLENGTH];
    strcpy(tmp_path, process_path);
    char *token;
    
    strcpy(homedir, "/"); 
    token = strtok(tmp_path, "/");
    if (token) {
        strcat(homedir, token);
        strcat(homedir, "/");
        token = strtok(NULL, "/");
        if (token) strcat(homedir, token);
    }
}

/*
 * 새로운 백업 로그 노드를 생성하여 전역 링크드 리스트(head)의 끝에 추가한다.
 * char *time: 백업된 시간
 * char *origin: 원본 파일 경로
 * char *backup: 백업된 파일 경로
 */
void log_add_node(char *time, char *origin, char *backup) {
    Logs *tmp = (Logs *)malloc(sizeof(Logs));
    tmp->time = strdup(time);
    tmp->origin = strdup(origin);
    tmp->backup = strdup(backup);
    tmp->next = NULL;
    tmp->prev = NULL;

    if (head == NULL) {
        head = tmp;
    } else {
        Logs *curr = head;
        while (curr->next != NULL) { 
            curr = curr->next;
        }
        curr->next = tmp;
        tmp->prev = curr;
    }
}

/*
 * 링크드 리스트에서 특정 노드를 찾아 연결을 끊고 메모리를 해제한다.
 * Logs *target: 삭제할 대상 노드 포인터
 */
void log_delete_node(Logs *target) {
    if (head == NULL || target == NULL) return;

    if (head == target) head = target->next;
    if (target->next != NULL) target->next->prev = target->prev;
    if (target->prev != NULL) target->prev->next = target->next;
    
    free(target->time);
    free(target->origin);
    free(target->backup);
    free(target);
}

/*
 * backup.log 파일을 읽어 입력된 백업 파일 경로와 일치하는 기록을 찾고 링크드 리스트에 추가한다.
 * char *path: 백업 디렉토리 내에 존재하는 파일의 절대 경로
 */
void log_add_node_from_file(char *path) {
    char charactor;
    char str[MAX_PATHLENGTH];
    char tmp[MAX_PATHLENGTH];
    
    strcpy(tmp, path + strlen(backupdir) + 1);
    char *log_time = strtok(tmp, "/");
    lseek(log_fd, 0, SEEK_SET); 
    
    int i = 0;
    while (read(log_fd, &charactor, 1) > 0) {
        str[i++] = charactor;
        if (charactor == '\n') {
            str[i - 1] = '\0';
            
            char *time = strtok(str, " :"); 
            if (time) {
                char *origin = strtok(NULL, "\"");
                if (origin) origin = strtok(NULL, "\""); 
                strtok(NULL, "\"");
                char *backup = strtok(NULL, "\"");
                
                // 예외 처리: 토큰화된 문자열이 정상적으로 존재하고, 조건에 완벽히 일치할 때만 리스트에 추가
                if (origin && backup && strcmp(path, backup) == 0 && strcmp(log_time, time) == 0) {
                    log_add_node(time, origin, backup);
                }
            }
            i = 0;
        }
    }
}

/*
 * 백업 디렉토리를 재귀적으로 탐색하며 존재하는 파일들에 대해 log_add_node_from_file를 호출하여 리스트를 초기화한다.
 * char *tmp: 탐색할 대상 디렉토리 경로
 */
void log_read_all(char *tmp) {
    struct dirent **namelist;
    int count;
    char path[MAX_PATHLENGTH];
    
    snprintf(path, sizeof(path), "%s/", tmp); 
    // 예외 처리: scandir 실패 시 에러 출력 후 프로그램 종료
    if ((count = scandir(path, &namelist, NULL, alphasort)) < 0) {
        fprintf(stderr, "scandir error for %s\n", path);
        exit(1);
    }
    
    for (int i = 0; i < count; i++) {
        // 예외 처리: 현재 디렉토리와 상위 디렉토리 무시
        if (strcmp(".", namelist[i]->d_name) == 0 || strcmp("..", namelist[i]->d_name) == 0) {
            free(namelist[i]);
            continue;
        }
        
        char file[MAX_PATHLENGTH + 512];
        struct stat statbuf;
        snprintf(file, sizeof(file), "%s%s", path, namelist[i]->d_name);
        
        char log_path[MAX_PATHLENGTH];
        snprintf(log_path, sizeof(log_path), "%s/backup.log", backupdir);
        
        // 예외 처리: 자기 자신인 backup.log 파일은 탐색에서 제외
        if (strcmp(file, log_path) == 0) {
            free(namelist[i]);
            continue;
        }

        // 예외 처리: 파일 상태를 읽어올 수 없을 경우 해당 파일 스킵 및 에러 처리
        if (stat(file, &statbuf) < 0) {
            fprintf(stderr, "stat error for %s\n", file);
            exit(1);
        }

        if (S_ISREG(statbuf.st_mode)) {
            log_add_node_from_file(file);
        } else if (S_ISDIR(statbuf.st_mode)) {
            // 재귀 호출: 대상이 디렉토리일 경우 해당 하위 디렉토리를 재귀적으로 계속 탐색
            log_read_all(file);
        }
        free(namelist[i]); 
    }
    free(namelist); 
}

/*
 * 백업 디렉토리와 로그 파일(backup.log)의 존재 여부를 확인하고 생성한 뒤, 로그 기록을 읽어들인다.
 */
void init_system(void) {
    // 예외 처리: 백업 디렉토리가 없으면 기본 권한(0777)으로 새로 생성
    if (access(backupdir, R_OK | W_OK) != 0) 
        mkdir(backupdir, 0777); 

    char tmp[MAX_PATHLENGTH];
    snprintf(tmp, sizeof(tmp), "%s/backup.log", backupdir);
    // 예외 처리: 로그 파일을 열거나 생성하는 데 실패한 경우 에러 출력 후 종료
    if ((log_fd = open(tmp, O_RDWR | O_CREAT, 0666)) < 0) { 
        fprintf(stderr, "open error for backup.log\n");
        exit(1);
    }
    log_read_all(backupdir);
}

/*
 * 문자열로 입력된 명령어를 식별하여 정수 코드로 변환한다.
 * char *command: 입력된 내장 명령어 문자열
 * return : int, 명령어에 매칭되는 정수 (backup: 1, remove: 2, recover: 3, list: 4, help: 5). 매칭 실패 시 -1 반환
 */
int parse_command(char *command) {
    if (strcmp(command, "backup") == 0) return 1;
    if (strcmp(command, "remove") == 0) return 2;
    if (strcmp(command, "recover") == 0) return 3;
    if (strcmp(command, "list") == 0) return 4;
    if (strcmp(command, "help") == 0) return 5;
    return -1;
}

/*
 * 입력된 경로가 사용자의 홈 디렉토리 내에 위치하는지, 백업 디렉토리가 아닌지 유효성을 검사한다.
 * char *path: 검사할 파일 또는 디렉토리 경로
 */
void validate_path(char* path) {
    if (path == NULL) return;

    // 예외 처리: 홈 디렉토리 외부의 파일이거나, 이미 백업된 디렉토리 내부의 파일인 경우 에러 처리
    if (strncmp(path, homedir, strlen(homedir)) != 0 || 
        strncmp(path, backupdir, strlen(backupdir)) == 0) {
        fprintf(stderr, "\"%s\" is not in user directory\n", path);
        exit(1);
    }
}

/*
 * 상대경로나 잘못된 경로를 파싱하여 절대경로로 변환한다. 파일이 없더라도 유효한 포맷이면 절대경로를 유추한다.
 * char *path: 입력받은 경로 문자열
 * return : char*, 변환된 절대경로가 담긴 동적 할당된 문자열. 실패 시 프로그램 종료 혹은 NULL 반환
 */
char* get_absolute_path(char* path) {
    char *buf = malloc(MAX_PATHLENGTH); 
    char tmp[MAX_PATHLENGTH]; 

    // 예외 처리: realpath 함수가 정상적으로 경로를 찾지 못한 경우 (파일이 없거나 경로가 잘못된 경우)
    if (realpath(path, buf) == NULL) { 
        int err = errno;

        if (err == EINVAL) { 
            free(buf);
            return NULL;
        }
        // 예외 처리: 파일이 존재하지 않더라도, 삭제(-remove) 등에서 경로를 유추해야 하는 경우 홈 디렉토리 기준으로 수동 조합
        if (err == ENOENT) { 
            if (strncmp(path, "/home/", 6) == 0) {
                free(buf);
                return strdup(path); 
            }
            if (strncmp(path, "/", 1) == 0) {
                if (strncmp(path, "/home/", 6) != 0) {
                    fprintf(stderr, "\"%s\" is not in user directory\n", path);
                    free(buf);
                    exit(1);
                }
            }
            if (strncmp(path, "./", 2) == 0) strcpy(tmp, path + 2); 
            else strcpy(tmp, path);

            snprintf(buf, MAX_PATHLENGTH, "%s/%s", homedir, tmp);
            return buf;
        }
        // 예외 처리: 시스템 최대 허용 경로 길이를 초과했을 경우
        if (err == ENAMETOOLONG) { 
            fprintf(stderr, "Too long path. path must be under 4096 bytes\n");
            free(buf);
            exit(1);
        }
        fprintf(stderr, "realpath error %d\n", err);
        free(buf);
        exit(1);
    }
    return buf;
}

/*
 * 전체 경로에서 홈 디렉토리 부분과 파일명을 제외한 중간 디렉토리 경로들을 추출한다.
 * char *path: 원본 파일의 절대 경로
 * return : char*, 추출된 디렉토리 경로 문자열
 */
char *extract_dir_path(char *path) {
    char tmp[MAX_PATHLENGTH + 1];
    strncpy(tmp, path, MAX_PATHLENGTH);
    
    char *dname = dirname(tmp); // 경로에서 디렉토리 부분만 추출
    char *dir = (char *)malloc(MAX_PATHLENGTH);
    strcpy(dir, dname);
    
    return dir;
}

/*
 * 수행된 백업, 복구, 삭제 명령에 대한 결과를 backup.log 파일에 기록한다.
 * char *time: 작업 수행 시간
 * char *one: 대상 경로 1
 * char *two: 대상 경로 2
 * char *command: 명령어 동작을 설명하는 문자열
 */
void log_write(char *time, char* one, char* two, char *command) {
    char buf[MAX_STRLENGTH];
    snprintf(buf, sizeof(buf), "%s : \"%s\" %s \"%s\"\n", time, one, command, two);
    lseek(log_fd, 0, SEEK_END);
    if (write(log_fd, buf, strlen(buf)) < 0)
		fprintf(stderr, "Unable to write log\n");
    fsync(log_fd);
}

/*
 * 두 파일의 MD5 해시값을 계산하고 비교하여 내용이 동일한지 판단한다.
 * char *path1: 비교할 첫 번째 파일 경로
 * char *path2: 비교할 두 번째 파일 경로
 * return : bool, 두 파일의 해시가 일치하면 true, 다르면 false (파일이 존재하지 않아도 false 반환)
 */
bool compare_files_by_hash(char *path1, char *path2) {
    unsigned char *hash1 = calc_md5_hash(path1);
    unsigned char *hash2 = calc_md5_hash(path2);
    
    // 예외 처리: 어느 한 쪽이라도 파일 해시 계산에 실패(파일 열기 실패 등)하면 내용이 다른 것으로 간주
    if (hash1 == NULL || hash2 == NULL) { 
        free(hash1); free(hash2);
        return false;
    }

    bool flag = true;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (hash1[i] != hash2[i]) {
            flag = false;
            break;
        }
    }
    free(hash1);
    free(hash2); 
    return flag; 
}

/*
 * 링크드 리스트를 탐색하여 입력된 원본 경로와 백업본 내용(해시 기준)이 동일한 노드를 찾는다.
 * char *path: 탐색할 원본 파일 경로
 * return : Logs*, 조건에 일치하는 로그 노드 포인터. 찾지 못하면 NULL 반환
 */
Logs* find_backup_node(char *path) {
    Logs *tmp = head;
    while (tmp != NULL) { 
        if (strcmp(tmp->origin, path) == 0) {
            if (compare_files_by_hash(path, tmp->backup)) return tmp;
        }
        tmp = tmp->next;
    }
    return NULL;
}

/*
 * 주어진 파일의 크기를 바이트 단위로 계산하여 반환한다.
 * char *path: 크기를 측정할 파일 경로
 * return : int, 파일의 크기
 */
int get_filesize(char *path) {
    int fd;
    // 예외 처리: 파일 크기 측정을 위해 열 때 실패하면 에러 출력 후 종료
    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "open error for %s\n", path);
        exit(1);
    }
    int filesize = lseek(fd, 0, SEEK_END);
    close(fd); 
    return filesize;
}

/*
 * 주어진 디렉토리가 비어있는지(내부에 '.'와 '..'만 존재하는지) 확인한다.
 * char *path: 확인할 대상 디렉토리 경로
 * return : bool, 디렉토리가 비어있으면 true, 파일/디렉토리가 존재하면 false
 */
bool is_dir_empty(char *path) {
    struct dirent **namelist;
    int count;
    // 예외 처리: 디렉토리를 스캔할 수 없으면 접근 권한 등의 문제로 간주하고 에러 처리
    if ((count = scandir(path, &namelist, NULL, alphasort)) < 0) {
        fprintf(stderr, "scandir error for %s\n", path);
        exit(1);
    }
    for (int i = 0; i < count; i++) free(namelist[i]);
    free(namelist);
    return count == 2;
}

/*
 * 특정 파일이 과거에 백업된 기록이 링크드 리스트에 존재하는지 검사한다.
 * char *path: 검사할 대상 원본 파일 경로
 * return : bool, 백업 기록이 존재하면 true, 없으면 false
 */
bool has_backup_record(char *path) {
    Logs *tmp = head;
    while (tmp != NULL) {
        if (strcmp(tmp->origin, path) == 0) return true;
        tmp = tmp->next;
    }
    return false;
}

/*
 * 특정 디렉토리 하위에 백업된 파일 기록이 존재하는지 검사한다.
 * char *path: 검사할 대상 디렉토리 경로
 * return : bool, 하위에 백업 기록이 존재하면 true, 없으면 false
 */
bool has_backup_record_in_dir(char *path) {
    Logs *tmp = head;
    while (tmp != NULL) {
        if (strncmp(tmp->origin, path, strlen(path)) == 0) return true;
        tmp = tmp->next;
    }
    return false;
}

/*
 * 특정 디렉토리 하위를 탐색하며 내부에 파일이 없는 비어있는 디렉토리를 찾아 재귀적으로 삭제한다.
 * char *path: 탐색을 시작할 대상 디렉토리 경로
 */
void remove_empty_dirs_recursive(char *path) {
    if (path == NULL) return;
    struct dirent **namelist;
    int count;
    char tmp[MAX_PATHLENGTH];

    // 예외 처리: 스캔 에러 발생 시 진행 중단
    if ((count = scandir(path, &namelist, NULL, alphasort)) < 0) return;
    
    // 베이스 케이스: 디렉토리 내부에 '.'와 '..'만 존재한다면 해당 디렉토리 즉시 삭제 후 종료
    if (count == 2) {
        rmdir(path);
        for(int i = 0; i < count; i++) free(namelist[i]);
        free(namelist);
        return;
    }
    
    struct stat statbuf;
    // 하위 디렉토리 순회 및 재귀 처리
    for (int i = 0; i < count; i++) {
        if (strcmp(namelist[i]->d_name, ".") == 0 || strcmp(namelist[i]->d_name, "..") == 0) {
            free(namelist[i]);
            continue;
        }
        snprintf(tmp, sizeof(tmp), "%s/%s", path, namelist[i]->d_name);
        
        // 재귀 호출: 하위 항목이 디렉토리인 경우 더 깊이 들어가서 비우기를 시도
        if (stat(tmp, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            remove_empty_dirs_recursive(tmp);
        }
        free(namelist[i]);
    }
    free(namelist);

    // 재귀 후 체크: 하위 디렉토리를 모두 지우고 난 뒤 현재 디렉토리가 비게 되었다면 삭제
    if ((count = scandir(path, &namelist, NULL, alphasort)) >= 0) {
        if (count == 2) rmdir(path);
        for(int i = 0; i < count; i++) free(namelist[i]);
        free(namelist);
    }
}
/*
 * 입력된 경로의 모든 디렉토리를 재귀적으로 생성한다.
 * const char *path : 특정 파일까지의 경로
 */
void make_recursive_dir(const char *path) {
    char tmp[MAX_PATHLENGTH];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

    // 경로를 하나씩 쪼개며 디렉토리 생성
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (access(tmp, F_OK) != 0) {
                mkdir(tmp, 0777);
            }
            *p = '/';
        }
    }
    // 마지막 폴더 생성
    if (access(tmp, F_OK) != 0) {
        mkdir(tmp, 0777);
    }
}
