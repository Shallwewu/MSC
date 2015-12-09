#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

int main(int argc, char **argv)
{
	struct sockaddr_rc loc_addr = {0}, rem_addr = {0};
	char buf[1024] = {0};
	int s, client, bytes_read;
	unsigned int opt = sizeof(rem_addr);

	s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

	loc_addr.rc_family = AF_BLUETOOTH;
	loc_addr.rc_bdaddr = *BDADDR_ANY;      //any bluetooth device
	loc_addr.rc_channel = 1;
	bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));

	listen(s, 1);
        
        fprintf(stderr, "before accept \n");
	client = accept(s, (struct sockaddr *)&rem_addr, &opt);
	fprintf(stderr, "after accept \n");

	ba2str(&rem_addr.rc_bdaddr, buf);
	fprintf(stderr, "accepted connection from %s\n", buf);
	memset(buf, 0, sizeof(buf));

	bytes_read = recv(client, buf, sizeof(buf), 0);
	if (bytes_read>0)
	{
		printf("received [%s]\n", buf);
	}

	close(client);
	close(s);
	return 0;
}
