/*
 * Motion Sensor:MPU6050
 * Configure and read data with I2C module
 * This is a driver of I2C,also of Motion Sensor 
 * by Wu Xianwei
 * 2015 11-20
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define	SMPLRT_DIV		0x19	//陀螺仪采样率，典型值：0x07(125Hz)
#define	CONFIG			0x1A	//低通滤波频率，典型值：0x06(5Hz)
#define	GYRO_CONFIG		0x1B	//陀螺仪自检及测量范围,典型值：0x18(不自检，2000deg/s)
#define	ACCEL_CONFIG	0x1C	//加速计自检、测量范围及高通滤波频率，典型值：0x01(不自检，2G，5Hz)
#define	PWR_MGMT_1		0x6B	//电源管理，典型值：0x00(正常启用)
#define	SlaveAddress	0x68	//IIC从地址
#define	WHO_AM_I		0x75

static int MPU6050_init(struct i2c_client *client)
{
	int ret;
	struct i2c_adapter	*adapter = to_i2c_adapter(client->dev.parent);

	ret = i2c_smbus_write_byte_data(client,	PWR_MGMT_1, 0x00);
	if (ret < 0) 
	{
		dev_err(&client->dev, "array: PWR_MGMT_1 write failed");
		return ret;
	}
	ret = i2c_smbus_write_byte_data(client,	SMPLRT_DIV, 0x07);
	if (ret < 0) 
	{
		dev_err(&client->dev, "array: SMPLRT_DIV write failed");
		return ret;
	}
	ret = i2c_smbus_write_byte_data(client,	CONFIG, 0x06);
	if (ret < 0) 
	{
		dev_err(&client->dev, "array: CONFIG write failed");
		return ret;
	}
	ret = i2c_smbus_write_byte_data(client,	GYRO_CONFIG, 0x18);
	if (ret < 0) 
	{
		dev_err(&client->dev, "array: GYRO_CONFIG write failed");
		return ret;
	}
	ret = i2c_smbus_write_byte_data(client,	ACCEL_CONFIG, 0x01);
	if (ret < 0) 
	{
		dev_err(&client->dev, "array: ACCEL_CONFIG write failed");
		return ret;
	}
	dev_info(&adapter->dev, "MPU6050 initial succeed\n");
	return ret;
}

static int MPU6050_probe(struct i2c_client *client)
{
	struct i2c_adapter	*adapter = to_i2c_adapter(client->dev.parent);
	int			ret;	
	u8 id;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
	{
		dev_err(&adapter->dev,
			"MPU6050: I2C-Adapter doesn't support SMBUS\n");
		return -EIO;
	}

	id  = i2c_smbus_read_byte_data(client, WHO_AM_I);

	if (id == SlaveAddress)
	{
		dev_info(&adapter->dev, "MPU6050 Probed\n");
	}
	else
	{
		dev_err(&client->dev,
			"MPU6050 ID error %x\n", id);
		ret = -ENODEV;
		return ret;
	}

	ret = MPU6050_init(client);
	
	return ret;
}

static int MPU6050_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id MPU6050_id[] = {
	{ "mpu6050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, Motioni2c_id);

static struct i2c_driver MPU6050_driver = {
	.driver = {
		.name = "mpu6050",
	},
	.probe    = MPU6050_probe,
	.remove   = MPU6050_remove,
	.id_table = MPU6050_id,
};

module_i2c_driver(MPU6050_driver);

MODULE_DESCRIPTION("mpu6050 driver");
MODULE_AUTHOR("Wu Xianwei");
MODULE_LICENSE("GPL v2");
