/*
* Linux course assignment - ls
* Author: Semprathlon
* Date: 2017/12/17
*
* Options: -l -i -t -a -r  -h
* 功能分别为长格式、显示index、按修改时间排序子目录、显示隐藏文件、倒序显示、显示帮助。
*
* Usage: ./ls [OPTION]... [FILE]
*
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <sys/ioctl.h>
#include <pwd.h>  // 用来得到用户名
#include <grp.h>  // 用来得到组名
#endif

#define MAX_FILENAME_LEN 256
#define MAX_FILE_NUM 512

// ls options
#define LONG_FMT      1<<0 // -l
#define SORT_BY_MTIME 1<<1 // -t
#define SHOW_INODE    1<<2 // -i
#define ALL           1<<3 // -a
#define REVERSE       1<<4 // -r

struct attribute //存储文件属性的数据结构
{
    char* mode;  // 文件属性和权限
    int links;      // 链接数
    char* user_name; // 用户名
    char* group_name; // 所在的用户组
    long size;      // 文件大小
    char* mtime; // 最后修改的时间
    char* filename; // 文件名
    char* extra;  // 用来显示时候要加 "*"(可以执行的文件) 或者 "/" (目录) 的额外字符串
    long md,tm;
    unsigned int inode;
};

//int max(int a,int b){
//    return a>b ? a : b;
//}

void lsQuery(char *dir,int option);

char ls_help[]="用法: ./ls [OPTION]... [FILE]...\n(默认为查看当前目录).\n";

int main(int argc, char **argv) {

    int cur_arg=1,option=0;
    for(; cur_arg<argc; cur_arg++) { //接收参数，设置选项掩码
        if (argv[cur_arg][0]!='-') {
            break;
        }
        if (!strcmp(argv[cur_arg],"-l")) {
            option|=LONG_FMT;
        }
        if (!strcmp(argv[cur_arg],"-t")) {
            option|=SORT_BY_MTIME;
        }
        if (!strcmp(argv[cur_arg],"-i")||!strcmp(argv[cur_arg],"--inode")) {
            option|=SHOW_INODE;
        }
        if (!strcmp(argv[cur_arg],"-a")||!strcmp(argv[cur_arg],"--all")) {
            option|=ALL;
        }
        if (!strcmp(argv[cur_arg],"-r")||!strcmp(argv[cur_arg],"--reverse")) {
            option|=REVERSE;
        }
        if (!strcmp(argv[cur_arg], "-h")||!strcmp(argv[cur_arg],"--help")) { /* 显示ls用法指南 */
            printf(ls_help);
        }
    }
    int dir_num=0;
    for(; cur_arg<argc; cur_arg++) {
        dir_num++;
        printf("%s:\n", argv[cur_arg]); //多个目录分别显示

        char* dir=argv[cur_arg];
        int len = strlen(dir);
        if (dir[len - 1] != '/')
        {
            strcat(dir,"/"); //补上目录末尾的/
        }

        lsQuery(dir,option);
        printf("\n");

    }
    if (!dir_num) { // 默认为查看当前目录
        lsQuery("./",option);
        printf("\n");
    }

    return 0;
}

int getMaxTermWidth(){ // 获得终端窗口宽度
    #ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return columns;
    #elif __linux__
    winsize w {};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return (w.ws_col == 0) ? 82 : w.ws_col;
    #endif
}


// 由 int 型的 mode 转换为字符串
void mode2str(int mode, char str[])
{
    strcpy(str, "----------\0");
    #ifdef _WIN32
    if (mode&(1<<14)) str[0]='d';
    #elif __linux__
    if(S_ISDIR(mode)) str[0] = 'd';
    if(S_ISCHR(mode)) str[0] = 'c';
    if(S_ISBLK(mode)) str[0] = 'b';
    if(S_ISLNK(mode)) str[0] = 'l';
    #endif

    if(mode & S_IRUSR) str[1] = 'r';
    if(mode & S_IWUSR) str[2] = 'w';
    if(mode & S_IXUSR) str[3] = 'x';

    if(mode & S_IRGRP) str[4] = 'r';
    if(mode & S_IWGRP) str[5] = 'w';
    if(mode & S_IXGRP) str[6] = 'x';

    if(mode & S_IROTH) str[7] = 'r';
    if(mode & S_IWOTH) str[8] = 'w';
    if(mode & S_IXOTH) str[9] = 'x';
}

#ifdef __linux__
// 根据用户的 id 值,得到用户名
void uid2str(uid_t uid, char *user_name)
{
    struct passwd *pw_ptr;
    pw_ptr = getpwuid(uid);

    if( pw_ptr == NULL) {
        sprintf(user_name, "%d", uid);
    } else {
        strcpy(user_name, pw_ptr->pw_name);
    }
}
#endif

#ifdef __linux__
// 根据用户组的 id 值,得到用户组名
void gid2str(gid_t gid, char *group_name)
{
    struct group *grp_ptr;
    grp_ptr = getgrgid(gid);

    if( grp_ptr == NULL) {
        sprintf(group_name, "%d", gid);
    } else {
        strcpy(group_name, grp_ptr->gr_name);
    }
}
#endif

// 时间的格式化
void time2str(time_t t, char *time_str)
{
    strcpy( time_str, ctime(&t) + 4);
    time_str[12] = '\0';
}

// 要显示的某一个文件详细信息加载到 attribute 中
struct attribute* query_file_attributes(char *dirname, char *filename)
{
    struct attribute* file_attri=(struct attribute*)malloc(sizeof(struct attribute));
    // 根据文件夹名和文件名得到全名
    char* fullname=(char*)malloc(MAX_FILENAME_LEN);
    strcpy(fullname,dirname);
    strcat(fullname,filename);


    struct stat mystat;
    if ( stat(fullname, &mystat) == -1) {
        printf("query_file_attributes: stat error\n");
        return NULL;
    } else {

        int mode   = (int)  mystat.st_mode;
        int links  = (int)  mystat.st_nlink;
        int uid    = (int)  mystat.st_uid;
        int gid    = (int)  mystat.st_gid;
        long size  = (long) mystat.st_size;
        long mtime = (long) mystat.st_mtime;

        char str_mode[10]={0};  /* 文件类型和权限, "drwxrwx---" */
        char str_user_name[20]={0};
        char str_group_name[20]={0};
        char str_mtime[20]={0};

        mode2str(mode, str_mode);
    #ifdef __linux__
        uid2str(uid, str_user_name);
        gid2str(gid, str_group_name);
    #endif
        time2str(mtime, str_mtime);

        char extra[3] = {0};
        if (str_mode[0] == 'd') {
            extra[0] = '/';
        }

        file_attri->mode=str_mode;
        file_attri->links = links;
        file_attri->user_name=str_user_name;
        file_attri->group_name=str_group_name;
        file_attri->size = size;
        file_attri->mtime=str_mtime;
        file_attri->filename=filename;
        file_attri->extra=extra;
        file_attri->inode=mystat.st_ino;

        file_attri->md=mode;
        file_attri->tm=mtime;
    }
    return file_attri;
}

int str_compare(const char *a,const char *b){ //字符串排序用
    size_t len1=strlen(a),len2=strlen(b);
    char* str1=(char*)malloc(len1);
    char* str2=(char*)malloc(len2);
    strcpy(str1,a);
    strcpy(str2,b);
    for(int i=0;i<len1;i++)
        str1[i]=tolower(str1[i]);
    for(int i=0;i<len2;i++)
        str2[i]=tolower(str2[i]);
    return strcmp(str1,str2);
}

int attri_compare_by_name(const void* a,const void* b){ //按文件名正序排序
    const struct attribute* _a=a;
    const struct attribute* _b=b;
    return str_compare(_a->filename,_b->filename);
}

int attri_compare_by_mtime(const void* a,const void* b){ // 按修改时间倒序排序
    const struct attribute* _a=a;
    const struct attribute* _b=b;
    long tm1= _a->tm;
    long tm2= _b->tm;
    if (tm1==tm2) {
        return attri_compare_by_name(a,b);
    }
    return tm1>tm2;
}

int digits(long n) {  //求long值的位数
    int ret = 0;
    while(n) {
        n = n / 10;
        ++ret;
    }
    return ret;
}

void addFile(struct attribute** files,struct attribute* newfile, int *num){
    files[(*num)++]=newfile;
}

void lsQuery(char *dir,int option){
    DIR *mydir = opendir( dir ); /* directory */

    // 用来暂时存储要显示的目录下的所有文件名,每个文件名最长为MAX_FILENAME_LEN

    struct attribute** file_attribute=(struct attribute**)malloc(MAX_FILE_NUM* sizeof(struct attribute*));
    int file_num = 0;

    if (mydir == NULL) {
        // 直接显示该文件
        printf("%s\n", dir);
        return;
    } else {

        // 循环检查下面有多少文件,并把文件名全部放到filenames数组里
        struct dirent *mydirent; /* file */
        while ( (mydirent = readdir( mydir )) != NULL) {
            if ((option&ALL)||mydirent->d_name[0] != '.' ) {
                addFile(file_attribute,query_file_attributes(dir,mydirent->d_name),&file_num);
            }
        }
        closedir( mydir );
    }
    if (option&REVERSE) {
        qsort(file_attribute+sizeof(struct attribute*)*(file_num-1),-file_num,sizeof(struct attribute*),((option&SORT_BY_MTIME) ? attri_compare_by_mtime : attri_compare_by_name));
    }
    else{
        qsort(file_attribute,file_num,sizeof(struct attribute*),((option&SORT_BY_MTIME) ? attri_compare_by_mtime : attri_compare_by_name));
    }

    // 文件名的最大长度
    int max_len = 0;
    int max_width=getMaxTermWidth();

    for(int i = 0; i < file_num; ++i) {
        max_len = max(max_len,strlen(file_attribute[i]->filename)+4);
    }

    // 格式化输出时,考虑每个属性值的最大宽度
    int max_mode = 0;
    int max_links = 0;
    int max_user_name = 0;
    int max_group_name = 0;
    int max_size = 0;
    int max_mtime = 0;
    int max_filename = 0;
    int max_extra = 0;
    int max_ino=0;

    for (int i = 0; i < file_num; ++i)
    {
        max_ino=max(max_ino,digits(file_attribute[i]->inode));
        max_mode = max(max_mode,strlen(file_attribute[i]->mode));
        max_links = max(max_links,digits(file_attribute[i]->links));
        max_user_name = max(max_user_name,strlen(file_attribute[i]->user_name));
        max_group_name = max(max_group_name,strlen(file_attribute[i]->group_name));
        max_size = max(max_size,digits(file_attribute[i]->size));
        max_mtime = max(max_mtime,strlen(file_attribute[i]->mtime));
        max_filename = max(max_filename,strlen(file_attribute[i]->filename));
        max_extra = max(max_extra,strlen(file_attribute[i]->extra));
    }

    int current_len = 0;
    for(int i = 0; i < file_num; i++) {
        if (option&LONG_FMT) { //长格式输出
            if (option&SHOW_INODE) { // 输出index
                printf("%-*u ",max_ino,file_attribute[i]->inode );
            }
            printf("%*s %*d %*s %*s %*ld %*s %s%s\n",max_mode,file_attribute[i]->mode,
                   max_links, file_attribute[i]->links,
                   max_user_name, file_attribute[i]->user_name,
                   max_group_name, file_attribute[i]->group_name,
                   max_size, file_attribute[i]->size,
                   max_mtime, file_attribute[i]->mtime,
                   file_attribute[i]->filename, file_attribute[i]->extra);
        }
        else{ // 短格式输出
            if (option&SHOW_INODE) { // 输出index
                printf("%-*u ",max_ino,file_attribute[i]->inode );
            }
            printf("%-*s",max_len,file_attribute[i]->filename);
            current_len += max_len+((option&SHOW_INODE) ? max_ino+1 : 0); // 一行超宽则换行
            if(current_len+max_len+((option&SHOW_INODE) ? max_ino+1 : 0) > max_width) {
                printf("\n");
                current_len = 0;
            }
        }
    }
    printf("\n");
}