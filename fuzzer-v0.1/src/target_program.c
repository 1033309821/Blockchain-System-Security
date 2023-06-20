#include<stdio.h>
#include<stdlib.h>
int main(int argc, char **argv){
	char ptr[20];
	if(argc>1){
		FILE * fp = fopen(argv[1],"r");
		fgets(ptr, sizeof(ptr), fp);
	}
	else{
		fgets(ptr, sizeof(ptr), stdin);
	}
	printf("%s",ptr);
	if(ptr[0]== '0'){
		if(ptr[1]== 'x'){
			if(ptr[2]== 'a'){
				if(ptr[3]== 'a'){
						abort();
					}else printf("%c",ptr[3]);
			}else printf("%c",ptr[2]);
		}
		else printf("%c",ptr[1]);
	}
	else printf("%c",ptr[0]);
	return 0;
}
