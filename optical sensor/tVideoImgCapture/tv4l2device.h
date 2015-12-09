#ifndef TV4L2DEVICE_H
#define TV4L2DEVICE_H

#include <QObject>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

typedef enum {
    IO_METHOD_READ, IO_METHOD_MMAP, IO_METHOD_USERPTR,
} io_method;

struct buffer {
    void * start;
    size_t length;
};

class TV4l2Device : public QObject
{
    Q_OBJECT
public:
    explicit TV4l2Device(QString strDevPath = "/dev/video0", uint width = 640, uint height = 480, QObject *parent = 0);

    void VideoCapExit();

signals:
    void signalImgReady(QByteArray baDataYuyv, uint baSz);
    void signalImgReady(uchar *p, int size);

private slots:
    void slotGetImgYuyv();

private:
    void errno_exit(const char * s);
    int xioctl(int fd, int request, void * arg);
    //void process_image(const void * p, int size);
    void process_image(void *p, int size);
    int read_frame(void);
    void mainloop(void);
    void stop_capturing(void);
    void start_capturing(void);
    void uninit_device(void);
    void init_read(unsigned int buffer_size);
    void init_mmap(void);
    void init_userp(unsigned int buffer_size);
    void init_device(void);
    void close_device(void);
    void open_device(void);

    char dev_name[64];
    io_method io;
    int fd;
    struct buffer * buffers;
    unsigned int n_buffers;

    uint imgWidth;
    uint imgHeight;

    QString strDevicePath;
};

#endif // TV4L2DEVICE_H
