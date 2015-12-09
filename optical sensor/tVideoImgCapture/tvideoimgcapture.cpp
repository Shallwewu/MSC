//extern "C"{
//#include "jpeglib.h"
//}
#include "tvideoimgcapture.h"
#include <QLabel>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QThread>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDateTime>
#include <QMessageBox>
#include <QFile>
#include <QDebug>
#include "rgb2bmp.h"

#define PIX_WIDTH 640
#define PIX_HEIGHT 480
#define RGB_BUFSZ (PIX_WIDTH * PIX_HEIGHT * 3)
#define YUV_BUFSZ (PIX_WIDTH * PIX_HEIGHT * 2)

static uchar *yuvDBuf;

const char *video_device_path = "/dev/video1";

volatile static int saveImgFlag = 0;  // 1: Save

enum VideoCapState {
    VIDEOCAP_RUNNING,
    VIDEOCAP_PAUSE
};

TVideoImgCapture::TVideoImgCapture(QWidget *parent)
    : QWidget(parent)
{
    this->setWindowFlags(Qt::FramelessWindowHint);

    rgbBuf = (uchar *)malloc(RGB_BUFSZ);
    yuvDBuf = (uchar *)malloc(YUV_BUFSZ);

    imgCap = new QImage(rgbBuf, PIX_WIDTH, PIX_HEIGHT, QImage::Format_RGB888);

    lblImageDisplay = new QLabel();
    lblImageDisplay->setFixedSize(600, 450);
    lblImageDisplay->setAutoFillBackground(true);
    lblImageDisplay->setStyleSheet("background-color:honeydew; border-width: 1px; border-style: solid; border-color: rgb(255, 170, 0);");
    lblImageDisplay->setAlignment(Qt::AlignCenter);

    lblTitle = new QLabel(tr("视频图像采集测试"));
    lblTitle->setAlignment(Qt::AlignCenter);
    lblTitle->setMinimumHeight(45);
    lblTitle->setStyleSheet("background-color:honeydew; color:blue");
    lblTitle->setFrameStyle(QFrame::Box | QFrame::Sunken);

    btnStartStop = new QPushButton(tr("开始"));
    btnStartStop->setMinimumHeight(45);
    connect(btnStartStop, SIGNAL(clicked()), this, SLOT(slotOnBtnStartClicked()));
    btnSaveImg = new QPushButton(tr("保存图片"));
    btnSaveImg->setMinimumHeight(45);
    connect(btnSaveImg, SIGNAL(clicked()), this, SLOT(slotOnBtnSaveImgClicked()));
    btnQuit = new QPushButton(tr("退出"));
    btnQuit->setMinimumHeight(45);
    connect(btnQuit, SIGNAL(clicked()), this, SLOT(slotOnBtnQuitClicked()));

    lblCapThumbnail = new QLabel();
    lblCapThumbnail->setFixedSize(168, 126);
    lblCapThumbnail->setAutoFillBackground(true);
    lblCapThumbnail->setStyleSheet("background-color:honeydew; border-width: 1px; border-style: solid; border-color: rgb(255, 170, 0);");

    lblImgFilename = new QLabel();
    lblImgFilename->setMinimumHeight(42);
    lblImgFilename->setAlignment(Qt::AlignCenter);
    lblImgFilename->setStyleSheet("background-color:honeydew; border-width: 1px; border-style: solid; border-color: rgb(255, 170, 0);");

    lblCompanyInfo = new QLabel(tr("杭州安米电子有限公司"));
    lblCompanyInfo->setStyleSheet("color:blue");
    lblCompanyWebsite = new QLabel(tr("www.mcuzone.com"));
    lblCompanyWebsite->setStyleSheet("color:blue");

    QVBoxLayout *vLayoutRight = new QVBoxLayout();
    vLayoutRight->addSpacing(5);
    vLayoutRight->addWidget(lblTitle);
    vLayoutRight->addWidget(btnStartStop);
    vLayoutRight->addWidget(btnSaveImg);
    vLayoutRight->addWidget(lblCapThumbnail);
    vLayoutRight->addWidget(lblImgFilename);
    vLayoutRight->addStretch();
    vLayoutRight->addWidget(btnQuit);
    vLayoutRight->addSpacing(5);
    vLayoutRight->addWidget(lblCompanyInfo);
    vLayoutRight->addWidget(lblCompanyWebsite);
    vLayoutRight->addSpacing(5);

    QHBoxLayout *mhLayout = new QHBoxLayout(this);
    mhLayout->addWidget(lblImageDisplay);
    mhLayout->addLayout(vLayoutRight);

    timer = new QTimer(this);
    timer->setInterval(30);

    threadVideoCapture = new QThread(this);

    QTimer::singleShot(500, this, SLOT(slotInitVideoDevice()));
}

TVideoImgCapture::~TVideoImgCapture()
{
    timer->stop();
    videoDevice->VideoCapExit();
    threadVideoCapture->exit();

    free(yuvDBuf);
    free(rgbBuf);
}

void TVideoImgCapture::slotInitVideoDevice()
{
    QFile fileVideoDev(video_device_path);
    if (!fileVideoDev.exists()) {
        QMessageBox::warning(this, tr("Warning"), tr("设备%1不存在，请检查设备接入是否正确！").arg(video_device_path), QMessageBox::Ok);
        this->close();
    }
    videoDevice = new TV4l2Device(video_device_path, PIX_WIDTH, PIX_HEIGHT);
    videoDevice->moveToThread(threadVideoCapture);
    connect(timer, SIGNAL(timeout()), videoDevice, SLOT(slotGetImgYuyv()));
    connect(videoDevice, SIGNAL(signalImgReady(uchar*,int)), this, SLOT(slotOnImgReady(uchar*,int)));

    threadVideoCapture->start();
}

void TVideoImgCapture::slotOnBtnQuitClicked()
{
    this->close();
}
void TVideoImgCapture::slotOnBtnSaveImgClicked()
{
    saveImgFlag = 1;
}
void TVideoImgCapture::slotOnBtnStartClicked()
{
    static enum VideoCapState vcstate = VIDEOCAP_PAUSE;

    if (vcstate == VIDEOCAP_PAUSE) {
        timer->start();
        vcstate = VIDEOCAP_RUNNING;
        btnStartStop->setText(tr("暂停"));
    } else if (vcstate == VIDEOCAP_RUNNING) {
        timer->stop();
        vcstate = VIDEOCAP_PAUSE;
        btnStartStop->setText(tr("开始"));
    }
}

void TVideoImgCapture::slotOnImgReady(uchar *yuvBuf, int size)
{
    //static int cntx = 0;
    struct timeval tStartTime;
    struct timeval tEndTime;
    memcpy(yuvDBuf, yuvBuf, size);
#if 0
    if (++cntx == 30) {
        FILE *fp = fopen("abc000.yuv", "w");
        fwrite(yuvDBuf, size, 1, fp);
        fclose(fp);
    }
#endif
    convert_yuv_to_rgb_buffer(yuvDBuf, rgbBuf, PIX_WIDTH, PIX_HEIGHT);
    gettimeofday(&tStartTime,0);
    imgCap->loadFromData(rgbBuf, RGB_BUFSZ);
    //QImage tempImg = imgCap->mirrored(false, false);
    //lblImageDisplay->setPixmap(QPixmap::fromImage(tempImg.scaled(600, 450, Qt::KeepAspectRatio)));
    lblImageDisplay->setPixmap(QPixmap::fromImage(imgCap->scaled(600, 450, Qt::KeepAspectRatio)));
    gettimeofday(&tEndTime,0);   
    //qDebug("[D] CNTX: %d", ++cntx);
    fprintf(stderr, "load picture sec time=%u,usec time=%u",(unsigned int)(tEndTime.tv_sec-tStartTime.tv_sec),(unsigned int)(tEndTime.tv_usec-tStartTime.tv_usec));
    if (saveImgFlag) {
        //qDebug() << "[D] START:" << QDateTime::currentDateTime().toString("hhmmsszzz");
        saveImgFlag = 0;

        //lblCapThumbnail->setPixmap(QPixmap::fromImage((tempImg.scaled(168,126,Qt::KeepAspectRatio))));
        lblCapThumbnail->setPixmap(QPixmap::fromImage((imgCap->scaled(168,126,Qt::KeepAspectRatio))));

        QString strCurrentDateTime = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
#ifdef BUILD_SAVE_FMT_JPEG
        QString strBmpFileName(strCurrentDateTime + ".jpg");
        FILE *bmpfp = fopen(strBmpFileName.toLatin1().data(), "wb");
        encode_jpeg((char *)rgbBuf, PIX_WIDTH, PIX_HEIGHT, bmpfp);
        lblImgFilename->setText(strCurrentDateTime + "\n.jpg");

        //qDebug() << "[D] STOP:" << QDateTime::currentDateTime().toString("hhmmsszzz");
#else
        QString strBmpFileName(strCurrentDateTime + ".bmp");
        FILE *bmpfp = fopen(strBmpFileName.toLatin1().data(), "wa+");
        RGB2BMP((char *)rgbBuf, PIX_WIDTH, PIX_HEIGHT, bmpfp);
        fclose(bmpfp);
        lblImgFilename->setText(strCurrentDateTime + "\n.bmp");  
#endif
    }
}

// YUYV_to_RGB
int TVideoImgCapture::convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
    int y0, u, y1, v;
    for(in = 0; in < width * height * 2; in += 4) {
        pixel_16 =
            yuv[in + 3] << 24 |
            yuv[in + 2] << 16 |
            yuv[in + 1] <<  8 |
            yuv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;
        pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
        pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
    }
    return 0;
}

int TVideoImgCapture::convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    unsigned int pixel32 = 0;
    unsigned char *pixel = (unsigned char *)&pixel32;
    int r, g, b;
    r = y + (1.370705 * (v-128));
    g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
    b = y + (1.732446 * (u-128));
    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;
    pixel[0] = r * 220 / 256;
    pixel[1] = g * 220 / 256;
    pixel[2] = b * 220 / 256;
    return pixel32;
}

#ifdef BUILD_SAVE_FMT_JPEG
int TVideoImgCapture::yuv422_to_jpeg(unsigned char *data, int image_width, int image_height, FILE *fp, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    //JSAMPROW row_pointer[1];
    //int row_stride;
    JSAMPIMAGE  buffer;
    int band,i,buf_width[3],buf_height[3], mem_size, max_line, counter;
    unsigned char *yuv[3];
    uint8_t *pSrc, *pDst;

    yuv[0] = data;
    yuv[1] = yuv[0] + (image_width * image_height);
    yuv[2] = yuv[1] + (image_width * image_height) /2;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = image_width;
    cinfo.image_height = image_height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE );

    cinfo.raw_data_in = TRUE;
    cinfo.jpeg_color_space = JCS_YCbCr;
    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 1;

    jpeg_start_compress(&cinfo, TRUE);

    buffer = (JSAMPIMAGE) (*cinfo.mem->alloc_small) ((j_common_ptr) &cinfo, JPOOL_IMAGE, 3 * sizeof(JSAMPARRAY));
    for(band=0; band <3; band++)
    {
        buf_width[band] = cinfo.comp_info[band].width_in_blocks * DCTSIZE;
        buf_height[band] = cinfo.comp_info[band].v_samp_factor * DCTSIZE;
        buffer[band] = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, buf_width[band], buf_height[band]);
    }
    max_line = cinfo.max_v_samp_factor*DCTSIZE;
    for(counter=0; cinfo.next_scanline < cinfo.image_height; counter++)
    {
        //buffer image copy.
        for(band=0; band <3; band++)
        {
            mem_size = buf_width[band];
            pDst = (uint8_t *) buffer[band][0];
            pSrc = (uint8_t *) yuv[band] + counter*buf_height[band] * buf_width[band];

            for(i=0; i <buf_height[band]; i++)
            {
                memcpy(pDst, pSrc, mem_size);
                pSrc += buf_width[band];
                pDst += buf_width[band];
            }
        }
        jpeg_write_raw_data(&cinfo, buffer, max_line);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return 0;
}

int TVideoImgCapture::encode_jpeg(char *lpbuf,int width,int height, FILE *fp)
{
    struct jpeg_compress_struct cinfo ;
    struct jpeg_error_mgr jerr ;
    JSAMPROW  row_pointer[1] ;
    int row_stride ;
    char *buf=NULL ;
    int x ;

    FILE *fptr_jpg = fp;
    if(fptr_jpg==NULL)
    {
    printf("Encoder:open file failed!/n") ;
     return -1;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fptr_jpg);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);


    jpeg_set_quality(&cinfo, 80,TRUE);


    jpeg_start_compress(&cinfo, TRUE);

    row_stride = width * 3;
    buf=(char *)malloc(row_stride) ;
    row_pointer[0] = (JSAMPROW)buf;
    while (cinfo.next_scanline < height)
    {
     for (x = 0; x < row_stride; x+=3)
    {

    buf[x]   = lpbuf[x];
    buf[x+1] = lpbuf[x+1];
    buf[x+2] = lpbuf[x+2];

    }
    jpeg_write_scanlines (&cinfo, row_pointer, 1);//critical
    lpbuf += row_stride;
    }

    jpeg_finish_compress(&cinfo);
    fclose(fptr_jpg);
    jpeg_destroy_compress(&cinfo);
    free(buf) ;
    return 0;
}
#endif
