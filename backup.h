#ifndef BACKUP_H
#define BACKUP_H

#define OPENSSL_API_COMPAT 0x10100000L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <openssl/md5.h>
#include <sys/wait.h>
#include <libgen.h>

#define MAX_FILENAMESIZE 255
#define MAX_PATHLENGTH 4096
#define MAX_BUFSIZE 4096 
#define MAX_STRLENGTH 9000

typedef struct node {
    char *time;
    char *origin;
    char *backup;
    struct node *next;
    struct node *prev;
} Logs;

// 전역 변수
extern Logs *head;
extern int log_fd;
extern char *homedir;
extern char *backupdir;
extern int tree_count;
extern char **paths;

// 시스템 초기화 및 명령어 유틸
void init_system(void);               
void init_homedir(void);              
int parse_command(char *command);     

// 해시 및 시간 유틸
char* get_current_time(void);                  
unsigned char* calc_md5_hash(char *fname);     
bool compare_files_by_hash(char *p1, char *p2);

// 경로 및 파일/디렉토리 제어 유틸
void validate_path(char* path);                
char* get_absolute_path(char* path);           
char* extract_dir_path(char *path);            
int get_filesize(char *path);
bool is_dir_empty(char *path);                 
void remove_empty_dirs_recursive(char *path); 
void make_recursive_dir(const char *path);

// 로그 및 링크드 리스트 제어
void log_add_node(char *time, char *origin, char *backup); 
void log_delete_node(Logs *target);                        
void log_write(char *time, char* one, char* two, char *cmd); 
Logs* find_backup_node(char *path);                        
bool has_backup_record(char *path);                        
bool has_backup_record_in_dir(char *path);                 

// 명령어별 메인 함수
void cmd_backup(int argc, char **argv);
void cmd_remove(int argc, char *argv[]);
void cmd_recover(int argc, char *argv[]);
void cmd_list(int argc, char *argv[]);
void print_help(char *input);

#endif // BACKUP_H
