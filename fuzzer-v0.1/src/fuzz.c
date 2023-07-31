#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ERROR_SIZE 4096
#define MAX_SEED_SIZE 5 // 控制哈希值的长度
#define MAX_COVERAGE_SIZE 1024

#define MAX_MUTATION_SIZE 65536 // 最大变异尺寸
#define MAX_MUTATION_OPS 16     // 最大变异操作数
#define MAX_MUTATION_LEN 256    // 最大插入长度
#define MAX_MUTATION_PROB 10000 // 最大变异概率

#define MAX_TEST_CASE_LENGTH 4096
#define MAX_MUTATION_COUNT 5
#define MUTATION_PROBABILITY 0.5

volatile sig_atomic_t child_timed_out = 0;

// 位翻转操作
void flip_bit(char *seed, size_t seed_size)
{
    int byte_pos = rand() % seed_size;
    int bit_pos = rand() % 8;
    seed[byte_pos] ^= 1 << bit_pos;
}

// 整数运算操作
void integer_arithmetic(char *seed, size_t seed_size)
{
    int byte_pos = rand() % seed_size;
    int value = (int)seed[byte_pos];

    switch (rand() % 4)
    {
    case 0:
        // 加法
        value += rand() % 16;
        break;
    case 1:
        // 减法
        value -= rand() % 16;
        break;
    case 2:
        // 乘法
        value *= rand() % 16;
        break;
    case 3:
        // 位运算
        value &= rand() % 255;
        value |= rand() % 255;
        value ^= rand() % 255;
        break;
    }

    seed[byte_pos] = (char)value;
}

// 插入特殊内容操作
void insert_special(char *seed, size_t *seed_size)
{
    if (*seed_size + MAX_MUTATION_LEN >= MAX_MUTATION_SIZE)
    {
        return;
    }

    int i;
    int pos = rand() % (*seed_size + 1);
    int len = rand() % MAX_MUTATION_LEN + 1;
    char *special = "\x00\xff\x55\xaa";
    memmove(seed + pos + len, seed + pos, *seed_size - pos + 1);

    for (i = 0; i < len; i++)
    {
        seed[pos + i] = special[rand() % 4];
    }

    *seed_size += len;
}

// 拼接内容操作
void append_content(char *seed, size_t *seed_size)
{
    const char charset[] = "0123456789abcdef";
    if (*seed_size + MAX_MUTATION_LEN >= MAX_MUTATION_SIZE)
    {
        return;
    }

    int i;
    for (i = 0; i < *seed_size; ++i)
    {
        seed[i] = charset[rand() % (sizeof(charset) - 1)];
    }
}

// havoc操作
void havoc(char *seed, size_t *seed_size)
{
    int n_ops = rand() % 5 + 1;
    int i;
    for (i = 0; i < n_ops; i++)
    {
        int op = rand() % 5;
        switch (op)
        {
        case 0:
            flip_bit(seed, *seed_size);
            break;
        case 1:
            integer_arithmetic(seed, *seed_size);
            break;
        case 2:
            insert_special(seed, seed_size);
            break;
        case 3:
            append_content(seed, seed_size);
            break;
        case 4:
            if (*seed_size <= 1)
            {
                break;
            }
            int start = rand() % (*seed_size - 1);
            int len = rand() % (*seed_size - start) + 1;
            memmove(seed + start, seed + start + len, *seed_size - start - len);
            *seed_size -= len;
            break;
        }
    }
}

// 变异种子函数
size_t mutate(char *mutated_seed, int mutate_flag)
{
    size_t seed_size = strlen(mutated_seed);
    int choose_mutate = 0;
    choose_mutate = mutate_flag % 6;

    switch (choose_mutate)
    {
    case 0:
        flip_bit(mutated_seed, seed_size);
        break;
    case 1:
        integer_arithmetic(mutated_seed, seed_size);
        break;
    case 2:
        insert_special(mutated_seed, &seed_size);
        break;
    case 3:
        append_content(mutated_seed, &seed_size);
        break;
    case 4:
        havoc(mutated_seed, &seed_size);
        break;
    case 5:
        if (seed_size <= 1)
        {
            break;
        }
        int start = rand() % (seed_size - 1);
        int len = rand() % (seed_size - start) + 1;
        memmove(mutated_seed + start, mutated_seed + start + len, seed_size - start - len + 1);
        seed_size -= len;
        break;
    }

    return seed_size;
}

// 读取文件内容函数
unsigned char *readtestcase(char *d_name)
{
    FILE* fp = fopen(d_name, "rb");
    if (fp == NULL) {
        perror("Failed to open file");
        return NULL;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 读取文件内容
    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        perror("Failed to allocate memory");
        fclose(fp);
        return NULL;
    }
    size_t elements_read = fread(buffer, 1, file_size, fp);
    if (elements_read != (size_t)file_size) {
        perror("Error reading file");
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    // 添加字符串结束符
    buffer[file_size] = '\0';
    
    // 复制buffer并释放buffer
    char* result = strdup(buffer);
    free(buffer);
    return result;
    
}

// 将测试结果保存到文件
void save_test_result(unsigned char *d_name, double coverage, char *error, char *output, int flag1, int flag2, int signal_num, int *a, int *b, int *c)
{
    char filename[MAX_INPUT_SIZE];
    /*
        printf("seed:%s\n",seed);
        printf("coverage:%lf\n",coverage);
        printf("error:%s\n",error);
        printf("output:%s\n",output);
        printf("flag:%d\n",flag);
        */
    if(flag1==1)
    {
        snprintf(filename, MAX_INPUT_SIZE, "./out/hang/test_result_%d.txt", *a);
        FILE *fp = fopen(filename, "w");
        if (fp == NULL)
        {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fprintf(fp, "Testcase: %s\n", readtestcase(d_name));
        fprintf(fp, "Signal: %d\n", signal_num);
        fclose(fp);
        *a = *a+1;
        return;
    }
    if (flag2 == 0)
    {
        snprintf(filename, MAX_INPUT_SIZE, "./out/queue/test_result_%d.txt", *b);
        FILE *fp = fopen(filename, "w");
        if (fp == NULL)
        {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fprintf(fp, "Testcase: %s\n", readtestcase(d_name));
        fprintf(fp, "Coverage: %lf\n", coverage);
        fprintf(fp, "Error: %s\n", error);
        if (signal_num != 0)
            fprintf(fp, "Signal: %d\n", signal_num);
        fprintf(fp, "Output: %s\n", output);
        fclose(fp);
        *b = *b+1;
    }
    else
    {
        snprintf(filename, MAX_INPUT_SIZE, "./out/errors/test_result_%d.txt", *c);
        FILE *fp = fopen(filename, "w");
        if (fp == NULL)
        {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fprintf(fp, "Testcase: %s\n", readtestcase(d_name));
        fprintf(fp, "Coverage: %lf\n", coverage);
        fprintf(fp, "Error: %s\n", error);
        if (signal_num != 0)
            fprintf(fp, "Signal: %d\n", signal_num);
        fprintf(fp, "Output: %s\n", output);
        fclose(fp);
        *c = *c+1;
    }
}

int newfz(int coverage, int max_coverage)
{
    if (coverage > max_coverage)
        return 1;
    else
        return 0;
}


void getdata(char *coverage_cmd, int signal_num, int *pipe_fd, int *pipe_err, unsigned char *d_name, double *max_coverage, int *a, int *b, int *c)
{
    double coverage = 0;
    char gcov_output[MAX_COVERAGE_SIZE];
    char output[MAX_ERROR_SIZE] = "";
    char error[MAX_ERROR_SIZE] = "";
    int flag_fd;
    int flag_err;
    int ret;

    //  获取代码覆盖率
    FILE *fp = popen(coverage_cmd, "r"); // popen() 函数通过创建一个管道，调用 fork 产生一个子进程，执行一个 shell 以运行命令来开启一个进程,如果调用 fork() 或 pipe() 失败，或者不能分配内存将返回NULL，否则返回一个读或者打开文件的指针
    if (fp == NULL)
    {
        perror("popen");
        exit(EXIT_FAILURE);
    }

    // printf("parent process i:%d\n", i);

    fgets(gcov_output, MAX_COVERAGE_SIZE, fp);
    pclose(fp);
    coverage = atof(gcov_output); // 将字符串里的数字字符转化为浮点数。返回浮点值

    // 获取文件描述符的当前标志
    flag_fd = fcntl(pipe_fd[0], F_GETFL);
    if (flag_fd == -1)
    {
        perror("fcntl error");
        exit(1);
    }
    flag_err = fcntl(pipe_err[0], F_GETFL);
    if (flag_err == -1)
    {
        perror("fcntl error");
        exit(1);
    }
    // 设置文件描述符为非阻塞模式
    flag_fd |= O_NONBLOCK;
    ret = fcntl(pipe_fd[0], F_SETFL, flag_fd);
    if (ret == -1)
    {
        perror("fcntl error");
        exit(1);
    }
    flag_err |= O_NONBLOCK;
    ret = fcntl(pipe_err[0], F_SETFL, flag_err);
    if (ret == -1)
    {
        perror("fcntl error");
        exit(1);
    }
    // 获取目标程序的输出
    ret = read(pipe_fd[0], output, MAX_INPUT_SIZE - 1);
    if (ret == -1 && errno == EAGAIN)
    {
        // 没有数据可读的非阻塞处理
        // printf("output:%s\n", output);
    }
    else if (ret == -1)
    {
        perror("read error");
        exit(1);
    }
    else
    {
        // printf("output:%s\n", output);
        output[ret] = '\0';
    }

    // 获取目标程序的错误信息
    ret = read(pipe_err[0], error, MAX_ERROR_SIZE - 1);
    if (ret == -1 && errno == EAGAIN)
    {
        // 没有数据可读的非阻塞处理
        printf("error:%s\n", error);
    }
    else if (ret == -1)
    {
        perror("read error");
        exit(1);
    }
    else
    {
        printf("error:%s\n", error);
        error[ret] = '\0';
    }

    printf("coverage:%lf\n\n", coverage);

    // 判断是否出现新的分支
    if (newfz(coverage, *max_coverage))
    { // 如果出现了新的分支
        // 将当前种子与覆盖率传给optimize_seed函数，由该函数进行记录与优化种子
        *max_coverage = coverage;
        save_test_result(d_name, coverage, error, output, 0, 0, signal_num, a, b, c);
        // 更新种子
        // optimize_seed(testcase);
    }
    // 判断程序错误
    if (error[0] != '\0' || signal_num != 0)
    {
        save_test_result(d_name, coverage, error, output, 0, 1, signal_num, a, b, c);
    }
}

void timeout_handler(int signum)
{
    child_timed_out = 1;
}

void run_target_program(int *pipe_fd, int *pipe_err, int i, char *coverage_cmd, unsigned char *d_name, double *max_coverage, int *a, int *b, int *c)
{
    char input[MAX_INPUT_SIZE];
    pid_t pid = fork();

    if (pid == 0)
    {
        // 子进程执行代码
        close(pipe_fd[0]);
        close(pipe_err[0]);

        // printf("my pid is %d\n",getpid());

        printf("process i:%d\n", i);
        printf("d_name:%s\n", d_name);
        printf("子进程开始运行，pid为%d，ppid为%d\n", getpid(), getppid());
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_err[1], STDERR_FILENO);

        char *path = "./vim";  
        char str[50] = "./in/";
        char* args[] = {path, "-u", "NONE", "-X", "-Z", "-e", "-s", "-S", strcat(str, d_name), "-c", ":qa!", NULL};
        execv(path, args);

        // 如果execl调用失败，则打印错误信息并退出子进程
        perror("exec failed");
        exit(1);
/*
        sleep(10);  // 模拟子进程执行耗时任务
        exit(0);    // 子进程结束
*/
    }
    else if (pid > 0)
    {
        printf("父进程开始运行，pid为%d\n", getpid(), getpid());
        // 父进程等待子进程结束并处理僵尸进程
        int status;
        signal(SIGALRM, timeout_handler);   // 设置超时信号处理函数
        pid_t terminated_pid = 0;
        alarm(2);
        while (child_timed_out ==0 && terminated_pid ==0)
        {
            terminated_pid = waitpid(pid, &status, WNOHANG);
        }
        alarm(0);
        printf("父进程等待子进程结束\n");
        if (terminated_pid > 0) {
             if (WIFEXITED(status))
            { // 如果子进程正常退出（通过调用 exit 或返回 main 函数），WIFEXITED(status) 将返回非零值
                getdata(coverage_cmd, 0, pipe_fd, pipe_err,d_name, max_coverage, a, b, c);
            }
            else if (WIFSIGNALED(status))
            { // 如果子进程是由于信号而终止的（例如通过调用 kill 函数），WIFSIGNALED(status) 将返回非零值。
                int signal_num = WTERMSIG(status); // 可以使用 WTERMSIG(status) 获取子进程终止的信号编号。
                getdata(coverage_cmd, signal_num, pipe_fd, pipe_err, d_name, max_coverage, a, b, c);
            }
        }
        else if (terminated_pid == 0) {
            // 等待超时，向子进程发送终止信号
            kill(pid, 9);
            save_test_result(d_name, 0, NULL, NULL, 1, 0, SIGALRM, a, b, c);
        }
        else {
            // waitpid 出错
            printf("Error in waitpid.\n");
        }
       
    }
    else
    {
        // 创建子进程失败的错误处理
        perror("fork failed");
        exit(1);
    }
}

void open_pipe(int pipe_fd[2], int pipe_err[2])
{
     if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipe_err) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
   
}



int main(int argc, char *argv[])
{
    char seed[MAX_SEED_SIZE]="";
    unsigned char testcase[MAX_TEST_CASE_LENGTH]="";
    double coverage = 0;
    double max_coverage = 0;
    int i = 0;
    int a=0;
    int b=0;
    int c=0;
    int pipe_fd[2];
    int pipe_err[2];
    int pipe_asan[2];
    char coverage_cmd[MAX_INPUT_SIZE];
    char input[MAX_INPUT_SIZE];
    DIR *dir;
    struct dirent *entry;

    snprintf(coverage_cmd, MAX_INPUT_SIZE, "gcov ./vim -b | head -n 2 | tail -1 | grep -Eo '[0-9]{1,2}\\.[0-9]{2}|100\\.00'"); // 用于格式化输出字符串，并将结果写入到指定的缓冲区
   
    mkdir("./out", 0775);
    mkdir("./out/hang", 0775);
    mkdir("./out/queue", 0775);
    mkdir("./out/errors", 0775);

    open_pipe(pipe_fd, pipe_err);
    // strcpy(testcase,"./in/hang");
    // 打开目标文件夹
    dir = opendir("./in");
    if (dir != NULL) {
        // 循环遍历目标文件夹的内容
        while ((entry = readdir(dir)) != NULL) {
            // 过滤掉 "." 和 ".." 目录
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                for (i = 0; i < 1; i +=6)
                {
                    run_target_program(pipe_fd, pipe_err, i, coverage_cmd, entry->d_name, &max_coverage, &a, &b, &c);
                    //mutate(testcase, i);
                }
            }
        }
        // 关闭目标文件夹
        closedir(dir);
    } else {
        // 处理打开目标文件夹失败的情况
        perror("Failed to open the directory");
        return 1;
    }
/*
    snprintf(input, MAX_INPUT_SIZE, "gcc -g -O0 -fsanitize=address -fprofile-arcs -ftest-coverage -o target_program_asan target_program_v2.c");
    FILE *fp = popen(input, "r"); // popen() 函数通过创建一个管道，调用 fork 产生一个子进程，执行一个 shell 以运行命令来开启一个进程,如果调用 fork() 或 pipe() 失败，或者不能分配内存将返回NULL，否则返回一个读或者打开文件的指针
    if (fp == NULL)
    {
        perror("插桩失败");
        exit(EXIT_FAILURE);
    }
    pclose(fp);
*/

    

    return 0;
}
