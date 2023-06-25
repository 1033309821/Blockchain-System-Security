# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <signal.h>

int vuln(char *str){
    int len = strlen(str);
    if(str[0] == 'A' && len == 66){
        raise(SIGSEGV);
        //如果输入的字符串的首字母为A并且长度为66，则异常退出
    } 
    else if(str[0] == 'f' && len == 6){
        raise(SIGSEGV);
    } 
    else{
        printf("it is good!\n");
    }

    return 0;
}

int main(int argc, char *argv[]){
    char buf[100] = {0};
    gets(buf);  //存在栈溢出漏洞
    printf(buf);    //存在格式化字符串漏洞
    printf("\t");
    vuln(buf);

    return 0;
}
