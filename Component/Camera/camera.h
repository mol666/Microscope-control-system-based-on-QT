#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include<QString>
#include"Component/printerror.h"
#include"include/dcam/dcamapi4.h"
#include"include/dcam/dcamprop.h"
#include<QDir>
#include"include/opencv/opencv2/core.hpp"
#include"include/opencv/opencv2/opencv.hpp"
#include<QImage>
#include"imagewriter.h"
#include"brightnessset.h"
#include"contrast.h"
#include<QTimer>
#include"cameracontrast.h"
#include"anocameracontrast.h"

extern QString idCamera[4];
extern int32 idDevice[4];
void getCameraID(HDCAM hdcam,int i);
void dcamcon_init();
HDCAM dcamcon_open(int32 iDevice);
//void output_data(const char* filename, char* buf, int32 bufsize);

//定义了相机参数的结构体和所有参数的结构体
struct cameraInfo {
    double exposure,pixelSize,Width,Height;
    int waveLength,filter,Column;
    QString Path;
};

//定义一个相机类，实现相机的功能
class Camera : public QObject
{
    Q_OBJECT

public:
    explicit Camera(QObject *parent = nullptr);
    ~Camera();

    DCAMERR err;
    HDCAM m_hdcam;

    //double m_width,m_height,m_hpos,m_vpos=0;

    DCAMWAIT_OPEN m_waitopen;   //structure
    HDCAMWAIT m_hwait;          //handle
    DCAMWAIT_START m_waitstart; //structure
    DCAMBUF_FRAME m_bufframe;   //structure
    DCAMCAP_TRANSFERINFO m_transinfo;
    const int m_bufferCapacity = 50;

    cv::Size imagesize;

    bool setValue();
    bool set_ROI();
    bool prepareCapture();
    bool finishCapture();
    //bool setImageHeight(int);
    bool getimageInfo();
    void imaging(int i);
    void output_data(QList<void*> &buffers);

    //int imageHeight;
    //QString OutputPath;

    void setCamVal(struct cameraInfo camInfo);

    void CamStart(int i);
    void CamStart1(int i);

    struct cameraInfo camInfo;

    void Live1();
    void Live2();

    static bool stopflag;

    uchar* pData8 = nullptr;

    static bool NormalBrightness,AutoBrightness,setBrightness;

    //默认亮度
    int defBrightness = 16384;   //128*128取像素值的中位数作为图像显示的默认亮度

    QTimer *liveTimer = nullptr;

private:


signals:
    void transImage(QImage image);

    //void saveImage(void* image);
    void saveImage1(QByteArray a);

    void liveImage(QImage image);
    //void liveImage1(QByteArray a);

public slots:


};

#endif // CAMERA_H
