#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#include <sys/wait.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ERROR_SIZE 1024
#define MAX_SEED_SIZE 5     //控制哈希值的长度
#define MAX_COVERAGE_SIZE 1024

/* Maximum size of input file, in bytes (keep under 100MB): */

#define MAX_FILE (1 * 1024 * 1024)

#define ck_free DFL_ck_free
#define ck_alloc DFL_ck_alloc

#ifdef MESSAGES_TO_STDOUT
#  define SAYF(x...)    printf(x)
#else 
#  define SAYF(x...)    fprintf(stderr, x)
#endif /* ^MESSAGES_TO_STDOUT */

#define alloc_printf(_str...) ({ \
    unsigned char* _tmp; \
    int _len = snprintf(NULL, 0, _str); \
    if (_len < 0) FATAL("Whoa, snprintf() fails?!"); \
    _tmp = ck_alloc(_len + 1); \
    snprintf((char*)_tmp, _len + 1, _str); \
    _tmp; \
  })

#define ACTF(x...) do { \
    SAYF(cLBL "[*] " cRST x); \
    SAYF(cRST "\n"); \
  } while (0)

#define FATAL(x...) do { \
    SAYF(bSTOP RESET_G1 CURSOR_SHOW cRST cLRD "\n[-] PROGRAM ABORT : " \
         cBRI x); \
    SAYF(cLRD "\n         Location : " cRST "%s(), %s:%u\n\n", \
         __FUNCTION__, __FILE__, __LINE__); \
    exit(1); \
  } while (0)

static unsigned queued_paths,              /* Total number of queued testcases */
                queued_variable,           /* Testcases with variable behavior */
                queued_at_start,           /* Total number of initial inputs   */
                queued_discovered,         /* Items discovered during this run */
                queued_imported,           /* Items imported via -S            */
                queued_favored,            /* Paths deemed favorable           */
                queued_with_cov,           /* Paths with new coverage bytes    */
                pending_not_fuzzed,        /* Queued but not done yet          */
                pending_favored,           /* Pending favored paths            */
                cur_skipped_paths,         /* Abandoned inputs in cur cycle    */
                cur_depth,                 /* Current path depth               */
                max_depth,                 /* Max path depth                   */
                useless_at_start,          /* Number of useless starting paths */
                var_byte_count,            /* Bitmap bytes with var behavior   */
                current_entry,             /* Current queue entry ID           */
                havoc_div = 1;             /* Cycle count divisor for havoc    */

static unsigned long long   total_crashes,             /* Total number of crashes          */
                            unique_crashes,            /* Crashes with unique signatures   */
                            total_tmouts,              /* Total number of timeouts         */
                            unique_tmouts,             /* Timeouts with unique signatures  */
                            unique_hangs,              /* Hangs with unique signatures     */
                            total_execs,               /* Total execve() calls             */
                            slowest_exec_ms,           /* Slowest testcase non hang in ms  */
                            start_time,                /* Unix start time (ms)             */
                            last_path_time,            /* Time for most recent path (ms)   */
                            last_crash_time,           /* Time for most recent crash (ms)  */
                            last_hang_time,            /* Time for most recent hang (ms)   */
                            last_crash_execs,          /* Exec counter at last crash       */
                            queue_cycle,               /* Queue round counter              */
                            cycles_wo_finds,           /* Cycles without any new paths     */
                            trim_execs,                /* Execs done to trim input files   */
                            bytes_trim_in,             /* Bytes coming into the trimmer    */
                            bytes_trim_out,            /* Bytes coming outa the trimmer    */
                            blocks_eff_total,          /* Blocks subject to effector maps  */
                            blocks_eff_select;         /* Blocks selected as fuzzable      */

static unsigned char *in_dir,
                     *out_dir,
                     *out_file,
                     *target_path,
                     *shuffle_queue;

struct dirent
{
  long            d_ino;		/* Always zero. */
  unsigned short  d_reclen;		/* Always sizeof struct dirent. */
  unsigned short  d_namlen;		/* Length of name in d_name. */
  unsigned        d_type;		/* File attributes */
  char            d_name[FILENAME_MAX]; /* File name. */
};

struct queue_entry {

    unsigned char* fname;                          /* File name for the test case      */
    unsigned len;                            /* Input length                     */

    unsigned char cal_failed,                     /* Calibration failed?              */
                    trim_done,                      /* Trimmed?                         */
                    was_fuzzed,                     /* Had any fuzzing done yet?        */
                    passed_det,                     /* Deterministic stages passed?     */
                    has_new_cov,                    /* Triggers new coverage?           */
                    var_behavior,                   /* Variable behavior?               */
                    favored,                        /* Currently favored?               */
                    fs_redundant;                   /* Marked as redundant in the fs?   */

    unsigned bitmap_size,                    /* Number of bits set in bitmap     */
             exec_cksum;                     /* Checksum of the execution trace  */

    unsigned long long  exec_us,                        /* Execution time (us)              */
                        handicap,                       /* Number of queue cycles behind    */
                        depth;                          /* Path depth                       */

    unsigned char* trace_mini;                     /* Trace bytes, if kept             */
    unsigned tc_ref;                         /* Trace bytes ref count            */

    struct queue_entry *next,           /* Next element, if any             */
                        *next_100;       /* 100 elements ahead               */

};

static struct queue_entry *queue,     /* Fuzzing queue (linked list)      */
                          *queue_cur, /* Current offset within the queue  */
                          *queue_top, /* Top of the list                  */
                          *q_prev100; /* Previous 100 marker              */

static void usage(unsigned char* argv0) {

  SAYF("\n%s [ options ] -- /path/to/fuzzed_app [ ... ]\n\n"

       "Required parameters:\n\n"

       "  -i dir        - input directory with test cases\n"
       "  -o dir        - output directory for fuzzer findings\n\n"

       "Execution control settings:\n\n"

       "  -f file       - location read by the fuzzed program (stdin)\n"
       "  -t msec       - timeout for each run (auto-scaled, 50-%u ms)\n"
       "  -m megs       - memory limit for child process (%u MB)\n"
       "  -Q            - use binary-only instrumentation (QEMU mode)\n\n"     
 
       "Fuzzing behavior settings:\n\n"

       "  -d            - quick & dirty mode (skips deterministic steps)\n"
       "  -n            - fuzz without instrumentation (dumb mode)\n"
       "  -x dir        - optional fuzzer dictionary (see README)\n\n"

       "Other stuff:\n\n"

       "  -T text       - text banner to show on the screen\n"
       "  -M / -S id    - distributed mode (see parallel_fuzzing.txt)\n"
       "  -C            - crash exploration mode (the peruvian rabbit thing)\n"
       "  -V            - show version number and exit\n\n"
       "  -b cpu_id     - bind the fuzzing process to the specified CPU core\n\n"

       "For additional tips, please consult %s/README.\n\n",

       argv0, EXEC_TIMEOUT, MEM_LIMIT, doc_path);

  exit(1);

}

/* Get unix time in milliseconds */

static unsigned long long get_cur_time(void) {

  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  return (tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000);

}

/* Shuffle an array of pointers. Might be slightly biased. */
static void shuffle_ptrs(void** ptrs, u32 cnt) {

  u32 i;

  for (i = 0; i < cnt - 2; i++) {

    u32 j = i + UR(cnt - i);
    void *s = ptrs[i];
    ptrs[i] = ptrs[j];
    ptrs[j] = s;

  }

}

/* Append new test case to the queue. */

static void add_to_queue(unsigned char* fname, unsigned len, unsigned char passed_det) {

    struct queue_entry* q = ck_alloc(sizeof(struct queue_entry));

    q->fname        = fname;
    q->len          = len;
    q->depth        = cur_depth + 1;
    q->passed_det   = passed_det;

    if (q->depth > max_depth) max_depth = q->depth;

    if (queue_top) {

        queue_top->next = q;
        queue_top = q;

    } else q_prev100 = queue = queue_top = q;

    queued_paths++;
    pending_not_fuzzed++;

    cycles_wo_finds = 0;

    /* Set next_100 pointer for every 100th element (index 0, 100, etc) to allow faster iteration. */
    if ((queued_paths - 1) % 100 == 0 && queued_paths > 1) {

        q_prev100->next_100 = q;
        q_prev100 = q;

    }

    last_path_time = get_cur_time();

}

// 根据输入seed生成testcase
static void read_testcases(void) {
    struct dirent **nl;
    int nl_cnt;
    int i;
    unsigned char* fn;

    /* Auto-detect non-in-place resumption attempts. */

    fn = alloc_printf("%s/queue", in_dir);
    if (!access(fn, F_OK)) in_dir = fn; else ck_free(fn);

    ACTF("Scanning '%s'...", in_dir);

    nl_cnt = scandir(in_dir, &nl, NULL, alphasort);

    if (shuffle_queue && nl_cnt > 1) {
        ACTF("Shuffling queue...");
        shuffle_ptrs((void**)nl, nl_cnt);
    }
    
    for (i = 0; i < nl_cnt; i++) {

        struct stat st;

        unsigned char* fn = alloc_printf("%s/%s", in_dir, nl[i]->d_name);
        unsigned char* dfn = alloc_printf("%s/.state/deterministic_done/%s", in_dir, nl[i]->d_name);

        unsigned char passed_det = 0;

        free(nl[i]); /* not tracked */
 
        /* This also takes care of . and .. */

        if (!S_ISREG(st.st_mode) || !st.st_size || strstr(fn, "/README.testcases")) {
            ck_free(fn);
            ck_free(dfn);
            continue;
        }

        /* Check for metadata that indicates that deterministic fuzzing
            is complete for this entry. We don't want to repeat deterministic
            fuzzing when resuming aborted scans, because it would be pointless
            and probably very time-consuming. */

        if (!access(dfn, F_OK)) passed_det = 1;
        ck_free(dfn);

        add_to_queue(fn, st.st_size, passed_det);

    }

    free(nl); /* not tracked */

    last_path_time = 0;
    queued_at_start = queued_paths;

}

// 根据种子生成随机输入，并执行目标程序
int run_target_program(char *seed, char *input, int input_size, char *output, int output_size, char *error) {
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
    snprintf(input, input_size, "echo %s | ./target_program", seed);

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
void save_test_result(char *seed, int coverage, char *error, char *output) {
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
    	  fprintf(fp, "Coverage: %d\n", coverage);
    	  fprintf(fp, "Error: %s\n", error);
    	  fprintf(fp, "Output: %s\n", output);
	  fclose(fp);
     }

}

// 优化种子
void optimize_seed(char *seed, int coverage) {
    // 在这里根据测试结果优化种子，目前还是使用了初始化种子的算法
    const char charset[] = "0123456789abcdef";
    seed[0] = '0';
    seed[1] = 'x';
    int i;
    for (i = 2; i < MAX_SEED_SIZE-1; ++i) {
        seed[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    seed[MAX_SEED_SIZE-1] = '\0';
}

int main(int argc, char *argv[]) {
    char seed[MAX_SEED_SIZE];
    char input[MAX_INPUT_SIZE];
    char output[MAX_INPUT_SIZE];
    char error[MAX_ERROR_SIZE];
    int opt;
    int coverage = 0;
    int max_coverage = 0;
    int i=0;
    
    while ((opt = getopt(argc, argv, "+i:o:f:m:b:t:T:dnCB:S:M:x:QV")) > 0)

        switch (opt) {
            case 'i': /* input dir */

                if (in_dir) FATAL("Multiple -i options not supported");
                in_dir = optarg;

                if (!strcmp(in_dir, "-")) in_place_resume = 1;

                break;

            case 'o': /* output dir */

                if (out_dir) FATAL("Multiple -o options not supported");
                out_dir = optarg;
                break;
        }

    if (optind == argc || !in_dir || !out_dir) usage(argv[0]);

    if (getenv("AFL_SHUFFLE_QUEUE")) shuffle_queue = 1;

    read_testcases();

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

    start_time = get_cur_time();

    // 该部分涉及的是根据输入的seed生成testcase并执行target_program, 正在完善中
    //perform_dry_run(argv);

    //strcpy(seed,"0xaa");
    for(;i<100;i++) {

        // 根据种子生成随机输入，并执行目标程序，获取代码覆盖率和输出
        coverage = run_target_program(seed, input, MAX_INPUT_SIZE, output, MAX_INPUT_SIZE, error);

        // 将测试结果保存到文件
        save_test_result(seed, coverage, error, output);

        //将当前种子与覆盖率传给optimize_seed函数，由该函数进行记录与优化种子
        optimize_seed(seed, coverage);

        // 更新最大代码覆盖率和最优种子
        // if(coverage > max_coverage) {
        //     max_coverage = coverage;
        //     fprintf(stdout, "New maximum coverage: %d\n", max_coverage);
        // }

        // 不断生成新的种子并重复测试，直到达到所需的测试次数或代码覆盖率达到目标值
        // if(max_coverage >= 100) {
        //     break;
        // }
   }

    return 0;
}

