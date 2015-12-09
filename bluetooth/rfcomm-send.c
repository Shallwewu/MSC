#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

int main(int argc, char **argv)
{
	struct sockaddr_rc addr = {0};
	int s, status, revalue;
	unsigned long i=0;
	char dest[18] = "00:1A:7D:DA:71:13";

	s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

	addr.rc_family = AF_BLUETOOTH;
	addr.rc_channel = 1;
	str2ba(dest, &addr.rc_bdaddr);

	fprintf(stderr,"before connect \n");
	status = connect(s, (struct sockaddr *)&addr, sizeof(addr));

	if(status < 0)
	{
		fprintf(stderr,"connect error! \n");
		return 0;
	}
	fprintf(stderr,"connect succeded! \n");
	while(1)
	{
		revalue = send(s, "hello!", 6, 0);
		if(revalue < 0)
		{
			fprintf(stderr,"send error! \n");
			break;
		}
		else
		{
			i++;
			fprintf(stderr, "%ld : send succeded\n", i);
		}
	}

	close(s);
	return 0;
}
