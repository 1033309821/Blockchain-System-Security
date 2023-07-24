#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ERROR_SIZE 1024
#define MAX_SEED_SIZE 5 // 控制哈希值的长度
#define MAX_COVERAGE_SIZE 1024

#define MAX_MUTATION_SIZE 65536 // 最大变异尺寸
#define MAX_MUTATION_OPS 16     // 最大变异操作数
#define MAX_MUTATION_LEN 256    // 最大插入长度
#define MAX_MUTATION_PROB 10000 // 最大变异概率

#define MAX_TEST_CASE_LENGTH 1024
#define MAX_MUTATION_COUNT 5
#define MUTATION_PROBABILITY 0.5

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
size_t mutate(char *seed, char *mutated_seed, int mutate_flag)
{
    size_t seed_size = strlen(seed);
    int choose_mutate = 0;
    choose_mutate = mutate_flag % 6;
    // 对原始种子进行复制
    memcpy(mutated_seed, seed, seed_size + 1);

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

// 将测试结果保存到文件
void save_test_result(char *seed, unsigned char *testcase, double coverage, char *error, char *output, int flag, int signal_num)
{
    char filename[MAX_INPUT_SIZE];
    /*
        printf("seed:%s\n",seed);
        printf("coverage:%lf\n",coverage);
        printf("error:%s\n",error);
        printf("output:%s\n",output);
        printf("flag:%d\n",flag);
        */
    if (flag == 0)
    {
        snprintf(filename, MAX_INPUT_SIZE, "./out/queue/test_result_%s.txt", testcase);
        FILE *fp = fopen(filename, "w");
        if (fp == NULL)
        {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fprintf(fp, "Seed: %s\n", seed);
        fprintf(fp, "Testcase: %s\n", testcase);
        fprintf(fp, "Coverage: %lf\n", coverage);
        fprintf(fp, "Error: %s\n", error);
        if (signal_num != 0)
            fprintf(fp, "Signal: %d\n", signal_num);
        fprintf(fp, "Output: %s\n", output);
        fclose(fp);
    }
    else
    {
        snprintf(filename, MAX_INPUT_SIZE, "./out/errors/test_result_%s.txt", testcase);
        FILE *fp = fopen(filename, "w");
        if (fp == NULL)
        {
            perror("fopen");
            exit(EXIT_FAILURE);
        }
        fprintf(fp, "Seed: %s\n", seed);
        fprintf(fp, "Testcase: %s\n", testcase);
        fprintf(fp, "Coverage: %lf\n", coverage);
        fprintf(fp, "Error: %s\n", error);
        if (signal_num != 0)
            fprintf(fp, "Signal: %d\n", signal_num);
        fprintf(fp, "Output: %s\n", output);
        fclose(fp);
    }
}

// 更新种子
void optimize_seed(char *seed, unsigned char *testcase)
{
    strcpy(seed, testcase);
}

int newfz(int coverage, int max_coverage)
{
    if (coverage > max_coverage)
        return 1;
    else
        return 0;
}

void asan_target_program(char *seed, unsigned char *testcase)
{
    char input1[MAX_INPUT_SIZE];
    char input2[MAX_INPUT_SIZE];
    snprintf(input1, MAX_INPUT_SIZE, "gcc -fsanitize=address -g -o target_program_asan target_program.c");
    FILE *fp = popen(input1, "r"); // popen() 函数通过创建一个管道，调用 fork 产生一个子进程，执行一个 shell 以运行命令来开启一个进程,如果调用 fork() 或 pipe() 失败，或者不能分配内存将返回NULL，否则返回一个读或者打开文件的指针
    if (fp == NULL)
    {
        perror("asan命令1执行失败");
        exit(EXIT_FAILURE);
    }
    snprintf(input2, MAX_INPUT_SIZE, "echo \"%s\" | ./target_program_asan 2> ./out/asan/seed:%s_testcase:%s.txt", testcase, seed, testcase);
    fp = popen(input2, "r"); // popen() 函数通过创建一个管道，调用 fork 产生一个子进程，执行一个 shell 以运行命令来开启一个进程,如果调用 fork() 或 pipe() 失败，或者不能分配内存将返回NULL，否则返回一个读或者打开文件的指针
    if (fp == NULL)
    {
        perror("asan命令2执行失败");
        exit(EXIT_FAILURE);
    }
}

void getdata(char *coverage_cmd, int signal_num, int *pipe_fd, int *pipe_err, char *seed, unsigned char *testcase, double *max_coverage)
{
    double coverage = 0;
    char gcov_output[MAX_COVERAGE_SIZE];
    char output[MAX_COVERAGE_SIZE] = "";
    char error[MAX_COVERAGE_SIZE] = "";
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
    ret = read(pipe_err[0], error, MAX_INPUT_SIZE - 1);
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
        save_test_result(seed, testcase, coverage, error, output, 0, signal_num);
        // 更新种子
        optimize_seed(seed, testcase);
    }
    // 判断程序错误
    if (error[0] != '\0' || signal_num != 0)
    {
        save_test_result(seed, testcase, coverage, error, output, 1, signal_num);
        asan_target_program(seed, testcase);
    }
}

void run_target_program(int *pipe_fd, int *pipe_err, int i, char *coverage_cmd, char *seed, unsigned char *testcase, double *max_coverage)
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
        printf("seed:%s\n", seed);
        printf("testcase:%s\n", testcase);
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_err[1], STDERR_FILENO);

        // char *args[] = {"target_program", testcase, NULL};
        snprintf(input, MAX_INPUT_SIZE, "echo \"%s\" | ./target_program", testcase);
        execl("/bin/sh", "sh", "-c", input, (char *)NULL); // shell 会将 -c 后的部分当做命令来执行。
        // execv("./target_program", args);

        // 如果execl调用失败，则打印错误信息并退出子进程
        perror("exec failed");
        exit(1);
    }
    else if (pid > 0)
    {
        // 父进程等待子进程结束并处理僵尸进程
        int status;
        waitpid(pid, &status, 0);
        // printf("father i :%d\n", i);

        if (WIFEXITED(status))
        { // 如果子进程正常退出（通过调用 exit 或返回 main 函数），WIFEXITED(status) 将返回非零值
            getdata(coverage_cmd, 0, pipe_fd, pipe_err, seed, testcase, max_coverage);
        }
        else if (WIFSIGNALED(status))
        {                                      // 如果子进程是由于信号而终止的（例如通过调用 kill 函数），WIFSIGNALED(status) 将返回非零值。
            int signal_num = WTERMSIG(status); // 可以使用 WTERMSIG(status) 获取子进程终止的信号编号。
            getdata(coverage_cmd, signal_num, pipe_fd, pipe_err, seed, testcase, max_coverage);
        }
    }
    else
    {
        // 创建子进程失败的错误处理
        perror("fork failed");
        exit(1);
    }

    // 最后更新testcase
    // mutate(seed, testcase);
    // strcpy(testcase, "0xaa");
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
    char seed[MAX_SEED_SIZE];
    unsigned char testcase[MAX_SEED_SIZE];
    double coverage = 0;
    double max_coverage = 0;
    int i = 0;
    int pipe_fd[2];
    int pipe_err[2];
    char coverage_cmd[MAX_INPUT_SIZE];
    char input[MAX_INPUT_SIZE];

    snprintf(coverage_cmd, MAX_INPUT_SIZE, "gcov ./target_program -b | head -n 2 | tail -1 | grep -Eo '[0-9]{1,2}\\.[0-9]{2}|100\\.00'"); // 用于格式化输出字符串，并将结果写入到指定的缓冲区
    // 检查命令行参数
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s target_program\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    mkdir("./out", 0775);
    mkdir("./out/queue", 0775);
    mkdir("./out/errors", 0775);
    mkdir("./out/asan", 0775);
    /*
        for(;i<argc;i++){
          strcpy( seed, *(argv+i));
            // 根据种子生成随机输入，并执行目标程序，获取代码覆盖率和输出
            coverage = run_target_program(seed, input, MAX_INPUT_SIZE, output, MAX_INPUT_SIZE, error);

            // 将测试结果保存到文件
            save_test_result(seed, coverage, error, output);
        }

    */
    // 设置随机数种子
    srand(time(NULL));

    // mutate(seed, testcase);
    strcpy(seed, argv[2]);
    strcpy(testcase, argv[2]);

    open_pipe(pipe_fd, pipe_err);

    snprintf(input, MAX_INPUT_SIZE, "gcc target_program.c -fprofile-arcs -ftest-coverage -o target_program");
    FILE *fp = popen(input, "r"); // popen() 函数通过创建一个管道，调用 fork 产生一个子进程，执行一个 shell 以运行命令来开启一个进程,如果调用 fork() 或 pipe() 失败，或者不能分配内存将返回NULL，否则返回一个读或者打开文件的指针
    if (fp == NULL)
    {
        perror("gcc插桩失败");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < 100; i += 6)
    {
        run_target_program(pipe_fd, pipe_err, i, coverage_cmd, seed, testcase, &max_coverage);
        mutate(seed, testcase, i);
    }

    return 0;
}
