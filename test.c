#include "tfs.h"

#include "stdio.h"

int main(void){
	tfs_inittarfile("./test.tar");
	FILE* fp = fopen("@/root/minicom.log", "r");
	if(!fp){
		puts("fopen error");
		return 1;
	}
	char buf[64] = {};
	int count = fread(buf, 64, 16, fp);
	printf("count = %d, size = %ld\n", count, ((TFS_FILE*)fp)->data_len);
	puts(buf);
	fclose(fp);
	tfs_deinit();
	return 0;
}
