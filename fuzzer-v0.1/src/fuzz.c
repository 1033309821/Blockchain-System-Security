#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ERROR_SIZE 1024
#define MAX_SEED_SIZE 5     //控制哈希值的长度
#define MAX_COVERAGE_SIZE 1024

#define MAX_TEST_CASE_LENGTH 1024
#define MAX_MUTATION_COUNT 5
#define MUTATION_PROBABILITY 0.5

// 生成一个随机种子，为一串哈希值
void generate_seed(char* seed) {
    const char charset[] = "0123456789abcdef";
    seed[0] = '0';
    seed[1] = 'x';
    int i;
    for (i = 2; i < MAX_SEED_SIZE-1; ++i) {
        seed[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    seed[MAX_SEED_SIZE-1] = '\0';
}

// 利用种子进行变异，生成testcase用于执行target
void mutate(char* seed, char* test_case) {
    // Copy the seed to the test case
    strcpy(test_case, seed);

    // Get the length of the seed
    int seed_length = strlen(seed);

    // Randomly mutate the test case
    srand(time(NULL)); // Seed the random number generator
    int mutation_count = rand() % MAX_MUTATION_COUNT + 1; // Choose a random number of mutations
    for (int i = 0; i < mutation_count; i++) {
        // Choose a random character to mutate
        int mutation_index = rand() % seed_length;

        // Randomly choose a mutation strategy
        int mutation_strategy = rand() % 4;
        switch (mutation_strategy) {
        case 0:
            // Flip the case of a letter
            if (seed[mutation_index] >= 'a' && seed[mutation_index] <= 'z') {
                test_case[mutation_index] = seed[mutation_index] - 'a' + 'A';
            } else if (seed[mutation_index] >= 'A' && seed[mutation_index] <= 'Z') {
                test_case[mutation_index] = seed[mutation_index] - 'A' + 'a';
            }
            break;
        case 1:
            // Replace a character with a random printable ASCII character
            test_case[mutation_index] = rand() % 94 + 32;
            break;
        case 2:
            // Insert a random printable ASCII character
            if (strlen(test_case) < MAX_TEST_CASE_LENGTH - 1) {
                memmove(test_case + mutation_index + 1, test_case + mutation_index, strlen(test_case) - mutation_index + 1);
                test_case[mutation_index] = rand() % 94 + 32;
            }
            break;
        case 3:
            // Delete a character
            if (strlen(test_case) > 1) {
                memmove(test_case + mutation_index, test_case + mutation_index + 1, strlen(test_case) - mutation_index);
            }
            break;
        }
    }
}

// 根据种子生成随机输入，并执行目标程序
int run_target_program(unsigned char *testcase, char *input, int input_size, char *output, int output_size, char *error) {
    int coverage = 0;
    char coverage_cmd[MAX_INPUT_SIZE];
    char gcov_output[MAX_COVERAGE_SIZE];
    pid_t pid;

    // 生成随机输入
    /*
    snprintf() 函数的返回值是输出到 str 缓冲区中的字符数，不包括字符串结尾的空字符 \0。
    如果 snprintf() 输出的字符数超过了 size 参数指定的缓冲区大小，则输出的结果会被截断，
    只有 size - 1 个字符被写入缓冲区，最后一个字符为字符串结尾的空字符 \0。
    需要注意的是，snprintf() 函数返回的字符数并不包括字符串结尾的空字符 \0，
    因此如果需要将输出结果作为一个字符串使用，则需要在缓冲区的末尾添加一个空字符 \0。
    */
    snprintf(input, input_size, "echo %s | ./target_program", testcase);

    // 执行目标程序，并获取输出
    int pipe_fd[2];
    int pipe_err[2];

    /*
    该函数成功时返回0，并将一对打开的文件描述符值填入fd参数指向的数组。失败时返回 -1并设置errno。
    通过pipe函数创建的这两个文件描述符 fd[0] 和 fd[1] 分别构成管道的两端，往 fd[1] 写入的数据可以从 fd[0] 读出。
    并且 fd[1] 一端只能进行写操作，fd[0] 一端只能进行读操作，不能反过来使用。要实现双向数据传输，可以使用两个管道。
    */
    if(pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
     }
    if(pipe(pipe_err) == -1) {
 	    perror("pipe");
	    exit(EXIT_FAILURE);
    }

    pid = fork();
    /*
    这个地方要判断pid是否为0是因为fork函数的实现原理，fork函数最后的return 0是子进程进行
        的，所以进入这个判断的是子进程，而子进程返回的pid就是0，如果这个地方不加上该判断，子进
        程也会进入该for循环来创造进程，子又生孙孙又生子，而我们只希望父进程来创建三个子进程，
        所以加上了该判断
    */

    if(pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if(pid == 0) {
        // 子进程执行目标程序，并将输出重定向到管道
	    close(pipe_fd[0]);
	    close(pipe_err[0]);
  	    dup2(pipe_fd[1], STDOUT_FILENO);
	    dup2(pipe_err[1], STDERR_FILENO);
	    execl("/bin/sh", "sh", "-c", input, (char *) NULL);//shell 会将 -c 后的部分当做命令来执行。
        exit(EXIT_FAILURE);
    } else {
        // 父进程等待子进程执行完成，并获取代码覆盖率
        close(pipe_fd[1]);
	    close(pipe_err[1]);
        waitpid(pid, NULL, 0);

        // 获取代码覆盖率
        snprintf(coverage_cmd, MAX_INPUT_SIZE, "gcov target_program > /dev/null && cat target_program.c.gcov | grep -o '[1-9]: ' | wc -l");//用于格式化输出字符串，并将结果写入到指定的缓冲区
        FILE *fp = popen(coverage_cmd, "r");//popen() 函数通过创建一个管道，调用 fork 产生一个子进程，执行一个 shell 以运行命令来开启一个进程,如果调用 fork() 或 pipe() 失败，或者不能分配内存将返回NULL，否则返回一个读或者打开文件的指针
        if(fp == NULL) {
            perror("popen");
            exit(EXIT_FAILURE);
        }

        fgets(gcov_output, MAX_COVERAGE_SIZE, fp);
        pclose(fp);
        coverage = atoi(gcov_output);//将字符串里的数字字符转化为整形数。返回整形值

        // 获取目标程序的输出
        int nbytes = read(pipe_fd[0], output, output_size - 1);
        if(nbytes == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        output[nbytes] = '\0';

	    // 获取目标程序的错误信息
	    nbytes = read(pipe_err[0], error, MAX_ERROR_SIZE - 1);
	    if(nbytes == -1) {
    		perror("read");
    		exit(EXIT_FAILURE);
	    }
	    error[nbytes] = '\0';

        return coverage;
    }
}

// 将测试结果保存到文件
void save_test_result(char *seed, unsigned char *testcase, int coverage, char *error, char *output) {
    char filename[MAX_INPUT_SIZE];
/*
    printf("seed:%s\n",seed);
    printf("coverage:%d\n",coverage);
    printf("error:%s\n",error);
    printf("output:%s\n",output);
*/
    if(error[0]=='\0'){
	    snprintf(filename, MAX_INPUT_SIZE, "./out/queue/test_result_%s.txt", seed);
	    FILE *fp = fopen(filename, "w");
    	if(fp == NULL) {
        	perror("fopen");
        	exit(EXIT_FAILURE);
    	}
	    fprintf(fp, "Seed: %s\n", seed);
        fprintf(fp, "Testcase: %s\n", testcase);
    	fprintf(fp, "Coverage: %d\n", coverage);
    	fprintf(fp, "Error: %s\n", error);
    	fprintf(fp, "Output: %s\n", output);
	    fclose(fp);
    }
    else{
     	snprintf(filename, MAX_INPUT_SIZE, "./out/errors/test_result_%s.txt", seed);
	    FILE *fp = fopen(filename, "w");
    	if(fp == NULL) {
        	perror("fopen");
        	exit(EXIT_FAILURE);
    	}
	    fprintf(fp, "Seed: %s\n", seed);
        fprintf(fp, "Testcase: %s\n", testcase);
    	fprintf(fp, "Coverage: %d\n", coverage);
    	fprintf(fp, "Error: %s\n", error);
    	fprintf(fp, "Output: %s\n", output);
	    fclose(fp);
    }
}

// 更新种子
void optimize_seed(char *seed, unsigned char *testcase) {
    strcpy(seed, testcase);
}

int main(int argc, char *argv[]) {
    char seed[MAX_SEED_SIZE];
    char input[MAX_INPUT_SIZE];
    char output[MAX_INPUT_SIZE];
    char error[MAX_ERROR_SIZE];
    int coverage = 0;
    int max_coverage = 0;
    int i = 0;


    // 检查命令行参数
    if(argc < 2) {
        fprintf(stderr, "Usage: %s target_program\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    mkdir("./out",0775);
    mkdir("./out/queue",0775);
    mkdir("./out/errors",0775);
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

    // 生成一个随机种子
    generate_seed(seed);
    //strcpy(seed,"0xaa");

    unsigned char * testcase; 
    mutate(seed, testcase);
    // 根据种子生成随机输入，并执行目标程序，获取代码覆盖率和输出
    max_coverage = run_target_program(testcase, input, MAX_INPUT_SIZE, output, MAX_INPUT_SIZE, error);
    save_test_result(seed, testcase, coverage, error, output);

    for(;i<10000;i++) {

        unsigned char * testcase; 
        mutate(seed, testcase);

        // 根据种子生成随机输入，并执行目标程序，获取代码覆盖率和输出
        coverage = run_target_program(seed, input, MAX_INPUT_SIZE, output, MAX_INPUT_SIZE, error);

        if(coverage > max_coverage){
            // 将当前种子与覆盖率传给optimize_seed函数，由该函数进行记录与优化种子
            max_coverage = coverage;
            save_test_result(seed, testcase, coverage, error, output);
            optimize_seed(seed, testcase); 
        }

        // 添加代码 如果测试用例导致了错误，执行保存（未解决）
        
   }

    return 0;
}


