#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

int main(int argc, char **argv)
{
        struct sockaddr_rc loc_addr = {0}, rem_addr = {0};
        char buf[1024] = {0};
        int s, client, bytes_read,revalue;
	unsigned long i=0;
        unsigned int opt = sizeof(rem_addr);
       //	char dest[18] = "00:15:83:0C:BF:EB";
        s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

        loc_addr.rc_family = AF_BLUETOOTH;
        loc_addr.rc_channel = 1;
	loc_addr.rc_bdaddr = *BDADDR_ANY;
       //	str2ba(dest, &loc_addr.rc_bdaddr);
        revalue = bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));
	if(revalue == 0)
	{
		fprintf(stderr, "bind succeded \n");
	}
	
        listen(s, 1);

        fprintf(stderr, "before accept \n");
        client = accept(s, (struct sockaddr *)&rem_addr, &opt);
        fprintf(stderr, "after accept \n");

        ba2str(&rem_addr.rc_bdaddr, buf);
        fprintf(stderr, "accepted connection from %s\n", buf);

	while(1)
	{
        	memset(buf, 0, sizeof(buf));

        	bytes_read = recv(client, buf, sizeof(buf), 0);
        	if (bytes_read>0)
        	{
			i++;
                	printf("%ld received [%s]\n", i, buf);
        	}
	}
        close(client);
        close(s);
        return 0;
}

