/*
 * USB host side
 * test for a5 usb device(usb_mscdevice)
 * Bulk Transfer
 * devised from usb-skeleton
 * By Shallwe Woo
 * 2016-1-4
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>

/* Define these values to match your devices */
#define USB_MSC_VENDOR_ID	0x05AC
#define USB_MSC_PRODUCT_ID	0x2222

/* table of devices that work with this driver */
static const struct usb_device_id msc_supported_table[] = {
	{ USB_DEVICE(USB_MSC_VENDOR_ID, USB_MSC_PRODUCT_ID) },
	{ }					/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, msc_supported_table);

/* usb设备的次设备号起始 */
#define USB_MSC_MINOR_BASE 	199

/* Structure to hold all of our device specific stuff */
struct usb_msc {
	struct usb_device	*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
//	struct semaphore	limit_sem;		/* limiting the number of writes in progress */
//	struct usb_anchor	submitted;		/* in case we need to retract our submissions */
	struct urb		*bulk_in_urb;		/* the urb to read data with */
	unsigned char           *bulk_in_buffer;	/* the buffer to receive data */
	size_t			bulk_in_size;		/* the size of the receive buffer */
	size_t			bulk_in_filled;		/* number of bytes in the buffer */
	size_t			bulk_in_copied;		/* already copied to user space */
	__u8			bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
//	__u8			bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
	int			errors;			/* the last request tanked */
//	int			open_count;		/* count the number of openers */
//	bool			ongoing_read;		/* a read is going on */
//	bool			processed_urb;		/* indicates we haven't processed the urb */
	spinlock_t		err_lock;		/* lock for errors */
	struct kref		kref;
	struct mutex		io_mutex;		/* synchronize I/O with disconnect */
	struct completion	bulk_in_completion;	/* to wait for an ongoing read */
};

#define to_msc_dev(d) container_of(d, struct usb_msc, kref)

static struct usb_driver msc_driver;

static void msc_delete(struct kref *kref)
{
	struct usb_msc *dev = to_msc_dev(kref);

	printk("(msc_delete)  runs! \n");
	usb_free_urb(dev->bulk_in_urb);
	usb_put_dev(dev->udev);
	kfree(dev->bulk_in_buffer);
	kfree(dev);
}

static int msc_open(struct inode *inode, struct file *file)
{
	struct usb_msc *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	printk("(msc_open)  runs! \n");
	subminor = iminor(inode);

	interface = usb_find_interface(&msc_driver, subminor);
	if (!interface) {
		printk("%s - error, can't find device for minor %d",
		     __func__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		printk("(msc_open)  !dev! \n");
		retval = -ENODEV;
		goto exit;
	}

	/* increment our usage count for the device */
	kref_get(&dev->kref);

	/* lock the device to allow correctly handling errors
	 * in resumption */
	mutex_lock(&dev->io_mutex);

/*	if (!dev->open_count++) {
		retval = usb_autopm_get_interface(interface);
			if (retval) {
				printk("(msc_open)  retval! \n");
				dev->open_count--;
				mutex_unlock(&dev->io_mutex);
				kref_put(&dev->kref, msc_delete);
				goto exit;
			}
	}*/  /* else { //uncomment this block if you want exclusive open
		retval = -EBUSY;
		dev->open_count--;
		mutex_unlock(&dev->io_mutex);
		kref_put(&dev->kref, skel_delete);
		goto exit;
	} */
	/* prevent the device from being autosuspended */

	/* save our object in the file's private structure */
	file->private_data = dev;
	mutex_unlock(&dev->io_mutex);

exit:
	return retval;
}

static int msc_release(struct inode *inode, struct file *file)
{
	struct usb_msc *dev;

	printk("(msc_release)  runs! \n");
	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	/* allow the device to be autosuspended */
//	mutex_lock(&dev->io_mutex);
//	if (!--dev->open_count && dev->interface)
//		usb_autopm_put_interface(dev->interface);
//	mutex_unlock(&dev->io_mutex);

	/* decrement the count on our device */
	kref_put(&dev->kref, msc_delete);
	return 0;
}

static int msc_flush(struct file *file, fl_owner_t id) {
	return 0;
}

static void msc_read_bulk_callback(struct urb *urb)
{
	struct usb_msc *dev;

	dev = urb->context;
	printk("(msc_read_bulk_callback)  runs! \n");
	spin_lock(&dev->err_lock);
	/* sync/async unlink faults aren't errors */
	if (urb->status)
	{
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			printk("%s - nonzero write bulk status received: %d",
			    __func__, urb->status);

		dev->errors = urb->status;
	}
	else
	{
		dev->bulk_in_filled = urb->actual_length;
		printk("(msc_read_bulk_callback) urb->status OK, urb->actual_length=%d ! \n", urb->actual_length);
	}
	
	printk("(msc_read_bulk_callback) dev->bulk_in_buffer =%u,    %u,   %u,   %u ,   %u ,   %u ,   %u ,   %u ,   %u  ! \n", (dev->bulk_in_buffer)[0], (dev->bulk_in_buffer)[1], (dev->bulk_in_buffer)[2], (dev->bulk_in_buffer)[3],(dev->bulk_in_buffer)[4],(dev->bulk_in_buffer)[5],(dev->bulk_in_buffer)[6],(dev->bulk_in_buffer)[7],(dev->bulk_in_buffer)[8]);
	//dev->ongoing_read = 0;
	spin_unlock(&dev->err_lock);

	complete(&dev->bulk_in_completion);
}

/** Block Read**/
static ssize_t msc_read(struct file *file, char __user *user_buf, size_t count, loff_t *pos) {

	struct usb_msc *dev = (struct usb_msc *)file->private_data;
	struct urb  *urb = dev->bulk_in_urb;
	//unsigned int i;
	int retval = 0;
	//unsigned int tmp;

	//dev->isoc_in_buffer = usb_alloc_coherent(dev->udev, )
	printk("(msc_read)  runs! \n");
	
	mutex_lock(&dev->io_mutex);

	memset(dev->bulk_in_buffer, 0, dev->bulk_in_size);
	usb_fill_bulk_urb(urb, dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
				dev->bulk_in_buffer, dev->bulk_in_size, msc_read_bulk_callback, dev);
	//urb->transfer_dma = dev->bulk_in_buffer_dma;
	//urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;    //两句一起 要么都有 要么都没有

	/* submit urb */
	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval < 0) {
		 dev_err(&dev->interface->dev,
		 			"%s - failed submitting read urb, error %d\n",
                    __func__, retval);
		goto out;
	}
	else
	{
	    printk("(msc_read)  submit urb succeed! \n");
	}
	/* submit urb */
	/*wait for read operation complete*/
	wait_for_completion(&dev->bulk_in_completion);
	//while (dev->isoc_in_filled != SEMG_FRAME_SIZE)
	//	msleep(1);
	//goto out;
	printk("(msc_read)  read_callback back! \n");
	spin_lock(&dev->err_lock);
	/* errors must be reported */
	retval = dev->errors;
	if (retval < 0) {
		/* any error is reported once */
		dev->errors = 0;
		/* to preserve notifications about reset */
		retval = (retval == -EPIPE) ? retval : -EIO;
		/* no data to deliver */
		dev->bulk_in_filled = 0;
		spin_unlock(&dev->err_lock);
		/* report it */
		goto out;
	}
	spin_unlock(&dev->err_lock);

	//  只提供机制，不应该提供策略
	// if((dev->bulk_in_filled != SEMG_FRAME_SIZE) || (SEMG_FRAME_SIZE != count)) {
	// 	dev_err(&dev->interface->dev, "total received size :%zd\n", dev->bulk_in_filled);
	// 	retval = -EINVAL;
	// 	goto out;
	// }

	if (count > dev->bulk_in_filled)
		count = dev->bulk_in_filled;
	printk("(msc_read) count=%d \n", count);
	if(copy_to_user(user_buf, dev->bulk_in_buffer, count)) {
		printk("(msc_read)  copy to user failed! \n");
		retval = -EFAULT;
		goto out;
	}

	retval = count;

out:
	mutex_unlock(&dev->io_mutex);
	return retval;
}

static ssize_t msc_write(struct file *file, const char *user_buffer, size_t count, loff_t *ppos)
{
	return 0;
}

static const struct file_operations msc_fops = {
	.owner =	THIS_MODULE,
	.read =		msc_read,
	.write =	msc_write,
	.open =		msc_open,
	.release =	msc_release,
	.flush =	msc_flush,
};

static struct usb_class_driver msc_class = {
	.name = "msc-usb%d",		/* %d为设备编号*/
	.fops = &msc_fops,
	.minor_base = USB_MSC_MINOR_BASE,
};

static int msc_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_msc *dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	size_t buffer_size;
	int i;
	int retval = -ENOMEM;

	printk("(msc_probe)  runs! \n");
	/* allocate memory for our device state and initialize it */
	dev = (struct usb_msc *)kzalloc(sizeof(struct usb_msc), GFP_KERNEL);
	if (!dev) {
		printk("Out of memory \n");
		goto error;
	}
	kref_init(&dev->kref);
//	sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);
	mutex_init(&dev->io_mutex);
	spin_lock_init(&dev->err_lock);
//	init_usb_anchor(&dev->submitted);
	init_completion(&dev->bulk_in_completion);

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* set up the endpoint information */
	/* use only the first bulk-in and bulk-out endpoints */
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (!dev->bulk_in_endpointAddr &&
		    usb_endpoint_is_bulk_in(endpoint)) {
			/* we found a bulk in endpoint */
			buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
			dev->bulk_in_size = buffer_size;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				printk("Could not allocate bulk_in_buffer \n");
				goto error;
			}
			dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
			if (!dev->bulk_in_urb) {
				printk("Could not allocate bulk_in_urb \n");
				goto error;
			}
		}

	//	if (!dev->bulk_out_endpointAddr &&
	//	    usb_endpoint_is_bulk_out(endpoint)) {
			/* we found a bulk out endpoint */
	//		dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
	//	} 
	}
	if (!(dev->bulk_in_endpointAddr)) {
		printk("Could not find bulk-in endpoints \n");
		goto error;
	}
	printk("(msc_probe)  dev->bulk_in_endpointAddr=%d \n", dev->bulk_in_endpointAddr);
	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &msc_class);
	if (retval) {
		/* something prevented us from registering this driver */
		printk("Not able to get a minor for this device. \n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */
	dev_info(&interface->dev,
		 "USB msc device now attached to USBMSC-%d",
		 interface->minor);
	printk("(msc_probe)  probe succeed!  \n");
	return 0;

error:
	if (dev)
		/* this frees allocated memory */
		kref_put(&dev->kref, msc_delete);
	return retval;
}

static void msc_disconnect(struct usb_interface *interface)
{
	struct usb_msc *dev;
	int minor = interface->minor;

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &msc_class);

	/* prevent more I/O from starting */
	mutex_lock(&dev->io_mutex);
	dev->interface = NULL;
	mutex_unlock(&dev->io_mutex);

//	usb_kill_anchored_urbs(&dev->submitted);

	/* decrement our usage count */
	kref_put(&dev->kref, msc_delete);

	dev_info(&interface->dev, "USB MSC #%d now disconnected", minor);
	printk("(msc_disconnect)  runs! \n");
}

static struct usb_driver msc_driver = {
//	.owner = THIS_MODULE,
	.name = "usb_msc",
	.id_table = msc_supported_table,
	.probe = msc_probe,
	.disconnect = msc_disconnect
};

static int __init usb_msc_init(void)
{
	int result;

	/* register this driver with the USB subsystem */
	result = usb_register(&msc_driver);
	if (result)
		pr_err("usb_register failed. Error number %d", result);
	else
	{
	  printk(KERN_EMERG "(usb_msc_init)  usb_register succeed! \n");
	}
	
	return result;
}

static void __exit usb_msc_exit(void)
{
	usb_deregister(&msc_driver);
	printk("(usb_msc_exit)  usb_deregister! \n");
}

module_init(usb_msc_init);
module_exit(usb_msc_exit);

MODULE_AUTHOR("Shallwe Woo");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0:0.1");
MODULE_DESCRIPTION("Used for receiving instruction from MSC");