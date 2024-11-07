#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <sstream>


int main(){
	char filePath[100];
        sprintf(filePath, "/sys/class/gpio/gpio960/value");
	char spec_temp_buffer[50 + 1] = {0};
        int file = open(filePath, O_RDONLY);
	struct timespec tptemp;
while(1){
	int resnum = read(file, spec_temp_buffer, 50 + 1);
        if (resnum == -1)
        {
            perror("read()");
        }
	clock_gettime(CLOCK_MONOTONIC, &tptemp);
	printf("Data: %s  %lld %lld \n", spec_temp_buffer, (long long)tptemp.tv_sec, (long long)tptemp.tv_nsec);
	lseek(file, 0, SEEK_SET);
	
	}
return 0;
}


