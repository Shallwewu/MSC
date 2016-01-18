/*
 * Test for receiving data for USB host side of MSC
 * Test in linux PC
 * By Shallwe Woo
 * 2016-1-4
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <sys/ioctl.h>

int main(int argc, char const *argv[])
{
	unsigned int fd;
	char strbuf[20] ={0};
	unsigned char inbuffer[512] = {0};
	unsigned int count; 
	int i;
	
	sprintf(strbuf, "/dev/msc-usb%d", 0);
	printf("usb:%s",strbuf);
	fd = open(strbuf, O_RDONLY);	
	if (fd < 0) 
	{
		perror("open usb error");
		return -1;
	}
	else
	{
		printf("open msc-usb succeed!");
	}
	
	count = read(fd, inbuffer, 100);
	if (count < 0) 
	{
			fprintf(stderr, "read0 error code:%d; %s\n", errno, strerror(errno));
			//perror("read");
			return -1;
	}  

	printf("receive data:%d", inbuffer[0]);

}