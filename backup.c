#include "backup.h"

/*
 * 단일 파일을 백업 디렉토리에 복사하고 로그를 남긴다.
 * char *path: 백업할 원본 파일의 절대 경로
 * char *base_path: 디렉토리 재귀 백업 시 기준이 되는 최상위 경로 (단일 파일 백업 시 원본 경로와 동일)
 * bool r_flag: 하위 디렉토리 구조 유지 여부 플래그
 * bool y_flag: 덮어쓰기(강제 백업) 허용 여부 플래그
 */
void do_backup_file(char *path, char *base_path, bool r_flag, bool y_flag) {
    int fd_ori, fd_bak;
    Logs *tmp;
    char backuppath[MAX_PATHLENGTH * 2];
    char *filename = strrchr(path, '/') + 1;
    
    // 예외 처리: -y 옵션이 없을 때 이미 백업된 파일인지 해시로 무결성 검사
    if (!y_flag) {
        if ((tmp = find_backup_node(path)) != NULL) {
            printf("\"%s\" already backuped to \"%s\"\n", path, tmp->backup);
            return;
        }
    }

    char *curtime = get_current_time();
    
    char time_dir[MAX_PATHLENGTH];
    snprintf(time_dir, sizeof(time_dir), "%s/%s", backupdir, curtime);
    
    // 예외 처리: 타임스탬프 디렉토리가 없으면 생성
    if (access(time_dir, R_OK | W_OK) != 0) {
        mkdir(time_dir, 0777);
    }

    // -r 옵션이 있고, 원본 파일이 특정 디렉토리 하위에 위치하는 경우
    // 백업 디렉토리 내부에도 동일한 하위 폴더 트리를 생성한다.
    if (r_flag && base_path != NULL && strlen(path) > strlen(base_path)) {
        char *relative_path = path + strlen(base_path);
        if (relative_path[0] == '/') relative_path++; 

        char tmp_rel[MAX_PATHLENGTH];
        strcpy(tmp_rel, relative_path);
        
        char *slash_ptr = strchr(tmp_rel, '/');
        while (slash_ptr != NULL) {
            *slash_ptr = '\0'; 
            
            char target_dir[MAX_PATHLENGTH * 2];
            snprintf(target_dir, sizeof(target_dir), "%s/%s", time_dir, tmp_rel);
            
            // 예외 처리: 하위 경로상의 디렉토리가 없으면 순차적으로 생성
            if (access(target_dir, R_OK | W_OK) != 0) {
                mkdir(target_dir, 0777);
            }
            
            *slash_ptr = '/'; 
            slash_ptr = strchr(slash_ptr + 1, '/');
        }
        snprintf(backuppath, sizeof(backuppath), "%s/%s", time_dir, relative_path);
    } else {
        snprintf(backuppath, sizeof(backuppath), "%s/%s", time_dir, filename);
    }

    // 예외 처리: 원본 파일 열기 실패
    if ((fd_ori = open(path, O_RDONLY)) < 0) {
        fprintf(stderr, "open error for %s\n", path);
        free(curtime);
        exit(1);
    }
    
    // 예외 처리: 백업 파일 생성 실패
    if ((fd_bak = open(backuppath, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
        fprintf(stderr, "create error for %s\n", backuppath);
        close(fd_ori);
        free(curtime);
        exit(1);
    }

    int length;
    char buf[MAX_BUFSIZE];
    while ((length = read(fd_ori, buf, MAX_BUFSIZE)) > 0) {
        if (write(fd_bak, buf, length) < 0)
		    fprintf(stderr, "Unable to write log");
    }

    close(fd_ori);
    close(fd_bak);

    log_write(curtime, path, backuppath, "backuped to");
    printf("\"%s\" backuped to \"%s\"\n", path, backuppath);

    free(curtime); 
}

/*
 * 디렉토리 내부를 탐색하며 조건에 맞는 파일들을 백업한다.
 * char *path: 탐색할 디렉토리 경로
 * char *base_path: 디렉토리 재귀 백업 시 기준이 되는 최상위 디렉토리 경로
 * bool d_flag: 디렉토리 내부 파일 백업 플래그
 * bool r_flag: 하위 디렉토리 재귀 탐색 플래그
 * bool y_flag: 덮어쓰기 허용 여부 플래그
 */
void do_backup_dir(char *path, char *base_path, bool d_flag, bool r_flag, bool y_flag) {
    struct dirent **namelist;
    int count;
    
    // 예외 처리: 디렉토리 스캔 실패 시 종료
    if ((count = scandir(path, &namelist, NULL, alphasort)) < 0) {
        fprintf(stderr, "scandir error for %s\n", path);
        exit(1);
    }

    // 1차 순회: 현재 디렉토리 내부의 일반 파일들을 먼저 백업 처리
    for (int i = 0; i < count; i++) {
        if (strcmp(namelist[i]->d_name, ".") == 0 || strcmp(namelist[i]->d_name, "..") == 0) {
            continue;
        }
        
        char tmp[MAX_PATHLENGTH];
        snprintf(tmp, sizeof(tmp), "%s/%s", path, namelist[i]->d_name);
        
        struct stat statbuf;
        if (stat(tmp, &statbuf) < 0) continue;
        
        // 예외 처리: 파일 타입 검사 (일반 파일일 경우에만 처리)
        if (S_ISREG(statbuf.st_mode)) {
            do_backup_file(tmp, base_path, r_flag, y_flag);
        }
    }

    // 2차 순회 (재귀): -r 옵션 활성화 시 하위 디렉토리를 탐색
    if (r_flag) {
        for (int i = 0; i < count; i++) {
            if (strcmp(namelist[i]->d_name, ".") == 0 || strcmp(namelist[i]->d_name, "..") == 0) {
                continue;
            }
            
            char tmp[MAX_PATHLENGTH];
            snprintf(tmp, sizeof(tmp), "%s/%s", path, namelist[i]->d_name);
            
            struct stat statbuf;
            if (stat(tmp, &statbuf) < 0) continue;
            
            // 재귀 호출: 대상이 디렉토리인 경우 do_backup_dir를 다시 호출하여 더 깊이 탐색
            if (S_ISDIR(statbuf.st_mode)) {
                do_backup_dir(tmp, base_path, d_flag, r_flag, y_flag);
            }
        }
    }

    for (int i = 0; i < count; i++) {
        free(namelist[i]);
    }
    free(namelist);
}

/*
 * 사용자의 backup 명령을 처리한다. 옵션과 경로를 파싱하여 알맞은 백업 함수를 호출한다.
 * int argc: 프로그램 실행 시 입력된 인자의 개수
 * char **argv: 프로그램 실행 시 입력된 인자 배열
 */
void cmd_backup(int argc, char **argv) {
    bool d_flag = false;
    bool r_flag = false;
    bool y_flag = false;

    // 예외 처리: 백업할 대상 경로가 입력되지 않은 경우
    if (argv[2] == NULL) {
        print_help("backup");
        exit(1);
    }

    char *path = get_absolute_path(argv[2]);
    validate_path(path);
    
    char option;
    optind = 1; 
    // 옵션 파싱 및 예외 처리
    while ((option = getopt(argc, argv, "dry")) != -1) {
        if (option == 'd') d_flag = true;
        else if (option == 'r') r_flag = true;
        else if (option == 'y') y_flag = true;
        else {
            print_help("backup");
            free(path);
            exit(1);
        }
    }
    
    struct stat statbuf;
    if (stat(path, &statbuf) < 0) {
        // 예외 처리: 원본 파일 혹은 디렉토리가 존재하지 않는 경우
        if (errno == ENOENT) { 
            fprintf(stderr, "\"%s\" does not exist\n", path);
            free(path);
            exit(1);
        }
        fprintf(stderr, "stat error for %s\n", path);
        free(path);
        exit(1);
    }
    
    if (S_ISREG(statbuf.st_mode)) { 
        // 예외 처리: 단일 파일에 -d 또는 -r 옵션을 적용하려 한 경우 오류 발생
        if (d_flag || r_flag) { 
            fprintf(stderr, "-d or -r option needs directory not file\n");
            free(path);
            exit(1);
        }
        do_backup_file(path, path, r_flag, y_flag);
    } else if (S_ISDIR(statbuf.st_mode)) { 
        // 예외 처리: 디렉토리를 백업하려는데 -d 또는 -r 옵션이 없는 경우 오류 발생
        if (d_flag || r_flag) { 
            do_backup_dir(path, path, d_flag, r_flag, y_flag);
        } else {
            fprintf(stderr, "input directory with -d or -r option\n");
            free(path);
            exit(1);
        }
    } else {
        // 예외 처리: 파일도 디렉토리도 아닌 경우 (심볼릭 링크 등)
        fprintf(stderr, "not regular file\n");
        free(path);
        exit(1);
    }

    free(path); 
}
