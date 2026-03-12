#include "backup.h"

/*
 * 프로그램의 진입점. 초기화 함수들을 호출하고, 사용자가 입력한 명령어를 파싱하여 
 * 각 기능(backup, remove, recover, list, help)을 수행하는 서브 모듈로 분기한다.
 * int argc: 프로그램 실행 시 입력된 인자의 개수
 * char *argv[]: 프로그램 실행 시 입력된 인자 배열
 * return : int, 프로그램 종료 상태 코드 (정상 종료 시 0)
 */
int main(int argc, char *argv[]) {
    int command;

    // 예외 처리 : 명령어 인자가 충분하지 않은 경우
    if (argc < 2) {
        fprintf(stderr, "Usage: ./backup <command> [options]\n");
        fprintf(stderr, "Type './backup help' for more information.\n");
        exit(1);
    }
    // 명령어 확인 및 정수 코드로 변환
    if ((command = parse_command(argv[1])) < 0) {
        fprintf(stderr, "Invalid command: %s\n", argv[1]);
        exit(1);
    }
    if (command == 5) {
        // help 명령어 우선 처리
        print_help(argv[2]);
		exit(0);
    }
    // 사용자의 홈 디렉토리 경로 설정
    init_homedir();

    // 백업 디렉토리 및 로그 파일(backup.log) 존재 여부 확인 및 초기화 (링크드 리스트 구성)
    init_system();



    // 명령어 분기 처리
    switch (command) {
        case 1:
            cmd_backup(argc, argv);
            break;
        case 2:
            cmd_remove(argc, argv);
            break;
        case 3:
            cmd_recover(argc, argv);
            break;
        case 4:
            cmd_list(argc, argv);
            break;
        default:
            fprintf(stderr, "Unknown command code.\n");
            exit(1);
    }
    exit(0);
}
