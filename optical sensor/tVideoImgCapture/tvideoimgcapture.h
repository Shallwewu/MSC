#ifndef TVIDEOIMGCAPTURE_H
#define TVIDEOIMGCAPTURE_H

#include <QWidget>
#include "tv4l2device.h"

//#define BUILD_SAVE_FMT_JPEG
#ifdef BUILD_SAVE_FMT_JPEG
#include <jpeglib.h>
#endif
class QLabel;
class QImage;
class QPainter;
class QTimer;
class QThread;
class QPushButton;
class QPaintEvent;
#ifdef BUILD_SAVE_FMT_JPEG
typedef unsigned char uint8_t;
#endif

class TVideoImgCapture : public QWidget
{
    Q_OBJECT

public:
    TVideoImgCapture(QWidget *parent = 0);
    ~TVideoImgCapture();

private slots:
    void slotOnBtnStartClicked();
    void slotOnBtnSaveImgClicked();
    void slotOnBtnQuitClicked();

    void slotOnImgReady(uchar *yuvBuf, int size);

    void slotInitVideoDevice();

private:
    int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height);
    int convert_yuv_to_rgb_pixel(int y, int u, int v);
#ifdef BUILD_SAVE_FMT_JPEG
    int yuv422_to_jpeg(unsigned char *data, int image_width, int image_height, FILE *fp, int quality);
    //int compress_yuyv_to_jpeg(unsigned char *buf, unsigned char *buffer, int size, int quality);
    int encode_jpeg(char *lpbuf,int width,int height, FILE *fp);
#endif
    QLabel *lblTitle;

    QLabel *lblImageDisplay;
    QPushButton *btnStartStop;
    QPushButton *btnSaveImg;
    QPushButton *btnQuit;
    QLabel *lblCapThumbnail;
    QLabel *lblImgFilename;

    QLabel *lblCompanyInfo;
    QLabel *lblCompanyWebsite;

    QImage *imgCap;
    uchar *rgbBuf;

    QTimer *timer;

    QThread *threadVideoCapture;
    TV4l2Device *videoDevice;
};

#endif // TVIDEOIMGCAPTURE_H
