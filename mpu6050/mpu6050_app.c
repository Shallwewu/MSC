/*
 * mpu6050 data collect by i2c
 * application code
 * by Wu Xianwei
 * 2015-11-24
 */

#include <stdio.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define	ACCEL_XOUT_H	0x3B
#define	ACCEL_XOUT_L	0x3C
#define	ACCEL_YOUT_H	0x3D
#define	ACCEL_YOUT_L	0x3E
#define	ACCEL_ZOUT_H	0x3F
#define	ACCEL_ZOUT_L	0x40
#define	TEMP_OUT_H		0x41
#define	TEMP_OUT_L		0x42
#define	GYRO_XOUT_H		0x43
#define	GYRO_XOUT_L		0x44	
#define	GYRO_YOUT_H		0x45
#define	GYRO_YOUT_L		0x46
#define	GYRO_ZOUT_H		0x47
#define	GYRO_ZOUT_L		0x48
#define	SlaveAddress	0x68	//IIC写入时的地址字节数据，+1为读取

unsigned short dataaddr[6] = {ACCEL_XOUT_H, ACCEL_YOUT_H, ACCEL_ZOUT_H, GYRO_XOUT_H, GYRO_YOUT_H, GYRO_ZOUT_H};

int mpu6050_data[6] = {0, 0, 0, 0, 0, 0};

static char dev_name[]= "/dev/i2c-0";

int main(int argc, char **argv)
{
	struct i2c_rdwr_ioctl_data mpu6050_queue;
	unsigned int fd;
	unsigned int slave_address, reg_address;
	int ret,i2cloop;
    
	fd = open(dev_name, O_RDWR);

	if(!fd)
	{
        fprintf(stderr, "Cannot open '%s': %d, %s \n", dev_name, errno, strerror(errno));
        return 0;
	}
	else
	{
		printf("Open %s succeed! \n", dev_name);
	}

	mpu6050_queue.nmsgs = 2;
	mpu6050_queue.msgs = (struct i2c_msg*)malloc(mpu6050_queue.nmsgs*sizeof(struct i2c_msg));
	(mpu6050_queue.msgs[0]).buf = (unsigned char*)malloc(1);
	(mpu6050_queue.msgs[1]).buf = (unsigned char*)malloc(1);
	if(!mpu6050_queue.msgs)
	{
		printf("MPU6050 Memory alloc error\n");
		close(fd);
		return 0;
	}

	ioctl(fd, I2C_TIMEOUT, 2);
	ioctl(fd, I2C_RETRIES, 1);
    while(1){
	for (i2cloop = 0; i2cloop < 6; ++i2cloop)
	{
		char H, L;
	 //	usleep(50);
		(mpu6050_queue.msgs[0]).len = 1;
		(mpu6050_queue.msgs[0]).addr = SlaveAddress;
		(mpu6050_queue.msgs[0]).flags = 0;
		(mpu6050_queue.msgs[0]).buf[0]= dataaddr[i2cloop];
		(mpu6050_queue.msgs[1]).len = 1;
		(mpu6050_queue.msgs[1]).addr = SlaveAddress;
		(mpu6050_queue.msgs[1]).flags = I2C_M_RD;
		(mpu6050_queue.msgs[1]).buf[0] = 0;		
		ret = ioctl(fd, I2C_RDWR, (unsigned long)&mpu6050_queue);
		if (ret < 0)
		{
			printf("mpu6050 ioctl error");
		}
		H = (mpu6050_queue.msgs[1]).buf[0];
       // usleep(50);
		(mpu6050_queue.msgs[0]).len = 1;
		(mpu6050_queue.msgs[0]).addr = SlaveAddress;
		(mpu6050_queue.msgs[0]).flags = 0;
		(mpu6050_queue.msgs[0]).buf[0]= dataaddr[i2cloop]+1;
		(mpu6050_queue.msgs[1]).len = 1;
		(mpu6050_queue.msgs[1]).addr = SlaveAddress;
		(mpu6050_queue.msgs[1]).flags = I2C_M_RD;
		(mpu6050_queue.msgs[1]).buf[0] = 0;		
		ret = ioctl(fd, I2C_RDWR, (unsigned long)&mpu6050_queue);
		if(ret < 0)
		{
			printf("mpu6050 ioctl error");
		}	
		L = (mpu6050_queue.msgs[1]).buf[0]; 
		mpu6050_data[i2cloop] = (H<<8)+L;

		printf("mpu6050_data[%x] = %x \n", i2cloop, mpu6050_data[i2cloop]);
	}
    }
	close(fd);
	return;
}
