/*
 * MSC USB device 
 * USB2.0
 * As a character device which application could use to 
 *  read and write with USB protocol
 * Transport mode:Bulk transport
 * By Wu Xianwei
 * 2015-12-25
 */

#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/types.h> /* size_t */
#include <linux/errno.h> /* error codes */
#include <asm/system.h>
#include <asm/io.h>
#include <linux/sched.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"

//#include "composite.c"
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

static const char shortname[] = "MSC_USB";
static const char sendinstruc[] = "send gesture instruction to host";
static const char longname[] = "Gadget MSC_USB";
static const char source_sink[] = "source and sink data";
static char serial[] = "0123456789.0123456789.0123456789";

#define STRING_MANUFACTURER 25
#define STRING_PRODUCT 42
#define STRING_SERIAL 101
#define STRING_SOURCE_SINK 250
#define STRING_SENDINSTRUCTION 251

#define DRIVER_VENDOR_NUM 0x05AC /* NetChip */
#define DRIVER_PRODUCT_NUM 0x2222 

#define USB_BUFSIZ 256

static char manufacturer[50] = "ZheJiangUniversity@WuXianwei";
struct class *usbmsc_class;
static int usb_msc_major = 230;
dev_t devno;

static const char *EP_OUT_NAME;

struct msc_dev { //msc设备结构
                spinlock_t lock;
                struct usb_gadget *gadget;
                struct usb_request *req; /* for control responses */
                struct usb_ep *in_ep;
                struct cdev cdev;
                unsigned char data[128];
                unsigned int data_size;
                //wait_queue_head_t bulkrq;
		struct completion bulkrq;
};

#define CONFIG_SENDINSTRUC 2   //配置值

/**********************************************************描述符************************************************/

static struct usb_device_descriptor device_desc = { //设备描述符
                .bLength = sizeof device_desc,
                .bDescriptorType = USB_DT_DEVICE,
                .bcdUSB = __constant_cpu_to_le16(0x0200),  //USB2.0协议版本
                .bDeviceClass = USB_CLASS_VENDOR_SPEC,
                .idVendor = __constant_cpu_to_le16(DRIVER_VENDOR_NUM),
                .idProduct = __constant_cpu_to_le16(DRIVER_PRODUCT_NUM),
                .iManufacturer = STRING_MANUFACTURER,
                .iProduct = STRING_PRODUCT,
                .iSerialNumber = STRING_SERIAL,
                .bNumConfigurations = 1,
};

static struct usb_endpoint_descriptor fs_source_desc = { //端点描述符
                    .bLength = USB_DT_ENDPOINT_SIZE,
                    .bDescriptorType = USB_DT_ENDPOINT,
                    .bEndpointAddress = USB_DIR_IN, //对主机端来说，输出
                    .bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_config_descriptor sendinstruc_config = { //配置描述符
                    .bLength = sizeof sendinstruc_config,
                    .bDescriptorType = USB_DT_CONFIG,
                    /* compute wTotalLength on the fly */
                    .bNumInterfaces = 1,
                    .bConfigurationValue = CONFIG_SENDINSTRUC,
                    .iConfiguration = STRING_SENDINSTRUCTION,
                    .bmAttributes = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
                    .bMaxPower = 1, /* self-powered */
};

static const struct usb_interface_descriptor sendinstruc_intf = { //接口描述符
                    .bLength = sizeof sendinstruc_intf,
                    .bDescriptorType = USB_DT_INTERFACE,
                    .bNumEndpoints = 1,
                    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
                    .iInterface = STRING_SENDINSTRUCTION,
};

static struct usb_string strings[] = { //字符串描述符
                    { STRING_MANUFACTURER, manufacturer, },
                    { STRING_PRODUCT, longname, },
                    { STRING_SERIAL, serial, },
                    { STRING_SENDINSTRUCTION, sendinstruc, },
                    { STRING_SOURCE_SINK, source_sink, },
                    { } /* end of list */
};

static struct usb_gadget_strings stringtab = {
                    .language = 0x0409, /* en-us */
                    .strings = strings,
};

static const struct usb_descriptor_header *fs_sendinstruc_function[] = {
                    (struct usb_descriptor_header *) &sendinstruc_intf,
                    (struct usb_descriptor_header *) &fs_source_desc,
                    NULL,
};

/******************************************************字符设备操作**************************************/

static int usb_msc_open (struct inode *inode, struct file *file) //打开设备
{
    struct msc_dev *dev =
    container_of (inode->i_cdev, struct msc_dev, cdev);
    file->private_data = dev;
    printk("usb_msc_open !dev->in_ep->desc =%d\n", !dev->in_ep->desc);
    //init_waitqueue_head (&dev->bulkrq);
    init_completion(&dev->bulkrq);
    return 0;
}

static int usb_msc_release (struct inode *inode, struct file *file) //关闭设备
{
    return 0;
}

static void free_ep_req(struct usb_ep *ep, struct usb_request *req)  //释放分配给端点的req请求
{
    kfree(req->buf);
    usb_ep_free_request(ep, req);
}

static struct usb_request *alloc_ep_req(struct usb_ep *ep, unsigned length)   //分配请求给端点
{
    struct usb_request *req;

    req = usb_ep_alloc_request(ep, GFP_ATOMIC);
    if (req) 
    {
        req->length = length;
        req->buf = kmalloc(length, GFP_ATOMIC);
        if (!req->buf) 
        {
          usb_ep_free_request(ep, req);
          req = NULL;
        }
    }
    return req;
}

static void source_sink_complete(struct usb_ep *ep, struct usb_request *req)//请求完成函数
{
    struct msc_dev *dev = ep->driver_data;
    int status = req->status;
    printk("source_sink_complete status= \n", req->status);
    switch (status) 
    {
        case 0: /* normal completion */
                if (ep == dev->in_ep) 
                {
                    printk("Send gesture instruction succeed! Bytes need to send: %d , Actual send bytes: %d", req->length, req->actual);
                }
                     /* this endpoint is normally active while we're configured */
                break;
        case -ECONNABORTED: /* hardware forced ep reset */
        case -ECONNRESET: /* request dequeued */
        case -ESHUTDOWN: /* disconnect from host */
                        printk("%s gone (%d), %d/%d/n", ep->name, status, req->actual, req->length);
        case -EOVERFLOW: /* buffer overrun on read means that we didn't provide a big enough buffer.*/
        default:
        #if 1
                printk("%s complete --> %d, %d/%d/n", ep->name, status, req->actual, req->length);
        #endif
        case -EREMOTEIO: /* short read */
                break;
    }
    free_ep_req(ep, req);
    //wake_up_interruptible(&dev->bulkrq); //唤醒写函数
    complete(&dev->bulkrq); //唤醒写函数
}

ssize_t usb_msc_write (struct file * file, const char __user * userbuf, size_t nbytes) //读设备
{
    struct msc_dev *dev =file->private_data;
    struct usb_request *req;
    int status;
    struct usb_ep *ep;
//    struct usb_gadget *gadget = dev->gadget;
//    ssize_t ret = 0;
//    int result;
    printk("nbytes=%d \n",nbytes);
    if (nbytes < 0)
        return -EINVAL;
    ep = dev->in_ep;
    printk("usb_msc_write !ep.desc =%d\n", !ep->desc);
    if(nbytes > 128)
    {
        printk("Error :too much bytes to write.  \n");
        return 0;
    }
    req = alloc_ep_req(ep, 128);
    if (!req)
    {
	printk("alloc_ep_req(ep, 128) error  \n");
        return 0;
    }
    copy_from_user(req->buf, userbuf, nbytes);
    req->length = nbytes;
    req->complete = source_sink_complete; //请求完成函数
    status = usb_ep_queue(ep, req, GFP_ATOMIC); //递交请求
    if (status) 
    {
      struct msc_dev *dev = ep->driver_data;
      printk("start %s error--> %d \n", ep->name, status);
      free_ep_req(ep, req);
      req = NULL;
    }
    else
    {
      printk("usb_ep_queue(ep, req, GFP_ATOMIC) succeed \n");
    }
    //interruptible_sleep_on (&dev->bulkrq);//睡眠，等到请求完成
    wait_for_completion(&dev->bulkrq);
    printk("sleep back! \n");
    printk("req->actual=%d \n", req->actual);
    return req->actual;
}

struct file_operations usb_msc_fops = {
            .owner = THIS_MODULE,
            .write = usb_msc_write,
            .open = usb_msc_open,
            .release = usb_msc_release,
};

static void usb_msc_setup_cdev (struct msc_dev *dev, int index)
{
    int err;
    
    devno = MKDEV(usb_msc_major, index);
    cdev_init(&dev->cdev, &usb_msc_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, devno, 1);
    if(err)
    {
        printk(KERN_EMERG "Error %d adding usb_msc%d", err, index);
	return;
    }
}

static int usb_msc_cdev_init (struct msc_dev *dev)//注册字符设备驱动
{
    int ret;
    devno = MKDEV(usb_msc_major, 0);

    if(usb_msc_major)
    {
        printk(KERN_EMERG "register_chrdev_region usb_msc_major%d", usb_msc_major);
        ret = register_chrdev_region(devno, 1, "usb_msc");
        printk(KERN_EMERG "register_chrdev_region ret%d", ret);
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "usb_msc");
        usb_msc_major = MAJOR(devno);
        printk(KERN_EMERG "alloc_chrdev_region usb_msc_major%d", usb_msc_major);
    }

    if(ret < 0)
        return ret;

    usbmsc_class = class_create(THIS_MODULE, "usb_msc_class");
    if(IS_ERR(usbmsc_class))
    {
	printk("%s create class error \n", __func__);
	return -1;
    }

    device_create(usbmsc_class, NULL, devno, NULL, "usb_msc");

    usb_msc_setup_cdev(dev, 0);
    return 0;
}
/**************************************************USB Gadget*******************************************/

static void msc_setup_complete(struct usb_ep *ep, struct usb_request *req)//配置端点0的请求完成处理
{
    if (req->status || req->actual != req->length)
        printk("setup complete --> %d, %d/%d/n",req->status, req->actual, req->length);
}

static void msc_reset_config(struct msc_dev *dev) //复位配置
{
    usb_ep_disable(dev->in_ep);
    dev->in_ep = NULL;
}

static void msc_disconnect(struct usb_gadget *gadget)//卸载驱动时被调用，做一些注销工作
{
    struct msc_dev *dev = get_gadget_data(gadget);
    unsigned long flags;
    //unregister_chrdev_region (MKDEV (usb_msc_major, 0), 1);
    //device_destroy(usbmsc_class, devno);
    //class_destroy(usbmsc_class);
    //unregister_chrdev_region (devno, 1);
    //cdev_del (&(dev->cdev));
    //msc_reset_config(dev);
    printk("in %s/n",__FUNCTION__);
}

static int config_buf(struct usb_gadget *gadget,u8 *buf, u8 type, unsigned index)   //获取配置描述符（包括接口描述符和端点描述符）
{
    //int is_source_sink;
    int len;
    const struct usb_descriptor_header **function;
    int hs = 0;
    function =fs_sendinstruc_function;//根据fs_sendinstruc_function，得到长度，
                                    //此处len=配置（9）+1个接口（9）+1个端点（7）=25
    len = usb_gadget_config_buf(&sendinstruc_config, buf, USB_BUFSIZ, function);
    if (len < 0)
        return len;
    ((struct usb_config_descriptor *) buf)->bDescriptorType = type;
    return len;
}  

static int set_sendinstruc_config(struct msc_dev *dev)  //激活设备
{
    int result = 0;
    struct usb_ep *ep;
    struct usb_gadget *gadget = dev->gadget;
    //const struct usb_endpoint_descriptor *d;
    dev->in_ep->desc = &fs_source_desc;
    ep=dev->in_ep;
    printk (KERN_EMERG "msc_setup set_sendinstruc_config function runs \n");
    //dev->in_ep=usb_ep_autoconfig(gadget, &fs_source_desc);
    //printk (KERN_EMERG "msc_setup set_sendinstruc_config usb_ep_autoconfig function runs \n");
    result = usb_ep_enable(ep); //激活端点
    //printk("");
    printk (KERN_EMERG "msc_setup set_sendinstruc_config usb_ep_enable(ep) function runs,result=%d \n",result);
    if (result == 0) {
        printk("connected/n"); //如果成功，打印“connected”
    }
    else
        printk("can't enable %s, result %d/n", ep->name, result);
    return result;
}

static int msc_set_config(struct msc_dev *dev, unsigned number)
{
    int result = 0;
    struct usb_gadget *gadget = dev->gadget;
    printk (KERN_EMERG "msc_setup msc_set_config function runs \n");
    result = set_sendinstruc_config(dev);//激活设备
    if (result!=0)
        msc_reset_config(dev); //复位设备
    else 
    {
        char *speed;

        switch (gadget->speed) 
        {
            case USB_SPEED_LOW: speed = "low"; break;
            case USB_SPEED_FULL: speed = "full"; break;
            case USB_SPEED_HIGH: speed = "high"; break;
            default: speed = " "; break;
        }
        printk("Gadget speed is %s", speed);
    }
    return result;
 /*   char *speed;

    switch (gadget->speed) 
    {
       case USB_SPEED_LOW: speed = "low"; break;
       case USB_SPEED_FULL: speed = "full"; break;
       case USB_SPEED_HIGH: speed = "high"; break;
       default: speed = " "; break;
    }
    printk("Gadget speed is %s", speed);
    return 0;  */
}

/*msc_setup完成USB设置阶段和具体功能相关的交互部分*/
static int msc_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
    struct msc_dev *dev = get_gadget_data(gadget);
    struct usb_request *req = dev->req;
    int value = -EOPNOTSUPP;
    u16 w_index = le16_to_cpu(ctrl->wIndex);
    u16 w_value = le16_to_cpu(ctrl->wValue);
    u16 w_length = le16_to_cpu(ctrl->wLength);

/* usually this stores reply data in the pre-allocated ep0 buffer,
                   * but config change events will reconfigure hardware.
                   */
    req->zero = 0;
    printk (KERN_EMERG "msc_setup function runs,ctrl->bRequest = %d \n", ctrl->bRequest);
    switch (ctrl->bRequest) 
    {
        case USB_REQ_GET_DESCRIPTOR: //获取描述符
            if (ctrl->bRequestType != USB_DIR_IN)
                    goto unknown;
	    printk (KERN_EMERG "ctrl->bRequestType = USB_DIR_IN \n");
            switch (w_value >> 8) 
            {
                case USB_DT_DEVICE: //获取设备描述符
			printk (KERN_EMERG "Get USB_DT_DEVICE desciptor \n");
                        value = min(w_length, (u16) sizeof device_desc);
                        memcpy(req->buf, &device_desc, value);
                        break;
                case USB_DT_CONFIG: //获取配置，注意：会根据fs_sendinstruc_function读取到接口、端点描述符，注意通过config_buf完成读取数据及数量的统计。
                        printk (KERN_EMERG "Get USB_DT_CONFIG desciptor \n");
			value = config_buf(gadget, req->buf, w_value >> 8, w_value & 0xff);
                        if (value >= 0)
                            value = min(w_length, (u16) value);
                        break;
                case USB_DT_STRING:
			printk (KERN_EMERG "Get USB_DT_STRING desciptor \n");
                        value = usb_gadget_get_string(&stringtab, w_value & 0xff, req->buf);
                        if (value >= 0)
                        value = min(w_length, (u16) value);
                        break;
            }
            break;

        case USB_REQ_SET_CONFIGURATION:             //主机在控制阶段最后发出的命令  在此激活所有要用的端点
	    printk (KERN_EMERG "Set USB_REQ_SET_CONFIGURATION \n");
            if (ctrl->bRequestType != 0)
                    goto unknown;
            spin_lock(&dev->lock);
            value = msc_set_config(dev, w_value);//激活相应的端点
            spin_unlock(&dev->lock);
            break;

        default:
            unknown:
                printk("unknown control req%02x.%02x v%04x i%04x l%d \n", ctrl->bRequestType, ctrl->bRequest, 
                    w_value, w_index, w_length);
    }
                  /* respond with data transfer before status phase */
    printk (KERN_EMERG "sign value \n");
    if (value >= 0) 
    {
        req->length = value;
        req->zero = value < w_length;
	printk (KERN_EMERG "value = %d \n", value);
        value = usb_ep_queue(gadget->ep0, req, GFP_ATOMIC);//通过端点0完成setup
        if (value < 0) 
        {
            printk("ep_queue --> %d/n", value);
            req->status = 0;
            msc_setup_complete(gadget->ep0, req);
        }
    }
                  /* device either stalls (value < 0) or reports success */
    printk("msc_setup() !dev->ep->desc =%d \n", !dev->in_ep->desc);
    
    return value;
}

static void msc_unbind(struct usb_gadget *gadget) //解除绑定
{
    struct msc_dev *dev = get_gadget_data(gadget);

    printk("unbind function runs/n");
    //unregister_chrdev_region (MKDEV (usb_msc_major, 0), 1);
    device_destroy(usbmsc_class, devno);
    class_destroy(usbmsc_class);
    unregister_chrdev_region (devno, 1);
    cdev_del (&(dev->cdev));
    /* we've already been disconnected ... no i/o is active */
    if (dev->req) 
    {
        dev->req->length = USB_BUFSIZ;
        free_ep_req(gadget->ep0, dev->req);
    }
    kfree(dev);
    set_gadget_data(gadget, NULL);
}

static int __init msc_bind(struct usb_gadget *gadget) //绑定过程
{
    struct msc_dev *dev;
    struct usb_ep *ep;
    int gcnum,result;
    printk (KERN_EMERG "msc_bind function runs!\n");
    usb_ep_autoconfig_reset(gadget);
    ep = usb_ep_autoconfig(gadget, &fs_source_desc);//根据端点描述符及控制器端点情况，分配一个合适的端点。
    if (!ep)
        goto enomem;
    printk (KERN_EMERG "usb_ep_autoconfig succeeded!\n");
    EP_OUT_NAME = ep->name; //记录名称
    gcnum = usb_gadget_controller_number(gadget);//获得控制器代号
    if (gcnum >= 0)
        device_desc.bcdDevice = cpu_to_le16(0x0200 + gcnum);//赋值设备描述符
    else 
    {
        pr_warning("%s: controller '%s' not recognized/n", shortname, gadget->name);
        device_desc.bcdDevice = __constant_cpu_to_le16(0x9999);
    }
    dev = kzalloc(sizeof(*dev), GFP_KERNEL); //分配设备结构体
    if (!dev)
        return -ENOMEM;
    spin_lock_init(&dev->lock);    //dev结构体锁初始化
    dev->gadget = gadget;
    set_gadget_data(gadget, dev);
    dev->req = usb_ep_alloc_request(gadget->ep0, GFP_KERNEL);//分配一个请求给端点0
    if (!dev->req)
        goto enomem;
    dev->req->buf = kmalloc(USB_BUFSIZ, GFP_KERNEL);
    if (!dev->req->buf)
        goto enomem;
    dev->req->complete = msc_setup_complete;
    ep->driver_data = dev;
    //ep->desc = &fs_source_desc;
    dev->in_ep=ep; //记录端点（就是接收host端数据的端点）
    printk("!dev->in_ep->desc =%d \n", !dev->in_ep->desc);
    //printk("!dev->in_ep.desc =%d \n", !dev->in_ep.desc);
    printk("!ep->desc =%d \n", !ep->desc);
    //printk("!ep.desc =%d \n", !ep.desc);
    printk("dev->in_ep->name=%s \n",dev->in_ep->name); //打印出这个端点的名称
    printk(KERN_EMERG "dev->in_ep->maxpacket=%d \n",dev->in_ep->maxpacket);
    printk(KERN_EMERG "dev->in_ep->address=%d \n",dev->in_ep->address);
    //result = usb_ep_enable(dev->in_ep); //激活端点
    //printk("");
    printk (KERN_EMERG "msc_setup usb_ep_enable(dev->in_ep) function runs,result=%d \n",result);
    device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;
    usb_gadget_set_selfpowered(gadget);
    gadget->ep0->driver_data = dev;
    snprintf(manufacturer, sizeof manufacturer, "%s %s with %s", 
        init_utsname()->sysname, init_utsname()->release, gadget->name);
/**************************字符设备注册*******************/
    usb_msc_cdev_init (dev);
    return 0;
    enomem:
        //msc_unbind(gadget);
        msc_disconnect(gadget);
	return -ENOMEM;
}

/******************************************************USB Gadget结构体注册**************************/

static struct usb_gadget_driver msc_driver = { //gadget驱动的核心数据结构
        #ifdef CONFIG_USB_GADGET_DUALSPEED
                .max_speed = USB_SPEED_HIGH,
        #else
                .max_speed = USB_SPEED_FULL,
        #endif
                .function = (char *) longname,
                //.bind = msc_bind,
                .unbind = __exit_p(msc_unbind),
                .setup = msc_setup,
                .disconnect = msc_disconnect,
                //.suspend = msc_suspend, //不考虑电源管理的功能
                //.resume = msc_resume,
                .driver = {
                            .name = (char *) shortname,
                            .owner = THIS_MODULE,
                },
};

MODULE_AUTHOR("Shallwe Woo");
MODULE_LICENSE("GPL");

static int __init init(void)
{
    return usb_gadget_probe_driver(&msc_driver, msc_bind);
}
module_init(init);

static void __exit cleanup(void)
{
    usb_gadget_unregister_driver(&msc_driver); //注销驱动，通常会调用到unbind解除绑定， //在s3c2410_udc.c中调用的是disconnect方法
}
module_exit(cleanup);
