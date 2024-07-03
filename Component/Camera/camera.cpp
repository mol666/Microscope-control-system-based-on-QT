#include "camera.h"
#include<QDebug>
#include<QThread>
#include "visor.h"
#include<QTime>

QString idCamera[4] = {NULL};
int32 idDevice[4] = {0};
bool Camera::stopflag = false;
bool Camera::NormalBrightness = true,Camera::AutoBrightness = false,Camera::setBrightness = false;

inline const int my_dcamdev_string(DCAMERR& err,HDCAM hdcam,int32 idStr,char* text,int32 textbytes){
    DCAMDEV_STRING param;
    memset(&param,0,sizeof(param));
    param.size = sizeof (param);
    param.text = text;
    param.textbytes = textbytes;
    param.iString = idStr;

    err = dcamdev_getstring(hdcam,&param);
    return !failed(err);
}

void getCameraID(HDCAM hdcam,int i){
    char cameraid[64];
    DCAMERR err;
    if(!my_dcamdev_string(err,hdcam,DCAM_IDSTR_CAMERAID,cameraid,sizeof(cameraid))){
        PrintError(hdcam,err,"dcamdev_getstring(DCAM_IDSTR_CAMERAID)\n");
    }
    idCamera[i] = cameraid;
}

//相机初始化
void dcamcon_init(){
    DCAMAPI_INIT apiinit;
    memset(&apiinit,0,sizeof(apiinit));
    apiinit.size = sizeof(apiinit);
    DCAMERR err = dcamapi_init(&apiinit);
    if(failed(err)) {
        PrintError(NULL,err,"dcamapi_init()");
        return;
    }
    int32 nDevice = apiinit.iDeviceCount;
    qDebug()<<"Find Camera"<<nDevice;
    assert(nDevice>0);
    for(int32 iDevice=0;iDevice<nDevice;iDevice++) {
        idDevice[iDevice] = iDevice;
        getCameraID((HDCAM)(intptr_t)iDevice,iDevice);
    }
}

//打开相机
HDCAM dcamcon_open(int32 iDevice){
    DCAMDEV_OPEN devopen;
    memset(&devopen,0,sizeof(devopen));
    devopen.size = sizeof(devopen);
    devopen.index = iDevice;
    DCAMERR err = dcamdev_open(&devopen);
    if(!failed(err)) {
        HDCAM hdcam = devopen.hdcam;
        qDebug()<<"open"<<(iDevice+1)<<"camera";
        return hdcam;
    }
    PrintError((HDCAM)(intptr_t)iDevice,err,"dcamdev_open()","index is %d\n",iDevice);
    return NULL;
}

//在关闭窗口之后记得关闭相机和去初始化,   在主线程中VISoR类的析构中完成

/**获得相机相机句柄后，创建相机类，实现相机的参数设置、拍照、数据读出***************************/
Camera::Camera(QObject *parent)
    : QObject{parent}
{
    pData8=new uchar[2304*2304*2];
    liveTimer = new QTimer(this);
    liveTimer->setTimerType(Qt::PreciseTimer);
}

//设置图像区域    图像长度，宽度，左上角顶点的位置信息   设置成功返回true，失败返回false
bool Camera::set_ROI()
{
    err = dcamprop_setvalue(m_hdcam,DCAM_IDPROP_SUBARRAYMODE,DCAMPROP_MODE__OFF);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"set_SUBARRAYMODE__OFF");
        return false;
    }
    err = dcamprop_setvalue(m_hdcam,DCAM_IDPROP_SUBARRAYHSIZE,camInfo.Width);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"set_SUBARRAYHSIZE");
        return false;
    }
    err = dcamprop_setvalue(m_hdcam,DCAM_IDPROP_SUBARRAYVSIZE,camInfo.Height);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"set_SUBARRAYVSIZE");
        return false;
    }
    err = dcamprop_setvalue(m_hdcam,DCAM_IDPROP_SUBARRAYHPOS,0);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"set_SUBARRAYHPOS");
        return false;
    }
    err = dcamprop_setvalue(m_hdcam,DCAM_IDPROP_SUBARRAYVPOS,(2304-camInfo.Height)/2);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"set_SUBARRAYVPOS");
        return false;
    }
//    err = dcamprop_setvalue(m_hdcam,DCAM_IDPROP_SUBARRAYVPOS,300);
//    if(failed(err))
//    {
//        PrintError(m_hdcam,err,"set_SUBARRAYVPOS");
//        return false;
//    }
    err = dcamprop_setvalue(m_hdcam,DCAM_IDPROP_SUBARRAYMODE,DCAMPROP_MODE__ON);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"set_SUBARRAYMODE__ON");
        return false;
    }
    return true;
}

//设置相机参数（ORCA-Fusion,Hamamatsu） 外触发，产生TIMESTAMP源，读取速度
bool Camera::setValue()
{
    if(failed(dcamprop_setvalue(m_hdcam,DCAM_IDPROP_TRIGGERSOURCE,DCAMPROP_TRIGGERSOURCE__EXTERNAL))
     //||failed(dcamprop_setvalue(m_hdcam,DCAM_IDPROP_TRIGGERACTIVE,DCAMPROP_TRIGGERACTIVE__EDGE))
     ||failed(dcamprop_setvalue(m_hdcam,DCAM_IDPROP_TRIGGERACTIVE,DCAMPROP_TRIGGERACTIVE__SYNCREADOUT))
     ||failed(dcamprop_setvalue(m_hdcam,DCAM_IDPROP_TRIGGERPOLARITY,DCAMPROP_TRIGGERPOLARITY__POSITIVE))
     //||failed(dcamprop_setvalue(m_hdcam,DCAM_IDPROP_TRIGGER_GLOBALEXPOSURE,DCAMPROP_TRIGGER_GLOBALEXPOSURE__GLOBALRESET)) //设置global rolling
     ||failed(dcamprop_setvalue(m_hdcam,DCAM_IDPROP_TIMESTAMP_PRODUCER,DCAMPROP_TIMESTAMP_PRODUCER__IMAGINGDEVICE))
     ||failed(dcamprop_setvalue(m_hdcam,DCAM_IDPROP_READOUTSPEED,DCAMPROP_READOUTSPEED__FASTEST))
     //||failed(dcamprop_setvalue(m_hdcam,DCAM_IDPROP_EXPOSURETIME,camInfo.exposure/1000))
     ){
        qWarning()<<"Unable to connect to camera";
        return false;
    }
    return true;
}

//设置HDCAMWAIT handle、Waiting EVENT、Acquisition buffer
bool Camera::prepareCapture()
{
    //dcamprop_setvalue(m_hdcam,DCAM_IDPROP_EXPOSURETIME,0.1);
    //The HDCAMWAIT handle is prepared for the dcamwait functions
    memset(&m_waitopen,0,sizeof(m_waitopen));
    m_waitopen.size = sizeof(m_waitopen);
    m_waitopen.hdcam = m_hdcam;
    err = dcamwait_open(&m_waitopen);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"dcamwait_open()");
        return false;
    }
    m_hwait = m_waitopen.hwait;         //在相机拍完照之后关闭该handle
                                        //创建DCAMWAIT_START指定capturing和recording事件
    memset(&m_waitstart,0,sizeof(m_waitstart));
    m_waitstart.size = sizeof(m_waitstart);
    m_waitstart.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    //m_waitstart.timeout = DCAMWAIT_TIMEOUT_INFINITE;
    m_waitstart.timeout = 200;

    memset(&m_bufframe,0,sizeof(m_bufframe));
    m_bufframe.size = sizeof(m_bufframe);
    m_bufframe.iFrame = -1;

    //allocates internal image buffers for image acquisition
    //在Camera::finishCapture()中调用dcambuf_release(m_hdcam)完成内存的释放
    err = dcambuf_alloc(m_hdcam,m_bufferCapacity);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"dcambuf_alloc()");
        return false;
    }

    memset(&m_transinfo,0,sizeof(m_transinfo));
    m_transinfo.size = sizeof(m_transinfo);
    return true;
}

//拍照结束，释放给相机分配的内存
bool Camera::finishCapture()
{
    err = dcambuf_release(m_hdcam);
    if(failed(err))
    {
        PrintError(m_hdcam,err,"dcambuf_release()");
        return false;
    }
    return true;
}

//获取相机拍摄的图像信息(位深、宽度、高度)
/*
bool Camera::getimageInfo()
{
    double v;
    err = dcamprop_getvalue(m_hdcam,DCAM_IDPROP_IMAGE_PIXELTYPE,&v);
    if(failed(err)){
        PrintError(m_hdcam,err,"dcamprop_getvalue","IDPROP:IMAGE_PIXELTYPE");
        return false;
    }else {
        pixeltype = (int32)v;
    }
    err = dcamprop_getvalue(m_hdcam,DCAM_IDPROP_IMAGE_WIDTH,&v);
    if(failed(err)){
        PrintError(m_hdcam,err,"dcamprop_getvalue","IDPROP:IMAGE_WIDTH");
        return false;
    }else {
        width = (int32)v;
    }
    err = dcamprop_getvalue(m_hdcam,DCAM_IDPROP_IMAGE_HEIGHT,&v);
    if(failed(err)){
        PrintError(m_hdcam,err,"dcamprop_getvalue","IDPROP:IMAGE_HEIGHT");
        return false;
    }else {
        height = (int32)v;
    }
    return true;
}
*/

/*
void Camera::imaging(int i)
{
    qDebug()<<"相机一线程地址为："<<QThread::currentThread();
    qDebug()<<"Camera1 start Capture";
    int lastFrameCount = 0;
    QList<int32> bufferId;
    QList<void*> buffers1;

    while(!VISoR::bStopFlag){
        if(bufferId.empty()){
            err = dcamwait_start(m_hwait,&m_waitstart);
            if(failed(err)){
                PrintError(m_hdcam,err,"dcamwait_start");
                //if(err == DCAMERR_ABORT)
                    break;
            }
            err = dcamcap_transferinfo(m_hdcam,&m_transinfo);
            if(failed(err)){
                PrintError(m_hdcam,err,"dcamcap_transferinfo");
            }
            if(m_transinfo.nFrameCount<1){
                qDebug()<<"not capture image";
            }
            //qDebug()<<"test "<<m_transinfo.nFrameCount;

            int32 frameCount = m_transinfo.nFrameCount - lastFrameCount;
            int32 frameIndex = m_transinfo.nNewestFrameIndex;
            lastFrameCount = m_transinfo.nFrameCount;
            if(frameCount > 1){
                if(frameCount > m_bufferCapacity){
                    frameCount = m_bufferCapacity;
                }
            }
            for(int i=0;i<frameCount;i++){
                int32 id = (frameIndex - i + m_bufferCapacity)%m_bufferCapacity;
                bufferId.prepend(id);
            }
        }
        m_bufframe.iFrame = bufferId.first();
        bufferId.pop_front();
        err = dcambuf_lockframe(m_hdcam,&m_bufframe);
        if(failed(err)){
            PrintError(m_hdcam,err,"dcambuf_lockframe");
        }
        if(imagesize.empty()){
            imagesize = cv::Size(m_bufframe.width,m_bufframe.height);
        }
        void *buffer = malloc(imagesize.area()*2);
        memcpy(buffer,m_bufframe.buf,imagesize.area()*2);
        buffers1.append(buffer);
    }

    dcamcap_stop(m_hdcam);
    dcamwait_abort(m_hwait);
    qDebug()<<"Camera1 stop Capture";
    qDebug()<<"Camera1 stop Capture";
    c
}
*/

/*
void Camera::output_data(QList<void*> &buffers)
{
    QString m_filePath = camInfo.Path+"/OneCol3.ome.tif";
    flsmio::ImageWriter writer(m_filePath);
    int count = 1;
    cv::Size imageSize=cv::Size(m_bufframe.width,m_bufframe.height);
    foreach (void *mbuffer, buffers) {
        writer.addImage(cv::Mat(imageSize,CV_16UC1,mbuffer));
        //std::cout<<"Saving image "<<count<<"\r\b";
        //qDebug()<<count;
        count++;
    }
    qDebug()<<"finish write";
}
*/

void Camera::setCamVal(struct cameraInfo camInfo)
{
    this->camInfo = camInfo;
    //qDebug()<<"camera config"<<this->camInfo.Column;
    //根据界面输入的参数，设置相机参数
    setValue();
    set_ROI();
    prepareCapture();
}
/*
void Camera::CamStart(int i)
{
    QTime startCaptime = QTime::currentTime();
    err = dcamcap_start(m_hdcam,DCAMCAP_START_SEQUENCE);
    if(failed(err)){
        //qDebug()<<"cam error";
        PrintError(m_hdcam,err,"dcamcap_start");
    }
    qDebug()<<"camera"+QString::number(i)<<" start capture";
    qDebug()<<"camera"+QString::number(i)<<" thread address: "<<QThread::currentThread();
    int lastFrameCount = 0;
    QList<int32> bufferId;
    QList<void*> buffers;
//    while(!VISoR::bStopFlag){
//    while(!failed(err) || !VISoR::bStopFlag){
   while(!failed(err)){
        if(bufferId.empty()){
            err = dcamwait_start(m_hwait,&m_waitstart);
            if(failed(err)){
                PrintError(m_hdcam,err,"dcamwait_start");
                //if(err == DCAMERR_ABORT)
                    break;
            }
            err = dcamcap_transferinfo(m_hdcam,&m_transinfo);
            if(failed(err)){
                PrintError(m_hdcam,err,"dcamcap_transferinfo");
            }
            if(m_transinfo.nFrameCount<1){
                qDebug()<<"not capture image";
            }
            //qDebug()<<"test "<<m_transinfo.nFrameCount;
            if(m_transinfo.nFrameCount == 2000){
                QTime stopCaptime = QTime::currentTime();
                int Capelapsed = startCaptime.msecsTo(stopCaptime);
                qDebug()<< "capture time:" << Capelapsed <<"ms";
            }

            int32 frameCount = m_transinfo.nFrameCount - lastFrameCount;
            int32 frameIndex = m_transinfo.nNewestFrameIndex;
            lastFrameCount = m_transinfo.nFrameCount;
            if(frameCount > 1){
                if(frameCount > m_bufferCapacity){
                    frameCount = m_bufferCapacity;
                }
            }
            for(int i=0;i<frameCount;i++){
                int32 id = (frameIndex - i + m_bufferCapacity)%m_bufferCapacity;
                bufferId.prepend(id);
            }
        }
        m_bufframe.iFrame = bufferId.first();
        bufferId.pop_front();
        err = dcambuf_lockframe(m_hdcam,&m_bufframe);
        if(failed(err)){
            PrintError(m_hdcam,err,"dcambuf_lockframe");
        }
        if(imagesize.empty()){
            imagesize = cv::Size(m_bufframe.width,m_bufframe.height);
            qDebug()<<m_bufframe.width<<" "<<m_bufframe.height;
        }
        void *buffer = malloc(imagesize.area()*2);
        memcpy(buffer,m_bufframe.buf,imagesize.area()*2);
        buffers.append(buffer);
    }
    dcamcap_stop(m_hdcam);
    dcamwait_abort(m_hwait);
    QTime stopCaptime = QTime::currentTime();
    qDebug()<<i<<"camera buffers: "<<buffers.size();
    int Capelapsed = startCaptime.msecsTo(stopCaptime);
    qDebug()<< "camera time:" << Capelapsed <<"ms";


//    OutData* task = new OutData;
//    task->m_buffers = &buffers;
//    //slice每次在界面设置，column在每次column拍照之前设置
//    //有了slice序号(s)  cloumn序号(c)之后，将图像文件名称设置为如下
//    //task->m_path = camInfo.Path + "/Human" + "-Slice-" + QString::number(s) + "-Column-" + QString::number(a) + ".ome.tif";
//    task->m_path = camInfo.Path+"/test.ome.tif";        //考虑路径名称的设置
//    task->m_width = m_bufframe.width;
//    task->m_height = m_bufframe.height;
//    QThreadPool::globalInstance()->start(task);

    QTime starttime = QTime::currentTime();
    output_data(buffers);
    QTime stoptime = QTime::currentTime();
    qDeleteAll(buffers);
    buffers.clear();
    int elapsed = starttime.msecsTo(stoptime);
    qDebug()<< "compress time:" <<elapsed<<"ms";

    //每次拍照结束，不再释放分配的buffer
    dcambuf_release(m_hdcam);
}
*/
void Camera::CamStart1(int i)
{
//    int index = 0;
//    ushort* pData16 = new ushort[camInfo.Width*camInfo.Height];
    QTime startCaptime = QTime::currentTime();
    err = dcamcap_start(m_hdcam,DCAMCAP_START_SEQUENCE);
    if(failed(err)){
        PrintError(m_hdcam,err,"dcamcap_start");
    }
    qDebug()<<"camera"+QString::number(i)<<" start capture";
    qDebug()<<"camera"+QString::number(i)<<" thread address: "<<QThread::currentThread();

//    int lastFrameCount = 0;
//    while(!VISoR::bStopFlag){

    while(!failed(err) || !VISoR::bStopFlag){
            err = dcamwait_start(m_hwait,&m_waitstart);
            if(failed(err)){
                PrintError(m_hdcam,err,"dcamwait_start");
                //if(err == DCAMERR_ABORT)
                    break;
            }
//            err = dcamcap_transferinfo(m_hdcam,&m_transinfo);
//            if(failed(err)){
//                PrintError(m_hdcam,err,"dcamcap_transferinfo");
//            }
//            if(m_transinfo.nFrameCount<1){
//                qDebug()<<"not capture image";
//            }
            //qDebug()<<"test "<<m_transinfo.nFrameCount;

        //dcamprop_getvalue(m_hdcam,DCAM_IDPROP_TIMING_GLOBALEXPOSUREDELAY,&v);
        //qDebug()<<v;

        err = dcambuf_lockframe(m_hdcam,&m_bufframe);
        if(failed(err)){
            PrintError(m_hdcam,err,"dcambuf_lockframe");
        }
        if(imagesize.empty()){
            imagesize = cv::Size(m_bufframe.width,m_bufframe.height);
        }

//        if(index%50 ==0)
//        {
//            //qDebug()<<"to start to live 20ms";
//            int Ii,Ixn;
//            memcpy(pData16,(ushort*)m_bufframe.buf,camInfo.Width*camInfo.Height*2);
//            int Imax = pData16[0],Imin = pData16[0];
//            for (int i = 0; i < camInfo.Width * camInfo.Height; i++)
//            {
//                Ii = pData16[i];
//                if (Ii > Imax)
//                    Imax = Ii;
//                if (Ii < Imin)
//                    Imin = Ii;
//            }
//            Ixn = Imax - Imin;
//            for(int i = 0; i < camInfo.Width * camInfo.Height; i++)
//            {
//                pData8[i] = 0.8 * ((pData16[i] - Imin) * 255 / Ixn);
//            }
//            QImage image = QImage(pData8, camInfo.Width,camInfo.Height, QImage::Format_Grayscale8);  //QImage::Format_Indexed8
//            QImage image2 = image;
//            image2.detach();
//            emit liveImage(image2);
//        };
//        ++index;

        //void *buffer1 = malloc(imagesize.area()*2);
        //memcpy(buffer1,m_bufframe.buf,imagesize.area()*2);
        //emit saveImage(buffer1);
//        void *buffer2 = malloc(imagesize.area()*2);
//        memcpy(buffer2,m_bufframe.buf,imagesize.area()*2);
//        QByteArray b((char*)buffer2,imagesize.area()*2);
//        emit liveImage1(b);
//        free(buffer2);
//        buffer2 = nullptr;

        void *buffer1 = m_bufframe.buf;
        QByteArray a((char*)buffer1,imagesize.area()*2);
        emit saveImage1(a);
    }
    QTime stopCaptime = QTime::currentTime();
    int Capelapsed = startCaptime.msecsTo(stopCaptime);
    qDebug()<< "camera time:" << Capelapsed <<"ms";
    dcamcap_stop(m_hdcam);
    //dcambuf_release(m_hdcam);
    dcamwait_abort(m_hwait);
    //发出信号，释放ImageWriter
    //emit saveImage1(NULL);

//    free(pData16);
}

void Camera::Live1()
{
    //考虑当界面选择的Width和Height改变之后，pData8该如何赋值
    //if(pData8 == nullptr) pData8=new uchar[camInfo.Width*camInfo.Height*2];
    Camera::stopflag = false;
    err = dcamcap_start(m_hdcam,DCAMCAP_START_SEQUENCE);
    if(failed(err)){
        //qDebug()<<"cam"+QString::number(i)<<" error";
        PrintError(m_hdcam,err,"dcamcap_start");
    }
    ushort* pData16 = new ushort[camInfo.Width*camInfo.Height];
    int Ixn;
    while((!failed(err)) && (stopflag == false)){
        err = dcamwait_start(m_hwait,&m_waitstart);
        if(failed(err)){
            PrintError(m_hdcam,err,"dcamwait_start");
            if(err == DCAMERR_ABORT||err == DCAMERR_TIMEOUT) break;

            //break;
        }
        err = dcamcap_transferinfo(m_hdcam,&m_transinfo);
        if(failed(err)){
            PrintError(m_hdcam,err,"dcamcap_transferinfo");
        }
        if(m_transinfo.nFrameCount<1){
            qDebug()<<"not capture image";
        }

        err = dcambuf_lockframe(m_hdcam,&m_bufframe);
        memcpy(pData16,(ushort*)m_bufframe.buf,camInfo.Width*camInfo.Height*2);

//        int Imax = pData16[0],Imin = pData16[0];
//        for (int i = 0; i < camInfo.Width*camInfo.Height; i++)
//        {
//            Ii = pData16[i];
//            if (Ii > Imax)
//                Imax = Ii;
//            if (Ii < Imin)
//                Imin = Ii;
//        }
//        Ixn = Imax - Imin;
//        if(Ixn == 0) break;


//        for(int i = 0; i < camInfo.Width*camInfo.Height; i++)
//        {
//            pData8[i] = ((pData16[i] - Imin) * 255 / Ixn);  //Imax - pData16[i]         pData16[i] - Imin
//        }
/*
        //正常显示
        if(NormalBrightness | setBrightness){
            for(int i = 0; i < camInfo.Width*camInfo.Height; i++)
            {
                pData8[i] = ((pData16[i] - Imin) * 255 / Ixn);  //Imax - pData16[i]         pData16[i] - Imin
            }
            if(setBrightness){
                //设置图像亮度为滑块设置的值
                for(int i=0;i<camInfo.Width*camInfo.Height; i++){
                    pData8[i] = 0.01 * (contrast::contrastness) * pData8[i] + (BrightnessSet::Brightness/255);
                }
            }
        }

        //判断BrightnessSet的参数值，来对图像显示进行处理，自动对比度显示
        if(AutoBrightness){
            //对图像数据进行处理
            for(int i = 0; i < camInfo.Width*camInfo.Height; i++)
            {
//                if(pData16[i]<Imin){
//                    pData8[i] = 0;
//                }else if(pData16[i]>Imax){
//                    pData8[i] = 255;
//                }else{
//                    pData8[i] = ((pData16[i] - Imin) * 255 / Ixn);  //Imax - pData16[i]         pData16[i] - Imin
//                }
                pData8[i] = 0.8*((pData16[i] - Imin) * 255 / Ixn);
            }
        }
*/
        Ixn = CameraContrast::maxPixel - CameraContrast::minPixel;
        for(int i = 0; i < camInfo.Width*camInfo.Height; i++)
        {
            pData16[i] = (pData16[i] > CameraContrast::maxPixel)? CameraContrast::maxPixel : pData16[i];
            pData16[i] = (pData16[i] < CameraContrast::minPixel)? CameraContrast::minPixel : pData16[i];
            pData8[i] = ((pData16[i] - CameraContrast::minPixel) * 255 / Ixn);  //Imax - pData16[i]         pData16[i] - Imin
        }
        QImage image = QImage(pData8, camInfo.Width,camInfo.Height, QImage::Format_Grayscale8);  //Format_Indexed8
        QImage image2 = image;
        image2.detach();
        emit transImage(image2);
        QThread::msleep(200);
    }
    QThread::sleep(1);
    delete[] pData16;
    pData16 = nullptr;
    qDebug()<<"camera finished live";
    dcamcap_stop(m_hdcam);
    dcambuf_release(m_hdcam);
    dcamwait_close(m_hwait);
}

void Camera::Live2()
{
    Camera::stopflag = false;
    err = dcamcap_start(m_hdcam,DCAMCAP_START_SEQUENCE);
    if(failed(err)){
        PrintError(m_hdcam,err,"dcamcap_start");
    }
    ushort* pData16 = new ushort[camInfo.Width*camInfo.Height];
    int Ixn;
    while((!failed(err)) && (stopflag == false)){
        err = dcamwait_start(m_hwait,&m_waitstart);
        if(failed(err)){
            PrintError(m_hdcam,err,"dcamwait_start");
            if(err == DCAMERR_ABORT||err == DCAMERR_TIMEOUT) break;

            //break;
        }
        err = dcamcap_transferinfo(m_hdcam,&m_transinfo);
        if(failed(err)){
            PrintError(m_hdcam,err,"dcamcap_transferinfo");
        }
        if(m_transinfo.nFrameCount<1){
            qDebug()<<"not capture image";
        }

        err = dcambuf_lockframe(m_hdcam,&m_bufframe);
        memcpy(pData16,(ushort*)m_bufframe.buf,camInfo.Width*camInfo.Height*2);

        Ixn = AnoCameraContrast::anomaxPixel - AnoCameraContrast::anominPixel;
        for(int i = 0; i < camInfo.Width*camInfo.Height; i++)
        {
            pData16[i] = (pData16[i] > AnoCameraContrast::anomaxPixel)? AnoCameraContrast::anomaxPixel : pData16[i];
            pData16[i] = (pData16[i] < AnoCameraContrast::anominPixel)? AnoCameraContrast::anominPixel : pData16[i];
            pData8[i] = ((pData16[i] - AnoCameraContrast::anominPixel) * 255 / Ixn);  //Imax - pData16[i]         pData16[i] - Imin
        }
        QImage image = QImage(pData8, camInfo.Width,camInfo.Height, QImage::Format_Grayscale8);  //Format_Indexed8
        QImage image2 = image;
        image2.detach();
        emit transImage(image2);
        QThread::msleep(200);
    }
    QThread::sleep(1);
    delete[] pData16;
    pData16 = nullptr;
    qDebug()<<"camera finished live";
    dcamcap_stop(m_hdcam);
    dcambuf_release(m_hdcam);
    dcamwait_close(m_hwait);
}
Camera::~Camera()
{
    //qDebug()<<"析构相机类";
    delete[] pData8;
    dcambuf_release(m_hdcam);
}

