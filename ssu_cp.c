#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <utime.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

//옵션처리 변수
int opt_s, opt_i, opt_l, opt_n, opt_p, opt_r, opt_d;
int child_num;
int directory;

void checkopt(void);
void print_time(struct tm time);
void print_usage(void);
void writefile(char *source, char *target);
void writedir(char *source, char *target);
void run(char *source, char *target);

//옵션 에러를 체크하는 함수
void checkopt(void)
{
    //i, n 옵션을 같이 사용할 수 없음
    if (opt_i && opt_n) {
        fprintf(stderr, "-i, -n can not be used together\n");
        print_usage();
        exit(1);
    }
    
    //s옵션은 단독으로 쓰임
    if (opt_s) {
        if (opt_i || opt_l || opt_n || opt_p || opt_r || opt_d) {
            fprintf(stderr, "-s should be used alone\n");
            print_usage();
            exit(1);
        }
    }

}

//시간 구조체를 읽어 출력하는 함수
void print_time(struct tm time)
{
    printf("%04d.%02d.%02d ", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
    printf("%02d:%02d:%02d\n", time.tm_hour, time.tm_min, time.tm_sec);
}

//ssu_cp의 사용법을 출력하는 함수
void print_usage(void)
{
    printf("usage : in case of file\n");
    printf(" cp [-i/n][-l][-p]  [source][target]\n");
    printf(" or cp [-s][source][target]\n");
    printf(" in case of directory cp [-i/n][-l][-p][-r]-d][N]\n");
}

//source파일을 target파일에 복사하는 함수
void writefile(char *source, char *target)
{
    int fd_s, fd_t;
    int len;
    char buf[1024];
    struct stat statbuf;
    struct stat lstatbuf;
    struct utimbuf time;

    if (stat(source, &statbuf) < 0) {
        fprintf(stderr, "stat error for %s\n", source);
        print_usage();
        exit(1);
    }

    if (lstat(source, &lstatbuf) < 0) {
        fprintf(stderr, "lstat error for %s\n", source);
        print_usage();
        exit(1);
    }
    
    //target을 생성/오픈 source의 권한을 그대로 옮겨옴
    if ((fd_t = open(target, O_WRONLY|O_CREAT|O_TRUNC|statbuf.st_mode & 0777)) < 0) {
        fprintf(stderr, "open error for %s\n", target);
        print_usage();
        exit(1);
    }
    
    //링크파일일때 원본파일으로 복사하기 위해 readlink함수를 사용
    //그렇지 않을 때 source파일을 오픈하고 데이터를 읽어 target에 저장
    if (S_ISLNK(lstatbuf.st_mode)) {
        char namebuf[512];
        if ((len = readlink(source, namebuf, sizeof(namebuf)) < 0)) {
            fprintf(stderr, "readlink error for %s\n", source);
            print_usage();
            exit(1);
        }
        
        if ((fd_s = open(namebuf, O_RDONLY)) < 0) {
            fprintf(stderr, "open error for %s\n", source);
            print_usage();
            exit(1);
        }
    }
    else {
        if ((fd_s = open(source, O_RDONLY)) < 0) {
            fprintf(stderr, "open error for %s\n", source);
            print_usage();
            exit(1);
        }
    }

    while ((len = read(fd_s, buf, sizeof(buf)) > 0)) {
        write(fd_t, buf, len);
    }

    
    //l옵션이 활성화 되었을때
    if (opt_l) {
        time.actime = statbuf.st_atime;
        time.modtime = statbuf.st_mtime;
        
        //권한
        if (chmod(target, statbuf.st_mode & 0777) < 0) {
            fprintf(stderr, "chmod error for %s\n", target);
            print_usage();
            exit(1);
        }
        
        //유저 그룹 아이디
        if (chown(target, statbuf.st_uid, statbuf.st_gid) < 0) {
            fprintf(stderr, "chown error for %s\n", target);
            print_usage();
            exit(1);
        }
        
        //접근 수정시간
        if (utime(target, &time) < 0) {
            fprintf(stderr, "utime error for %s\n", target);
            print_usage();
            exit(1);
        }
    }
}

//디렉토리 복사함수
void writedir(char *source, char *target)
{
    struct dirent **namelist;
    struct stat statbuf;
    int count;
    int i;
    char s_path[PATH_MAX];
    char t_path[PATH_MAX];
    struct utimbuf time;
    int pid, status;

    printf("%s\n", source);

    if (stat(source, &statbuf) < 0) {
        fprintf(stderr, "stat error for %s\n", source);
        print_usage();
        exit(1);
    }
    
    //디렉토리가 이미 있는지 확인 없다면 디렉토리 생성
    if (access(target, F_OK) != 0) { 
        if (mkdir(target, statbuf.st_mode & 0777) < 0) {
            fprintf(stderr, "mkdir error for %s\n", target);
            print_usage();
            exit(1);
        }
    }
    
    if (stat(source, &statbuf) < 0) {
        fprintf(stderr, "stat error for %s\n", source);
        print_usage();
        exit(1);
    }
    
    //소스가 디렉토리가 아니면 에러처리
    if (!S_ISDIR(statbuf.st_mode)) {
        fprintf(stderr, "%s is not directory file.\n", source);
        print_usage();
        exit(1);
    }
    
    //l옵션 활성화
    if (opt_l) {
        //그룹 유저 아이디
        if (chown(target, statbuf.st_uid, statbuf.st_gid) < 0) {
            fprintf(stderr, "chown error for %s\n", target);
            print_usage();
            exit(1);
        }

        time.actime = statbuf.st_atime;
        time.modtime = statbuf.st_mtime;
        
        //접근 수정시간
        if (utime(target, &time) < 0) {
            fprintf(stderr, "utime error for %s\n", target);
            print_usage();
            exit(1);
        }
        
        //권한
        if (chmod(target, statbuf.st_mode & 0777) < 0) {
            fprintf(stderr, "chmod error for %s\n", target);
            print_usage();
            exit(1);
        }
    }
    
    //디렉토리 읽기
    if ((count = scandir(source, &namelist, NULL, alphasort)) == -1) {
        fprintf(stderr, "%s Directory Scanner Error\n", source);
        print_usage();
        exit(1);
    }

    for (i = 2 ; i < count ; i++) {
        //소스의 패스만들기
        memset(s_path, 0, sizeof(s_path));
        strcpy(s_path, source);
        strcat(s_path, "/");
        strcat(s_path, namelist[i]->d_name);
        //타겟의 패스만들기
        memset(t_path, 0, sizeof(t_path));
        strcpy(t_path, target);
        strcat(t_path, "/");
        strcat(t_path, namelist[i]->d_name);

        if ((stat(s_path, &statbuf)) < 0) {
            fprintf(stderr, "stat error for %s\n", s_path);
            print_usage();
            exit(1);
        }
        
        //파일이면 writefile으로 복사
        if (S_ISREG(statbuf.st_mode)) {
            writefile(s_path, t_path);
        }
        
        //디렉토리면 재귀호출로 디렉토리복사
        if (S_ISDIR(statbuf.st_mode)) {
            writedir(s_path, t_path);
        }
    }

    for (i = 0 ; i < count ; i++)
        free(namelist[i]);

    free(namelist);
}

void run(char *source, char *target)
{
    struct stat statbuf;
    struct passwd *pwd;
    struct group *grp;
    time_t atime, mtime, ctime;
    struct tm *time;
    char q;
    
    //소스 파일이 없으면 에러
    if (access(source, F_OK) != 0) {
        fprintf(stderr, "%s doesn't exist\n", source);
        print_usage();
        exit(1);
    }

    if (stat(source, &statbuf) < 0) {
        fprintf(stderr, "stat error for %s\n", source);
        print_usage();
        exit(1);
    }
    
    //s옵션 활성화시 symlink로 링크파일 생성
    if (opt_s) {
        if (access(target, F_OK) == 0)
            if(remove(target) < 0) {
                fprintf(stderr, "remove error for %s\n", target);
                print_usage();
                exit(1);
            }

        if (symlink(source, target) < 0) {
            fprintf(stderr, "symlink error\n");
            print_usage();
            exit(1);
        }

        return;
    }
    
    //p옵션 활성화 소스파일 정보출력
    if (opt_p) {
        atime = statbuf.st_atime;
        mtime = statbuf.st_mtime;
        ctime = statbuf.st_ctime;

        if ((pwd = getpwuid(statbuf.st_uid)) == NULL) {
            fprintf(stderr, "getpwuid error for %s\n", source);
            print_usage();
            exit(1);
        }

        if ((grp = getgrgid(statbuf.st_gid)) == NULL) {
            fprintf(stderr, "getgrgid error for %s\n", source);
            print_usage();
            exit(1);
        }

        printf("파일 이름 : %s\n", source);
        printf("데이터의 마지막 읽은 시간 : ");
        time = localtime(&atime);
        print_time(*time);
        printf("데이터의 마지막 수정 시간 : ");
        time = localtime(&mtime);
        print_time(*time);
        printf("데이터의 마지막 변경 시간 : ");
        time = localtime(&ctime);
        print_time(*time);
        printf("OWNER : %s\n", pwd->pw_name);
        printf("GROUP : %s\n", grp->gr_name);
        printf("file size : %ld\n", statbuf.st_size);
    }
    
    //소스가 파일일 때
    if (S_ISREG(statbuf.st_mode)) {
        //i옵션 처리
        if (opt_i) {
            if (access(target, F_OK) == 0) {
                printf("overwrite %s (y/n)? ", target);
                scanf("%c", &q);
                if (q == 'y')
                    writefile(source, target);
                else
                    return;
            }
        }
        
        //n옵션 처리
        if (opt_n) {
            if (access(target, F_OK) == 0) {
                return;
            }
        }
        
        //파일복사
        writefile(source, target);
    }
    
    //디렉토리 파일일 때
    if (S_ISDIR(statbuf.st_mode)) {
        //r옵션이 있어야 디렉토리 처리
        if (opt_r) {
            //i옵션 처리
            if (opt_i) {
                if (access(target, F_OK) == 0) {
                    printf("overwrite %s (y/n)? ", target);
                    scanf("%c", &q);
                    if (q == 'y')
                        writedir(source, target);
                    else
                        return;
                }
                else
                    writedir(source, target);
            }//n옵션 처리
            else if (opt_n) {
               if (access(target, F_OK) == 0) {
                   return;
               }
               else 
                   writedir(source, target);
            }
            else
                writedir(source, target);
        }
        else {
           fprintf(stderr, "directory should use -r option.\n");
           print_usage();
           exit(1);
        }
    }
}

int main(int argc, char *argv[])
{
    struct timeval start, end;
    int opt;
    char source[PATH_MAX], target[PATH_MAX];
    int i;

    gettimeofday(&start, NULL);
    
    //옵션 읽기
    while ((opt = getopt(argc, argv, "silnprd:")) != -1) {
        switch (opt) {
            case 's':
                printf("%c option is on\n", opt);
                opt_s = 1;
                break;
            case 'i':
                printf("%c option is on\n", opt);
                opt_i = 1;
                break;
            case 'l':
                printf("%c option is on\n", opt);
                opt_l = 1;
                break;
            case 'n':
                printf("%c option is on\n", opt);
                opt_n = 1;
                break;
            case 'p':
                printf("%c option is on\n", opt);
                opt_p = 1;
                break;
            case 'r':
                printf("%c option is on\n", opt);
                opt_r = 1;
                break;
            case 'd':
                printf("%c option is on\n", opt);
                opt_d = 1;
                child_num = atoi(optarg);
                break;
        }
    }

    memset(source, 0, sizeof(source));
    memset(target, 0, sizeof(target));
    
    //옵션빼고 인자가 2개가 아니면 에러
    if ((argc - optind) != 2) {
        fprintf(stderr, "put <source> <target>\n");
        print_usage();
        exit(1);
    }
    else {
        //소스와 타겟이 PATH_MAX보다 길면 에러
        if (strlen(argv[optind]) > PATH_MAX) {
            fprintf(stderr, "path <%s> is too long\n", argv[optind]);
            print_usage();
            exit(1);
        }

        if (strlen(argv[optind + 1]) > PATH_MAX) {
            fprintf(stderr, "path <%s> is too long\n", argv[optind + 1]);
            print_usage();
            exit(1);
        }

        strcpy(source, argv[optind]);
        strcpy(target, argv[optind + 1]);
        printf("source : %s\n", source);
        printf("target : %s\n", target);
    }
    
    checkopt();
    run(source, target);
    
    //실행시간 출력
    gettimeofday(&end, NULL);
    printf("%ld sec\n", end.tv_sec - start.tv_sec);
    exit(0);
}

