/*
 * Test for sending data for USB device side of MSC
 * Test in a5 arm-linux
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

static char dev_name[]= "/dev/usb_msc";
int main(int argc, char const *argv[])
{
	int fd;
	int count;
	unsigned char inbuffer[] = {1,2,3,4,5,6,7,8,9};

	fd = open(dev_name, O_RDWR);

	printf("fd = %d", fd);
	if(!fd)
	{
        	fprintf(stderr, "Cannot open '%s': %d, %s \n", dev_name, errno, strerror(errno));
        	return 0;
	}
	else
	{
		printf("Open %s succeed! \n", dev_name);
	}

	count = write(fd, inbuffer, sizeof(inbuffer));

	if (count < 0) 
	{
			printf("write error code:%d; %s\n", errno, strerror(errno));
			//perror("read");
			return -1;
	}

	printf("actually write count=%d", count);

	close(fd);
}
