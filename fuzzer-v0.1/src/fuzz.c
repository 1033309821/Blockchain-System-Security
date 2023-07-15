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
    int i = 0;
    // Copy the seed to the test case
    strcpy(test_case, seed);
    //printf("v1\n");
    // Get the length of the seed
    int seed_length = strlen(seed);

    // Randomly mutate the test case
    srand(time(NULL)); // Seed the random number generator
    int mutation_count = rand() % MAX_MUTATION_COUNT + 1; // Choose a random number of mutations
    //printf("v2\n");
    for (i = 0; i < mutation_count; i++) {
        // Choose a random character to mutate
        int mutation_index = rand() % seed_length;
        //printf("v3\n");
        // Randomly choose a mutation strategy
        int mutation_strategy = rand() % 4;
        //printf("v4\n");
        switch (mutation_strategy) {
        case 0:
		//printf("v5\n");
            // Flip the case of a letter
            if (seed[mutation_index] >= 'a' && seed[mutation_index] <= 'z') {
                test_case[mutation_index] = seed[mutation_index] - 'a' + 'A';
            } else if (seed[mutation_index] >= 'A' && seed[mutation_index] <= 'Z') {
                test_case[mutation_index] = seed[mutation_index] - 'A' + 'a';
            }
            break;
        case 1:
		//printf("v6\n");
            // Replace a character with a random printable ASCII character
            test_case[mutation_index] = rand() % 94 + 32;
            break;
        case 2:
		//printf("v7\n");
            // Insert a random printable ASCII character
            if (strlen(test_case) < MAX_TEST_CASE_LENGTH - 1) {
                memmove(test_case + mutation_index + 1, test_case + mutation_index, strlen(test_case) - mutation_index + 1);
                test_case[mutation_index] = rand() % 94 + 32;
            }
            break;
        case 3:
		//printf("v8\n");
            // Delete a character
            if (strlen(test_case) > 1) {
                memmove(test_case + mutation_index, test_case + mutation_index + 1, strlen(test_case) - mutation_index);
            }
            break;
        }
    }
}



// 将测试结果保存到文件
void save_test_result(char *seed, unsigned char *testcase, double coverage, char *error, char *output, int flag) {
    char filename[MAX_INPUT_SIZE];
/*
    printf("seed:%s\n",seed);
    printf("coverage:%lf\n",coverage);
    printf("error:%s\n",error);
    printf("output:%s\n",output);
    printf("flag:%d\n",flag);
	*/
    if(flag == 0 ){
	    snprintf(filename, MAX_INPUT_SIZE, "./out/queue/test_result_%s.txt", seed);
	    FILE *fp = fopen(filename, "w");
    	if(fp == NULL) {
            perror("fopen");
        	exit(EXIT_FAILURE);
    	}
	    fprintf(fp, "Seed: %s\n", seed);
        fprintf(fp, "Testcase: %s\n", testcase);
    	fprintf(fp, "Coverage: %lf\n", coverage);
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
    	fprintf(fp, "Coverage: %lf\n", coverage);
    	fprintf(fp, "Error: %s\n", error);
    	fprintf(fp, "Output: %s\n", output);
	    fclose(fp);
    }
}

// 更新种子
void optimize_seed(char *seed, unsigned char *testcase) {
    strcpy(seed, testcase);
}

int newfz(int coverage, int max_coverage){
    if(coverage>max_coverage) return 1;
    else return 0; 
}

volatile sig_atomic_t signal_received = 0;

// 自定义信号处理函数
void signal_handler(int signum) {
    signal_received = 1;
}

int main(int argc, char *argv[]) {
    char seed[MAX_SEED_SIZE];
    char input[MAX_INPUT_SIZE];
    char output[MAX_INPUT_SIZE];
    char error[MAX_ERROR_SIZE];
    unsigned char testcase[MAX_SEED_SIZE];
    double coverage = 0;
    double max_coverage = 0;
    int i = 0;
    int pipe_fd[2];
    int pipe_err[2];
    char coverage_cmd[MAX_INPUT_SIZE];
    char gcov_output[MAX_COVERAGE_SIZE];
    
    pid_t pid;
    int jc_flag=0;
    snprintf(coverage_cmd, MAX_INPUT_SIZE, "gcov ./target_program -b | head -n 2 | tail -1 | grep -Eo '[0-9]{1,2}\\.[0-9]{2}|100\\.00'"); //用于格式化输出字符串，并将结果写入到指定的缓冲区
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

    mutate(seed, testcase);

    if(pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if(pipe(pipe_err) == -1) {
 	  perror("pipe");
	  exit(EXIT_FAILURE);
    }
    if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
        perror("signal failed");
        exit(1);
    }
    pid = fork();
    
    for (i = 0; i < 10; i++) {
        //pid_t pid = fork();
        if (pid == 0) {
            // 子进程执行代码
            close(pipe_fd[0]);
            close(pipe_err[0]);
	    
		    printf("child process i:%d\n",i);
		    printf("seed:%s\n",seed);
		    printf("testcase:%s\n",testcase);
            dup2(pipe_fd[1], STDOUT_FILENO);
            dup2(pipe_err[1], STDERR_FILENO);

		

	        snprintf(input, MAX_INPUT_SIZE, "echo %s | ./target_program", testcase);
	        kill(getppid(), SIGUSR1); // 向父进程发送信号
	        execl("/bin/sh", "sh", "-c", input, (char *) NULL);//shell 会将 -c 后的部分当做命令来执行。
            //execl("./target_program", "target_program", testcase, NULL);

            // 如果execl调用失败，则打印错误信息并退出子进程
            perror("exec failed");
            exit(1);
        } else if (pid > 0) {
            // 父进程等待子进程结束并处理僵尸进程
 		while (!signal_received);
		printf("father i :%d\n",i);
		printf("signal_received:%d\n",signal_received);
		int status;
		waitpid(pid, &status, 0);
		close(pipe_fd[1]);
	    close(pipe_err[1]);
             
        if (WIFEXITED(status)) {//如果子进程正常退出（通过调用 exit 或返回 main 函数），WIFEXITED(status) 将返回非零值
            int exit_status = WEXITSTATUS(status);//可以使用 WEXITSTATUS(status) 获取子进程的退出状态码。
            if (exit_status == 0) { //子进程正常执行完毕
                //printf("Child process exited with non-zero status: %d\n", exit_status);
                // 获取代码覆盖率
                FILE *fp = popen(coverage_cmd, "r");//popen() 函数通过创建一个管道，调用 fork 产生一个子进程，执行一个 shell 以运行命令来开启一个进程,如果调用 fork() 或 pipe() 失败，或者不能分配内存将返回NULL，否则返回一个读或者打开文件的指针
                if(fp == NULL) {
                    perror("popen");
                    exit(EXIT_FAILURE);
                }
                
                printf("parent process i:%d\n",i);
        
                fgets(gcov_output, MAX_COVERAGE_SIZE, fp);
                printf("gcov_output=%s",gcov_output);
                pclose(fp);
                coverage = atof(gcov_output);//将字符串里的数字字符转化为浮点数。返回浮点值

                // 获取目标程序的输出
                int nbytes = read(pipe_fd[0], output, MAX_INPUT_SIZE - 1);
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
            
            
                printf("coverage:%lf\n",coverage);
                printf("error:%s\n",error);
                printf("output:%s\n\n",output);

                // 判断是否出现新的分支
                if(newfz(coverage, max_coverage)){//如果出现了新的分支
                    // 将当前种子与覆盖率传给optimize_seed函数，由该函数进行记录与优化种子
                    max_coverage = coverage;
                    save_test_result(seed, testcase, coverage, error, output , 0);
                    //更新种子
                    optimize_seed(seed, testcase);
                    //更换测试用例
                    mutate(seed, testcase);
                }else{
                    if(error[0]!='\0'){
                        save_test_result(seed, testcase, coverage, error, output , 1);
                    }
                    //更换测试用例
                    mutate(seed, testcase);
                }
            }else{//子进程execl调用失败，调用exit退出
                save_test_result(seed, testcase, coverage, error, output , 1);
                //更换测试用例
                mutate(seed, testcase);
            }
        } else if (WIFSIGNALED(status)) {//如果子进程是由于信号而终止的（例如通过调用 kill 函数），WIFSIGNALED(status) 将返回非零值。
            int signal_num = WTERMSIG(status);//可以使用 WTERMSIG(status) 获取子进程终止的信号编号。
            printf("%d\n",i);
            printf("seed:%s\n",seed);
            printf("testcase:%s\n",testcase);
            printf("coverage:%d\n",coverage);
            printf("error:%s\n",error);
            printf("output:%s\n",output);
            // printf("Child process terminated by signal: %d\n", signal_num);
        }
        } else {
            // 创建子进程失败的错误处理
            perror("fork failed");
            exit(1);
        }
	    signal_received=0;
        pid = fork();
    }

    return 0;
}
