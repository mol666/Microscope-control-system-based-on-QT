#include "visor.h"
#include "ui_visor.h"
#include "Component/printerror.h"
#include <QThread>
#include <QTimer>
#include <QMap>
#include <QMutex>
#include <QMessageBox>
#include <QDateTime>
#include <QMimeData>
#include <QRegularExpression>

//停止标识符
bool VISoR::bStopFlag = false;

int VISoR::num = 0;

bool jishi  = true;
QTime timedebug;

//qDebug重定向至控件和txt文件需要的全局变量
QFile VISoR::file("C:/Users/JiadingChen/Desktop/my/build-VISoR-Desktop_Qt_5_15_2_MSVC2019_64bit-Debug/Log.txt");
VISoR* VISoR::m_ptr = nullptr;

VISoR::VISoR(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::VISoR)
{
    //AVpData8=new uchar[2304*2304*2];
    //BVpData8=new uchar[2304*2304*2];
    ui->setupUi(this);
    //qDebug重定向至窗口控件
    m_ptr = this;
    qInstallMessageHandler(debugInfoOutput);
    connect(this,&VISoR::debugInfoOutputed,ui->debugInfo,&QPlainTextEdit::appendPlainText);
    setAcceptDrops(true);

    qDebug()<<"main thread:"<<QThread::currentThread();
    aerotechInit();
    laserInit();
    Camerainit();
    MPinit();

    qRegisterMetaType<QByteArray>("QByteArray");
    //NI控制卡初始化
    m_NIctrl = new NIctrl(this);

    liveTimer = new QTimer(this);
    liveTimer->setTimerType(Qt::PreciseTimer);

    //100ms反馈一次位置参数(这样是否线程安全?)，仅对positionFeedback变量进行读写
    QTimer *feedbackTimer = new QTimer(this);
    feedbackTimer->setTimerType(Qt::PreciseTimer);
    feedbackTimer->start(200);
    connect(feedbackTimer, &QTimer::timeout, this, [=]()
    {
        A3200StatusGetItem(aerotech->handle, aerotech->axisInfo_X.axis_index, STATUSITEM_PositionFeedback, 0, &aerotech->axisInfo_X.positionFeedback);
        A3200StatusGetItem(aerotech->handle, aerotech->axisInfo_Y.axis_index, STATUSITEM_PositionFeedback, 0, &aerotech->axisInfo_Y.positionFeedback);
        A3200StatusGetItem(aerotech->handle, aerotech->axisInfo_Z.axis_index, STATUSITEM_PositionFeedback, 0, &aerotech->axisInfo_Z.positionFeedback);
        ui->X_posFeedback->setText(QString::number(aerotech->axisInfo_X.positionFeedback,'f',4));
        ui->Y_posFeedback->setText(QString::number(aerotech->axisInfo_Y.positionFeedback,'f',4));
        ui->Z_posFeedback->setText(QString::number(aerotech->axisInfo_Z.positionFeedback,'f',4));
        //A3200错误检测
        PrintError();
    });

    //亮度调节下拉框的设置
    ui->Brightness->hide();
    ui->contrastness->hide();
    connect(ui->DisMode,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int index)
    {
        if(index==0||1){
            ui->Brightness->hide();
            ui->contrastness->hide();
            //auto,系统自动调节显示图像对比度
            if(index == 1){
                //设置一个bool类型的全局变量AutoBrightness，在live中判断该参数，为true则设置图像自动对比度输出，否则跳过
                Camera::AutoBrightness = true;
                Camera::NormalBrightness = false;
                Camera::setBrightness = false;
            }else{
                Camera::NormalBrightness = true;
                Camera::AutoBrightness = false;
                Camera::setBrightness = false;
            }

        }
        if(index==2){
            ui->Brightness->show();
            ui->contrastness->show();
            //设置int类型的全局变量Brightness，bool类型的全局变量setBrightness，在live中判断该参数，为true则根据滑块的数值赋值给Brightness，图像整体改变亮度
            Camera::setBrightness = true;
            Camera::NormalBrightness = false;
            Camera::AutoBrightness = false;
        }
    });

    //
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    //平台在起点已准备好，开拍一条col
    connect(aerotech,&Aerotech::aerotechReachStartSignal,this,&VISoR::goOneColumn);
    //一条col拍完
    connect(aerotech,&Aerotech::aerotechOneColumnDoneSignal,this,&VISoR::doneOneColumn);
}

VISoR::~VISoR()
{ 
    delete ui;
    //delete[] AVpData8;
    //delete[] BVpData8;
    //delete[] ApData16;
    //delete[] BpData16;
}

void VISoR::getOutputPath(int pathflag)
{
    QString selectdir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),"/home",QFileDialog::ShowDirsOnly);
    if(pathflag == 1){
        if(!selectdir.isEmpty()){
            ui->Path_C1->setText(selectdir);
        }
    }else if(pathflag == 2){
        if(!selectdir.isEmpty()){
            ui->Path_C2->setText(selectdir);
        }
    }
}

//关闭窗口前提示关闭laser
void VISoR::closeEvent(QCloseEvent *ev)
{
    //A3200MotionMoveInc(aerotech->handle,TASKID_Library,aerotech->axis_Z_index,-30,30);
    aerotech->aerotechStepWork(30,20,Z,posi);
    qDebug()<<"Axis Z is descending...please wait...";
    if(!m_laserSerial.empty()
        &&(ui->pushButton_637OnOff->text()=="Stop"
        ||ui->pushButton_561OnOff->text()=="Stop"
        ||ui->pushButton_488OnOff->text()=="Stop"
        ||ui->pushButton_404OnOff->text()=="Stop"))
    {
        QMessageBox::Button btn = QMessageBox::question(this, "Close window", "Power off all the lasers?");
        if(btn == QMessageBox::Yes) emit stopOBIS();
    }
    A3200MotionWaitForMotionDone(aerotech->handle,aerotech->axisInfo_Z.axis_mask,WAITOPTION_MoveDone,-1,NULL);
    ev->accept();
}
//拖拽border文件
void VISoR::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasUrls())                    // 数据中是否包含URL
            event->acceptProposedAction();              // 如果是则接收动作
    else event->ignore();                               // 否则忽略该事件

}
//导入border文件
void VISoR::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();      // 获取MIME数据
    if(mimeData->hasUrls())                             // 如果数据中包含URL
    {
        QList<QUrl> urlList = mimeData->urls();         // 获取URL列表
        // 将其中第一个URL表示为本地文件路径
        QString fileName = urlList.at(0).toLocalFile();
        if(!fileName.isEmpty())                         // 如果文件路径不为空
        {
            QFile file(fileName);                       // 建立QFile对象并且以只读方式打开该文件
            if(!file.open(QIODevice::ReadOnly|QIODevice::Text)) return;
            QTextStream inTxt(&file);                   // 建立文本流对象
            imInfo.paraFromBorder.clear();              // 清空存储参数的列表
            //读取文件内的参数
            qDebug()<<"***Import parameters***";
            while(!inTxt.atEnd())
            {
                QString str;
                str = inTxt.readLine();
                qDebug() << qPrintable(str);
                if(str.startsWith("#"))
                {
                    struct motionPara temp;
                    str.remove(QRegularExpression("[(,)#]"));
                    if(str.startsWith("0"))        //第0行坐标数据
                    {
                        temp = motionPara(str.section(" ",1,1).toDouble(),
                                          str.section(" ",2,2).toDouble(),
                                          str.section(" ",3,3).toDouble(),
                                          str.section(" ",4,4).toDouble());
                    }
                    else
                    {
                        temp = motionPara(str.section(" ",1,1).toDouble(),
                                          str.section(" ",2,2).toDouble(),
                                          str.section(" ",3,3).toDouble(),
                                          str.section(" ",4,4).toDouble(),
                                          str.section(" ",5,5).toDouble(),
                                          str.section(" ",6,6).toDouble());

                    }
                    imInfo.paraFromBorder.append(temp);
                }
            }
            file.close();
            imInfo.totalColums = imInfo.paraFromBorder.length()-1;
            qDebug().noquote()<<"import"<<((imInfo.totalColums==0)?"quick":"slow")<<"scan parameters!";
        }
    }
}

//qDebug内容重定向至控件QPlainTextEdit
void VISoR::debugInfoOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);

    static QMutex mutex;
    mutex.lock();
    //输出到日志文件

    //输出到UI控件
    emit m_ptr->debugInfoOutputed(msg);
    mutex.unlock();
}

/*********************边拍边存******************************/
void VISoR::testImagewriterInit(QString path,int i,Camera* &cam){
//    if(i>1) cam->disconnect(writer[(i%20)-2]);
//    if((i%20)>=4) cam->disconnect(writer[((i%20)-4)]);
//    if((i%20)==0&&i>0) cam->disconnect(writer[16]);
//    if(((i-1)%20)==0&&i>1) cam->disconnect(writer[17]);
//    if(((i-2)%20)==0&&i>2) cam->disconnect(writer[18]);
//    if(((i-3)%20)==0&&i>3) cam->disconnect(writer[19]);

    writer[i%20] = new flsmio::ImageWriter(path,imInfo.totalColums);
    thread[i%20] = new QThread;
    writer[i%20]->moveToThread(thread[i%20]);
    thread[i%20]->start();
//    connect(cam,&Camera::saveImage,writer[i%20],[=](void* buffer){
//        if(buffer){
//            writer[i%20]->addImage(cv::Mat(cv::Size((int)cam->camInfo.Width,(int)cam->camInfo.Height),CV_16UC1,buffer));
//            free(buffer);
//            buffer = nullptr;
//        }else{
//            thread[i%20]->quit();
//            thread[i%20]->wait();
//            writer[i%20]->deleteLater();
//            thread[i%20]->deleteLater();
//            cam->disconnect(writer[i%20]);
//        }
//    },Qt::QueuedConnection);
//    qDebug()<<"this is a first spot";
    connect(cam,&Camera::saveImage1,writer[i%20],[=](QByteArray a){
//        if(!a.isNull()){
            /*
//            qDebug()<<"this is a second spot";
//            addimage* task = new addimage(cv::Mat(cv::Size((int)cam->camInfo.Width,(int)cam->camInfo.Height),CV_16UC1,a.data()),
//                    writer[i%20]->m_bDummpy,writer[i%20]->m_file,writer[i%20]->m_images,writer[i%20]->m_totalOffset);
//            writer[i%20]->pool.start(task);
*/
            writer[i%20]->addImage(cv::Mat(cv::Size((int)cam->camInfo.Width,(int)cam->camInfo.Height),CV_16UC1,a.data()));

//            if(jishi){
//                timedebug.start();
//                jishi = false;
//            }

/*
        }else{
            thread[i%20]->quit();
            thread[i%20]->wait();
            writer[i%20]->deleteLater();
            thread[i%20]->deleteLater();
            //cam->disconnect(writer[i%20]);

//            qDebug()<<"store time:"<<timedebug.elapsed()<<"ms";
//            jishi = true;
        }*/
    },Qt::QueuedConnection);
    connect(this, &VISoR::destroyed, this, [=](){
        thread[i%20]->quit();
        thread[i%20]->wait();
        thread[i%20]->deleteLater();
        delete writer[i%20];
    });
}

//void VISoR::savedata1(QString path){
//    flsmio::ImageWriter* w1 = new flsmio::ImageWriter(path);
//    connect(cam1,&Camera::saveImage1,w1,[=](QByteArray a){
//        if(!a.isNull()){
//            w1->addImage(cv::Mat(cv::Size((int)cam1->camInfo.Width,(int)cam1->camInfo.Height),CV_16UC1,a.data()));
//        }else{
//            delete w1;
//        }
//    },Qt::QueuedConnection);
//}
//void VISoR::savedata2(QString path){
//    flsmio::ImageWriter* w2 = new flsmio::ImageWriter(path);
//    connect(cam2,&Camera::saveImage1,w2,[=](QByteArray a){
//        if(!a.isNull()){
//            w2->addImage(cv::Mat(cv::Size((int)cam2->camInfo.Width,(int)cam2->camInfo.Height),CV_16UC1,a.data()));
//        }else{
//            delete w2;
//        }
//    },Qt::QueuedConnection);
//}

void VISoR::TostartToLive1(QImage image)
{
    //qDebug()<<"111111111111111111111111";
    QImage result =image.scaled(ui->screen->splitlabel1->width(), ui->screen->splitlabel1->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    ui->screen->splitlabel1->setPixmap(QPixmap::fromImage(result));
    //QThread::msleep(50);
    //qDebug()<<"camera finished to start to live";
}

void VISoR::TostartToLive2(QImage image)
{
    QImage result =image.scaled(ui->screen->splitlabel2->width(), ui->screen->splitlabel2->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).mirrored(false,true);
    ui->screen->splitlabel2->setPixmap(QPixmap::fromImage(result));
}

void VISoR::Camerainit()
{
    qRegisterMetaType<cameraInfo>("cameraInfo");
    cam1 = new Camera;
    cam2 = new Camera;
    //初始化相机，并打开相机
    dcamcon_init();
    if(!idCamera[0].isEmpty()) {
        ui->Camera_0->setTitle(idCamera[0]);
    }
    if(!idCamera[1].isEmpty()) {
        ui->Camera_1->setTitle(idCamera[1]);
    }
    cam1->m_hdcam = dcamcon_open(idDevice[0]);
    cam2->m_hdcam = dcamcon_open(idDevice[1]);

    QThread* t1 = new QThread;
    QThread* t2 = new QThread;
    cam1->moveToThread(t1);
    cam2->moveToThread(t2);
    t1->start();
    t2->start();
    connect(this,&VISoR::setCamVal1,cam1,&Camera::setCamVal);
    connect(this,&VISoR::setCamVal2,cam2,&Camera::setCamVal);
    connect(this,&VISoR::startCap1,cam1,&Camera::CamStart1);
    connect(this,&VISoR::startCap2,cam2,&Camera::CamStart1);
    connect(this,&VISoR::startLive,cam1,&Camera::Live1);
    connect(this,&VISoR::startLive,cam2,&Camera::Live2);
    connect(cam1,&Camera::liveImage,this,&VISoR::TostartToLive1);
    connect(cam2,&Camera::liveImage,this,&VISoR::TostartToLive2);
    connect(this, &VISoR::destroyed, this, [=](){
        t1->quit();
        t1->wait();
        t1->deleteLater();

        t2->quit();
        t2->wait();
        t2->deleteLater();

        dcamdev_close(cam1->m_hdcam);
        dcamdev_close(cam2->m_hdcam);
        dcamapi_uninit();
        delete cam1;
        delete cam2;
    });
}

void VISoR::MPinit(){
    MP = new MergePlay;
    QThread * MPthread = new QThread;
    MP->moveToThread(MPthread);
    MPthread->start();
    connect(this,&VISoR::chuandiImage,MP,&MergePlay::rongheImage);
    connect(this, &VISoR::destroyed, this, [=](){
        MPthread->quit();
        MPthread->wait();
        MPthread->deleteLater();
        delete MP;
    });
}

/***********************************激光相关*****************************************/
void VISoR::laserInit()
{
    imInfo.m_laserInfo[404] = QString::number(ui->spinBox_404->value()*0.001,'f',3);
    imInfo.m_laserInfo[488] = QString::number(ui->spinBox_488->value()*0.001,'f',3);
    imInfo.m_laserInfo[561] = QString::number(ui->spinBox_561->value()*0.001,'f',3);
    imInfo.m_laserInfo[637] = QString::number(ui->spinBox_637->value()*0.001,'f',3);
//    qDebug().noquote()<<"404 power:"<<imInfo.m_laserInfo[404]<<"mW";
//    qDebug("404: %s mW",qUtf8Printable(imInfo.m_laserInfo[404]));
    //初始化激光器，并传入功率设定值
    foreach(const QSerialPortInfo &port, QSerialPortInfo::availablePorts())
    {
//        qDebug()<<port.portName();
//        qDebug()<<port.description();
//        qDebug()<<port.manufacturer();
        if(port.manufacturer()=="Coherent, Inc."
        &&port.description()=="Coherent OBIS Device"
        &&!m_laserSerial.contains(port.portName()))
        {
            //每有一个OBIS激光器，就创建一个子线程
            OBISCtrl *p = new OBISCtrl(port.portName(),imInfo.m_laserInfo);
            m_laserSerial[port.portName()] = p;

            QThread *t = new QThread;
            m_serialThread[port.portName()] = t;
            p->moveToThread(t);
            t->start();
            //初始化这个激光器
            connect(this,&VISoR::initOBIS,p,&OBISCtrl::OBISinit);
            emit initOBIS(port.portName());
            //接收初始化成功信号
            connect(p,&OBISCtrl::OBISInitSuccessSignal,this,[=](int w)
            {
               switch(w)
               {
                   case 404:ui->pushButton_404OnOff->setEnabled(true);break;
                   case 488:ui->pushButton_488OnOff->setEnabled(true);break;
                   case 561:ui->pushButton_561OnOff->setEnabled(true);break;
                   case 637:ui->pushButton_637OnOff->setEnabled(true);break;
                   default:break;
               }
            });
            //连接开关功能
            connect(this,&VISoR::onoffOBIS,p,&OBISCtrl::OBISonoff);
            //连接功率设置功能
            connect(this,&VISoR::setPowerOBIS,p,&OBISCtrl::OBISsetPower);
            //关闭窗口前关闭激光器
            connect(this,&VISoR::stopOBIS,p,&OBISCtrl::OBISstop);
            //释放线程，关闭窗口释放
            connect(this, &VISoR::destroyed, this, [=]()
            {
                t->quit();
                t->wait();
                t->deleteLater();

                p->deleteLater();
                qDebug()<<"delete"<<port.portName()<<"thread";
            });
        }
    }
    //检测是否有新激光器连接
    connect(ui->pushButton_laserUpdate,&QPushButton::clicked,this,&VISoR::laserInit);
}
/*************以下是激光器的开关按键和功率改变*************/
void VISoR::on_pushButton_404OnOff_clicked()
{emit onoffOBIS(404,(ui->pushButton_404OnOff->text()=="Start")?true:false);
ui->pushButton_404OnOff->setText((ui->pushButton_404OnOff->text()=="Start")?"Stop":"Start");}
void VISoR::on_pushButton_488OnOff_clicked()
{emit onoffOBIS(488,(ui->pushButton_488OnOff->text()=="Start")?true:false);
ui->pushButton_488OnOff->setText((ui->pushButton_488OnOff->text()=="Start")?"Stop":"Start");}
void VISoR::on_pushButton_561OnOff_clicked()
{emit onoffOBIS(561,(ui->pushButton_561OnOff->text()=="Start")?true:false);
ui->pushButton_561OnOff->setText((ui->pushButton_561OnOff->text()=="Start")?"Stop":"Start");}
void VISoR::on_pushButton_637OnOff_clicked()
{emit onoffOBIS(637,(ui->pushButton_637OnOff->text()=="Start")?true:false);
ui->pushButton_637OnOff->setText((ui->pushButton_637OnOff->text()=="Start")?"Stop":"Start");}
void VISoR::on_spinBox_404_valueChanged(int arg1){emit setPowerOBIS(404,QString::number(arg1*0.001,'f',3));}
void VISoR::on_spinBox_488_valueChanged(int arg1){emit setPowerOBIS(488,QString::number(arg1*0.001,'f',3));}
void VISoR::on_spinBox_561_valueChanged(int arg1){emit setPowerOBIS(561,QString::number(arg1*0.001,'f',3));}
void VISoR::on_spinBox_637_valueChanged(int arg1){emit setPowerOBIS(637,QString::number(arg1*0.001,'f',3));}
/*************以上是激光器的开关按键和功率改变*************/
/***********************************平台相关*****************************************/
void VISoR::aerotechInit()
{
    QThread* AerotechThread = new QThread;
    //Aerotech* aerotech = new Aerotech;
    aerotech = new Aerotech;
    //初始化完成，开始子线程
    aerotech->moveToThread(AerotechThread);
    AerotechThread->start();
    //在子线程中初始化平台
    connect(this,&VISoR::initAerotech,aerotech,&Aerotech::aerotechInit);
    //初始化成功，则使能所有stage操作相关按键
    connect(aerotech,&Aerotech::aerotechInitSuccessSignal,this,[this](bool sig){
       if(!sig&&QMessageBox::Yes == QMessageBox::question(this,"A3200 Mode","A3200 is in virtual mode, reset the controller?"))
       {
           //检测到虚拟仿真模式，询问是否reset
            A3200Reset(aerotech->handle);
            qDebug()<<"A3200 error now reseting...";
            emit initAerotech();
       }else
       {
           ui->pushButton_from->setEnabled(true);
           ui->pushButton_goto->setEnabled(true);
           ui->pushButton_home->setEnabled(true);
           ui->pushButton_stageAbort->setEnabled(true);
           ui->direction->StepButton(true);
       }
    });
    emit initAerotech();
    //释放线程
    connect(this, &VISoR::destroyed, this, [=]()
    {
        AerotechThread->quit();
        AerotechThread->wait();
        AerotechThread->deleteLater();  // delete t1;
        delete aerotech;
    });

    stageDialogFromInit();
    stageDialogGotoInit();
    /*  aerotech的3种运动模式
     *  0.两个按键Home和StageAbort
     *  1.点动，与Direction有关
     *  2.goto定位，点开gotoDialog
     *  3.预设fromto运动，起点、终点、速度，等待start信号开始运动
     */

    /***运动模式0：aerotech的home和abort操作***/
    //按下home键，平台重新home，home期间按键不可用，直到home完成
    connect(ui->pushButton_home,&QPushButton::clicked,aerotech,&Aerotech::aerotechHomeAgain);
    connect(ui->pushButton_home,&QPushButton::clicked,this,[this]()
    {
        ui->pushButton_from->setEnabled(false);
        ui->pushButton_goto->setEnabled(false);
        ui->direction->StepButton(false);
        ui->pushButton_stageAbort->setEnabled(false);

    });
    connect(aerotech,&Aerotech::aerotechHomeSuccessSignal,this,[this]()
    {
        ui->pushButton_from->setEnabled(true);
        ui->pushButton_goto->setEnabled(true);
        ui->direction->StepButton(true);
        ui->pushButton_stageAbort->setEnabled(true);
    });
    //按下abort键，平台终止当前运动
    connect(this,&VISoR::abortAerotech,aerotech,&Aerotech::aerotechAbort);
    connect(ui->pushButton_stageAbort,&QPushButton::clicked,this,[=]()
    {
        //只能终止fromtowork和startwork的运动
        Aerotech::bStageAbortFlag = true;
        //能终止stepwork和gotowork的运动
        emit abortAerotech();

    });

    /***运动模式1：UI按键和aerotech的相关操作***/
    //从direction对象接受信号，点动
    connect(ui->direction,&Direction::setAerotechStepPara,aerotech,&Aerotech::aerotechStepWork);

    /***运动模式2：gotoDialog和aerotech的相关操作***/
    //从gotoDialog接收信号，并传至aerotech子线程运行
    //1.realTime模式，参数是单根轴传递的
    void (StageDialog_Goto::*func1)(double,int) = &StageDialog_Goto::setAerotechGoToPara;
    connect(gotoDialog,func1,aerotech,&Aerotech::aerotechGotoWork);
    //2.非realTime模式，参数是三根轴同时传递的
    void (StageDialog_Goto::*func2)(double,double,double) = &StageDialog_Goto::setAerotechGoToPara;
    connect(gotoDialog,func2,this,[=](double paraX,double paraY,double paraZ)
    {
        if(aerotech->aerotechPosLock(moveToSetPos,paraX,paraY,paraZ))
        {
            aerotech->axisInfo_X.setPosition = paraX;
            aerotech->axisInfo_Y.setPosition = paraY;
            aerotech->axisInfo_Z.setPosition = paraZ;
            emit runAerotech(moveToSetPos);
        }else
        QMessageBox::warning(this,"Parameters Error","Aerotech cant move to set position safely!");
    });

    /***运动模式3：fromtoDialog和aerotech的相关操作***/
    //0.该模式下，先通过fromtodialog设定参数，但平台并不会动，等待按下UI中的start按键后，平台会运动到起点
    //1.此信号槽和start按键有关，按下start会发送check信号让平台运动到起点
    //2.然后给信号到aerotech线程,aerotech从起点运动到终点
    connect(this,&VISoR::runAerotech,aerotech,&Aerotech::aerotechRun);

    /*在平台fromtoWork和startWork期间，暂时失能UI中的stepmove按键*/
    connect(aerotech,&Aerotech::setStepButton,ui->direction,&Direction::StepButton);
}
/***以下是fromtoDialog和aerotech的相关操作***/
void VISoR::stageDialogFromInit()
{
    fromDialog = new StageDialog_From(this);
    //注册motionInfo Type，用于信号与槽传参
    qRegisterMetaType<motionPara>("motionPara");
    //fromDialog给信号到主UI，在UI显示XYZ三轴设定的运动起点终点和速度
    connect(fromDialog,&StageDialog_From::setAerotechFromToPara,this,[=](motionPara para)
    {
        bool Start = aerotech->aerotechPosLock(moveToStartPos,para.motionStart[X],para.motionStart[Y],para.motionStart[Z]);
        bool End = true;
        if(Start)//运动到起点参数检测
        {
            //这里只做UI显示并传值
            ui->X_startPos->setText(QString::number(para.motionStart[X],'f',4));
            ui->X_vel->setText(QString::number(para.velocity[X],'f',4));

            ui->Y_startPos->setText(QString::number(para.motionStart[X],'f',4));
            ui->Y_vel->setText(QString::number(para.velocity[X],'f',4));

            ui->Z_startPos->setText(QString::number(para.motionStart[Z],'f',4));
            ui->Z_vel->setText(QString::number(para.velocity[Z],'f',4));

            aerotech->axisInfo_X.startPosition = para.motionStart[X];
            aerotech->axisInfo_X.setVelocity = para.velocity[X];

            aerotech->axisInfo_Y.startPosition = para.motionStart[Y];
            aerotech->axisInfo_Y.setVelocity = para.velocity[Y];

            aerotech->axisInfo_Z.startPosition = para.motionStart[Z];
            aerotech->axisInfo_Z.setVelocity = para.velocity[Z];
            End = aerotech->aerotechPosLock(moveToEndPos,para.motionEnd[X],para.motionEnd[Y],para.motionEnd[Z]);
            if(End)
            {
                ui->X_endPos->setText(QString::number(para.motionEnd[X],'f',4));
                ui->Y_endPos->setText(QString::number(para.motionEnd[Y],'f',4));
                ui->Z_endPos->setText(QString::number(para.motionEnd[Z],'f',4));
                aerotech->axisInfo_X.endPosition = para.motionEnd[X];
                aerotech->axisInfo_Y.endPosition = para.motionEnd[Y];
                aerotech->axisInfo_Z.endPosition = para.motionEnd[Z];
                return;
            }
        }
        //弹出消息框，提示参数设置有误，并不进行参数赋值
        QMessageBox::warning(this,"Parameters Error",
        "Aerotech cant move to "+ QString::fromStdString((Start==false)?"start":"end")+" position safely!");
    });

    //UI按键和dialog显示相连
    //弹出设定起点和终点位置的dialog
    connect(ui->pushButton_from,&QPushButton::clicked,fromDialog,&StageDialog_From::exec);

}
/***以下是gotoDialog和aerotech的相关操作***/
void VISoR::stageDialogGotoInit()
{
    gotoDialog = new StageDialog_Goto(this);
    //发射renew信号，dialog接收renew信号
    connect(this,&VISoR::gotoDialogRenew,gotoDialog,&StageDialog_Goto::gotoDialogRenew);
    connect(aerotech,&Aerotech::gotoDialogRenew,gotoDialog,&StageDialog_Goto::gotoDialogRenew);
    //UI按键和dialog显示相连
    connect(ui->pushButton_goto,&QPushButton::clicked,this,[this]()
    {
        //将最新的位置信息传给dialog
        emit gotoDialogRenew(aerotech->axisInfo_X.nowPosition,aerotech->axisInfo_Y.nowPosition,aerotech->axisInfo_Z.nowPosition);
        //弹出goto位置的dialog
        gotoDialog->exec();
    });
}


/***********************************平台相关*****************************************/


/***以下是debug栏，NI控制卡按键***/
void VISoR::on_pushButton_NIPause_clicked()
{
    m_NIctrl->pauseNICtr();
}

void VISoR::on_pushButton_NIStop_clicked()
{
    m_NIctrl->stop();
}

void VISoR::on_pushButton_NIstartFiniteCount_clicked()
{
    m_NIctrl->NIstart((unsigned long long int)ui->spinBox_NIctrSamCount->value());
}

void VISoR::on_pushButton_setInterval_clicked()
{
    m_NIctrl->stop();
    m_NIctrl->setInterval((float)ui->doubleSB_setInterval->value());
    QMap<int,bool> test_wavelength;
    test_wavelength[404]=true;
    test_wavelength[488]=true;
    test_wavelength[561]=true;
    test_wavelength[637]=true;
    m_NIctrl->NIinit(2,1,test_wavelength);
}

void VISoR::on_pushButton_NIInit_clicked()
{
    m_NIctrl->stop();
    QMap<int,bool> test_wavelength;
    test_wavelength[404]=true;
    test_wavelength[488]=true;
    test_wavelength[561]=true;
    test_wavelength[637]=true;
    m_NIctrl->NIinit(2,1,test_wavelength);
}


/*********************运行拍照相关代码******************/
void VISoR::showMes()
{
    QString h = "\n";

    QString mode = "Mode: "+QString::fromStdString((imInfo.paraFromBorder.size()>0)?((imInfo.paraFromBorder.size()<=1)?"quick scan":"slow scan"):"single column debug")+h;
    QString etime = "Exposure Time: "+ui->exposure_C1->text()+h;
    QString TotalColumn = "TotalColumn: "+QString::number(imInfo.totalColums)+h;
    QString region_C1 = "Camera1 region: "+ui->Width_C1->text() + "*" +ui->Height_C1->text()+h;
    QString region_C2 = "Camera2 region: "+ui->Width_C2->text() + "*" +ui->Height_C2->currentText()+h;
    QString Path_C1 = "Camera1 Path: "+ ui->Path_C1->text()+h;
    QString Path_C2 = "Camera2 Path: "+ ui->Path_C2->text()+h;

    m_Message = mode + etime + TotalColumn + region_C1 + region_C2 + Path_C1 + Path_C2;

    m_Message += "X axis moves from "+QString::number(imInfo.paraFromBorder[0].motionStart[X])+"mm to "+QString::number(imInfo.paraFromBorder[0].motionEnd[X])+"mm"+h;
    m_Message += "X axis speed: "+QString::number(imInfo.velocity_X)+"mm/s"+h;
    m_Message += "Y axis moves from "+QString::number(imInfo.paraFromBorder[0].motionStart[Y])+"mm to "+QString::number(imInfo.paraFromBorder[0].motionEnd[Y])+"mm"+h;
    m_Message += "NI count: "+QString::number(imInfo.count);
    m_Message += "Use following lasers: "+h;
    m_Message += (imInfo.wavelength_sel[404]==true)?("404: "+imInfo.m_laserInfo[404]+"mW"+h):"";
    m_Message += (imInfo.wavelength_sel[488]==true)?("488: "+imInfo.m_laserInfo[488]+"mW"+h):"";
    m_Message += (imInfo.wavelength_sel[561]==true)?("561: "+imInfo.m_laserInfo[561]+"mW"+h):"";
    m_Message += (imInfo.wavelength_sel[637]==true)?("637: "+imInfo.m_laserInfo[637]+"mW"+h):"";

}

//start按键，平台、相机、NI控制卡开始
void VISoR::on_pushButton_start_clicked()
{
    imInfo.amplitude=ui->lineEdit_amplitude->text().toDouble();
    imInfo.offset=ui->lineEdit_offset->text().toDouble();
    imInfo.interval=ui->doubleSB_setInterval->text().toFloat();

    if(imInfo.totalColums>=0)
    {
        if(imInfo.totalColums==0) imInfo.nowCol = 0;    //导入了border文件，且快扫
        else
        {
            imInfo.nowCol = 1;                         //导入了border文件，且慢扫
        }
        aerotech->axisInfo_X.startPosition = imInfo.paraFromBorder[imInfo.nowCol].motionStart[X];
        aerotech->axisInfo_X.endPosition = imInfo.paraFromBorder[imInfo.nowCol].motionEnd[X];
        aerotech->axisInfo_Y.startPosition = imInfo.paraFromBorder[imInfo.nowCol].motionStart[Y];
        //aerotech->axisInfo_Y.endPosition = aerotech->axisInfo_Y.startPosition;
        aerotech->axisInfo_Y.endPosition = imInfo.paraFromBorder[imInfo.nowCol].motionEnd[Y];
        aerotech->axisInfo_Z.startPosition = imInfo.paraFromBorder[imInfo.nowCol].motionStart[Z];
        //aerotech->axisInfo_Z.endPosition = aerotech->axisInfo_Z.startPosition;
        aerotech->axisInfo_Z.endPosition = imInfo.paraFromBorder[imInfo.nowCol].motionEnd[Z];

        if(imInfo.totalColums!=0)
        {
            aerotech->axisInfo_Z.setVelocity = qAbs((aerotech->axisInfo_Z.endPosition-aerotech->axisInfo_Z.startPosition)/(aerotech->axisInfo_X.endPosition-aerotech->axisInfo_X.startPosition)*aerotech->axisInfo_X.setVelocity);
        }

    }

    imInfo.startPos_X=  aerotech->axisInfo_X.startPosition;
    imInfo.endPos_X=    aerotech->axisInfo_X.endPosition;
    imInfo.velocity_X=  aerotech->axisInfo_X.setVelocity;


    imInfo.wavelength_sel[404] =    (ui->pushButton_404OnOff->text()=="Stop")?true:false;
    imInfo.wavelength_sel[488] =    (ui->pushButton_488OnOff->text()=="Stop")?true:false;
    imInfo.wavelength_sel[561] =    (ui->pushButton_561OnOff->text()=="Stop")?true:false;
    imInfo.wavelength_sel[637] =    (ui->pushButton_637OnOff->text()=="Stop")?true:false;

    imInfo.m_laserInfo[404] =   ui->spinBox_404->text();
    imInfo.m_laserInfo[488] =   ui->spinBox_488->text();
    imInfo.m_laserInfo[561] =   ui->spinBox_561->text();
    imInfo.m_laserInfo[637] =   ui->spinBox_637->text();

    imInfo.count = qAbs(imInfo.endPos_X-imInfo.startPos_X)/imInfo.velocity_X/imInfo.interval*1000;

    showMes();
    if(QMessageBox::Cancel == QMessageBox::question(this,"System Start",m_Message,QMessageBox::Ok|QMessageBox::Cancel,QMessageBox::Ok))
    {
        qDebug()<<"cancel mission";
        return;
    }
    getInitData(0);
    getInitData(1);
/*
    if(ApData16 != nullptr){
        ApData16 = new ushort[imInfo.CamInfo[0].Width * imInfo.CamInfo[0].Height];
    }
    if(BpData16 != nullptr){
        BpData16 = new ushort[imInfo.CamInfo[0].Width * imInfo.CamInfo[0].Height];
    }
    connect(cam1,&Camera::liveImage1,ui->screen->splitlabel1,[=](QByteArray a){
        QImage result =image.scaled(ui->screen->splitlabel1->width(), ui->screen->splitlabel1->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        ui->screen->splitlabel1->setPixmap(QPixmap::fromImage(result));
            qDebug()<<"this is a A point";
        int Ii,Ixn;
        ushort* data = (ushort*)a.data();
        memcpy(ApData16,(ushort*)data,imInfo.CamInfo[0].Width * imInfo.CamInfo[0].Height *2);
        int Imax = ApData16[0],Imin = ApData16[0];
        for (int i = 0; i < imInfo.CamInfo[0].Width * imInfo.CamInfo[0].Height; i++)
        {
            Ii = ApData16[i];
            if (Ii > Imax)
                Imax = Ii;
            if (Ii < Imin)
                Imin = Ii;
        }
        Ixn = Imax - Imin;
        for(int i = 0; i < imInfo.CamInfo[0].Width * imInfo.CamInfo[0].Height; i++)
        {
            AVpData8[i] = 0.8 * ((ApData16[i] - Imin) * 255 / Ixn);
        }
        QImage image = QImage(AVpData8, imInfo.CamInfo[0].Width,imInfo.CamInfo[0].Height, QImage::Format_Grayscale8);  //QImage::Format_Indexed8
        QImage result =image.scaled(ui->screen->splitlabel1->width(), ui->screen->splitlabel1->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        ui->screen->splitlabel1->setPixmap(QPixmap::fromImage(result));
    });
    connect(cam2,&Camera::transImage,ui->screen->splitlabel2,[=](QImage image){
        QImage result =image.scaled(ui->screen->splitlabel2->width(), ui->screen->splitlabel2->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).mirrored(false,true);
        ui->screen->splitlabel2->setPixmap(QPixmap::fromImage(result));
            qDebug()<<"this is a B point";
            int Ii,Ixn;
            memcpy(BpData16,(ushort*)data,imInfo.CamInfo[1].Width * imInfo.CamInfo[1].Height *2);
            int Imax = BpData16[0],Imin = BpData16[0];
            for (int i = 0; i < imInfo.CamInfo[1].Width * imInfo.CamInfo[1].Height; i++)
            {
                Ii = BpData16[i];
                if (Ii > Imax)
                    Imax = Ii;
                if (Ii < Imin)
                    Imin = Ii;
            }
            Ixn = Imax - Imin;
            for(int i = 0; i < imInfo.CamInfo[1].Width * imInfo.CamInfo[1].Height; i++)
            {
                BVpData8[i] = 0.8 * ((BpData16[i] - Imin) * 255 / Ixn);
            }
            QImage image = QImage(BVpData8, imInfo.CamInfo[0].Width,imInfo.CamInfo[0].Height, QImage::Format_Grayscale8);  //QImage::Format_Indexed8
            QImage result =image.scaled(ui->screen->splitlabel2->width(), ui->screen->splitlabel2->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            ui->screen->splitlabel2->setPixmap(QPixmap::fromImage(result));
            free(data);
            data = nullptr;
    });
*/
    QString m_path1 = ui->Path_C1->text()+ "/405-0"  + ".ome.tif";
    QString m_path2 = ui->Path_C2->text()+ "/488-0"  + ".ome.tif";
    testImagewriterInit(m_path1,0,cam1);
    testImagewriterInit(m_path2,1,cam2);


    m_NIctrl->stop();
    m_NIctrl->setInterval(imInfo.interval);
    m_NIctrl->NIinit(imInfo.amplitude,imInfo.offset,imInfo.wavelength_sel);
    qDebug()<<"stage is running to start position...";
    emit runAerotech(moveToStartPos);   //平台运动到起点
}


void VISoR::goOneColumn(bool sig)
{    
    if(sig)
    {
        qDebug()<<"*******\r\nSystem Go!\r\n*******";
//        QString m_path1 = ui->Path_C1->text()+ "/405-" + QString::number(imInfo.nowCol) + ".ome.tif";
//        QString m_path2 = ui->Path_C2->text()+ "/488-" + QString::number(imInfo.nowCol) + ".ome.tif";
//        QString m_path3 = ui->Path_C3->text()+ "/561-" + QString::number(imInfo.nowCol) + ".ome.tif";
//        QString m_path4 = ui->Path_C4->text()+ "/640-" + QString::number(imInfo.nowCol) + ".ome.tif";
//        testImagewriterInit(m_path1,(imInfo.nowCol-1)*2,cam1);
//        testImagewriterInit(m_path2,(imInfo.nowCol-1)*2+1,cam2);
//        testImagewriterInit(m_path3,imInfo.nowCol*4+2,cam3);
//        testImagewriterInit(m_path4,imInfo.nowCol*4+3,cam4);
//        savedata1(m_path1);
//        savedata2(m_path2);
//        QThread::msleep(500);
        VISoR::bStopFlag = false;
        emit startCap1(1);
        emit startCap2(2);
        QThread::msleep(100);
        emit runAerotech(moveToEndPos);
        m_NIctrl->NIstart(imInfo.count);
    }else qDebug()<<"Preparatory phase error!";
}

void VISoR::doneOneColumn(bool sig)
{
    if(sig) //一条column拍完了
    {
        //区分快扫 慢扫 调试
        switch(imInfo.totalColums)
        {
        case -1:    //调试模式
            qDebug()<<"Debug mode: One col done!";
            break;
        case 0:     //快扫模式
            qDebug()<<"Quick scan mode: One col done!";
            if(aerotech->axisInfo_Y.nowPosition<imInfo.paraFromBorder[0].motionEnd[Y])  //还没扫完
            {
                aerotech->axisInfo_Y.startPosition = aerotech->axisInfo_Y.endPosition + 5;
                aerotech->axisInfo_Y.endPosition = aerotech->axisInfo_Y.startPosition;
                double t = aerotech->axisInfo_X.startPosition;
                aerotech->axisInfo_X.startPosition = aerotech->axisInfo_X.endPosition;
                aerotech->axisInfo_X.endPosition = t;
                imInfo.count = qAbs(aerotech->axisInfo_X.endPosition - aerotech->axisInfo_X.startPosition)/aerotech->axisInfo_X.setVelocity/imInfo.interval*1000;
                emit runAerotech(moveToStartPos);
            }
            break;
        default:
            qDebug()<<"Slow scan mode: One col done!";
            if(imInfo.nowCol<imInfo.totalColums)
            {
                VISoR::bStopFlag = true;
                ++imInfo.nowCol;
                //参数修改
                aerotech->axisInfo_X.startPosition = imInfo.paraFromBorder[imInfo.nowCol].motionStart[X];
                aerotech->axisInfo_X.endPosition = imInfo.paraFromBorder[imInfo.nowCol].motionEnd[X];
                aerotech->axisInfo_Y.startPosition = imInfo.paraFromBorder[imInfo.nowCol].motionStart[Y];
                aerotech->axisInfo_Y.endPosition = imInfo.paraFromBorder[imInfo.nowCol].motionEnd[Y];
                aerotech->axisInfo_Z.startPosition = imInfo.paraFromBorder[imInfo.nowCol].motionStart[Z];
                aerotech->axisInfo_Z.endPosition = imInfo.paraFromBorder[imInfo.nowCol].motionEnd[Z];

                aerotech->axisInfo_Z.setVelocity = qAbs((aerotech->axisInfo_Z.endPosition-aerotech->axisInfo_Z.startPosition)/(aerotech->axisInfo_X.endPosition-aerotech->axisInfo_X.startPosition)*aerotech->axisInfo_X.setVelocity);

                //计算NI控制卡的count
                imInfo.count = qAbs(aerotech->axisInfo_X.endPosition - aerotech->axisInfo_X.startPosition)/aerotech->axisInfo_X.setVelocity/imInfo.interval*1000;
                //运动到起点
                VISoR::bStopFlag = false;
                QThread::msleep(100);
                emit runAerotech(moveToStartPos);
                //**************************************************************************/
//                if(QMessageBox::Yes==QMessageBox::question(this,"Continue?","Next col?",QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes))
//                {
//                    VISoR::bStopFlag = false;
//                    emit runAerotech(moveToStartPos);
//                }
//                else imInfo.nowCol = 0;
            }else
            {
                //col全都拍完了
                imInfo.nowCol = 0;
                qDebug()<<"All columns done!";
    
            }
            break;

        }
    }else qDebug()<<"System ended accidentally!";


}

//stop按键，可以停止平台运动
void VISoR::on_pushButton_stop_clicked()
{
    VISoR::bStopFlag = true;
    Aerotech::bStageAbortFlag = true;
    m_NIctrl->stop();
}


/*************相机参数设定相关*****************/
void VISoR::on_fileBtn_C1_clicked(){getOutputPath(1);}
void VISoR::on_fileBtn_C2_clicked(){getOutputPath(2);}

void VISoR::getInitData(int camflag)
{
    //获取相机的参数
    if(camflag == 0){
        //dcambuf_release(cam1->m_hdcam);
        imInfo.CamInfo[camflag].exposure = ui->exposure_C1->text().toDouble();
        imInfo.CamInfo[camflag].pixelSize = ui->pixelSize_C1->text().toDouble();
        imInfo.CamInfo[camflag].waveLength = ui->waveLength_C1->currentIndex();
        imInfo.CamInfo[camflag].filter = ui->Filter_C1->currentIndex();
        imInfo.CamInfo[camflag].Width = ui->Width_C1->text().toDouble();
        imInfo.CamInfo[camflag].Height = ui->Height_C1->text().toDouble();
        imInfo.CamInfo[camflag].Column = ui->column_C1->text().toInt();
        imInfo.CamInfo[camflag].Path = ui->Path_C1->text();
        emit setCamVal1(imInfo.CamInfo[camflag]);

    }else if(camflag == 1){
        //dcambuf_release(cam2->m_hdcam);
        imInfo.CamInfo[camflag].exposure = ui->exposure_C2->text().toDouble();
        imInfo.CamInfo[camflag].pixelSize = ui->pixelSize_C2->text().toDouble();
        imInfo.CamInfo[camflag].waveLength = ui->waveLength_C2->currentIndex();
        imInfo.CamInfo[camflag].filter = ui->Filter_C2->currentIndex();
        imInfo.CamInfo[camflag].Width = ui->Width_C2->text().toDouble();
        imInfo.CamInfo[camflag].Height = ui->Height_C2->currentText().toDouble();
        imInfo.CamInfo[camflag].Column = ui->column_C2->text().toInt();
        imInfo.CamInfo[camflag].Path = ui->Path_C2->text();
        emit setCamVal2(imInfo.CamInfo[camflag]);
    }
    //获取其他部分的参数
}

/********************LIVE模式*********************/

void VISoR::on_pushButton_Live_clicked()
{
    if(ui->pushButton_Live->text() == "live")
    {
        ui->pushButton_Live->setText("abort");
        m_fScale = ui->Width_C1->text().toInt()/ui->Height_C1->text().toInt();
        liveTimer->start(12);
        connect(liveTimer, &QTimer::timeout, this, [=]()
        {
//            ui->screen->splitlabel1->repaint();
//            ui->screen->splitlabel2->repaint();
            ui->screen->mergelabel->repaint();
        });
        if(ui->screen->currentIndex() == 0){
            connect(cam1,&Camera::transImage,ui->screen->splitlabel1,[=](QImage image){
                //QImage result =image.scaled(ui->Width_C1->text().toInt(), ui->Height_C1->text().toInt(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                //ui->screen->splitlabel1->setPixmap(QPixmap::fromImage(image));
                //ui->screen->splitlabel1->repaint();
                ui->screen->splitlabel1->setPixmap(QPixmap::fromImage(image).scaled(ui->screen->splitlabel1->size(),Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
                ui->screen->splitlabel1->setScaledContents(true);
                ui->screen->splitlabel1->resize(ui->Width_C1->text().toInt(),ui->Height_C1->text().toInt());
            },Qt::BlockingQueuedConnection);
            connect(cam2,&Camera::transImage,ui->screen->splitlabel2,[=](QImage image){
                //QImage result =image.scaled(ui->Width_C2->text().toInt(), ui->Height_C2->currentText().toInt(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).mirrored(true,false);
                //ui->screen->splitlabel2->setPixmap(QPixmap::fromImage(image.mirrored(false,true)));
                //ui->screen->splitlabel2->repaint();
                ui->screen->splitlabel2->setPixmap(QPixmap::fromImage(image.mirrored(false,true)).scaled(ui->screen->splitlabel2->size(),Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
                ui->screen->splitlabel2->setScaledContents(true);
                ui->screen->splitlabel2->resize(ui->Width_C2->text().toInt(),ui->Height_C2->currentText().toInt());
            },Qt::BlockingQueuedConnection);
        }else{
            connect(cam1,&Camera::transImage,this,[=](QImage image1){
                merImage[0] =image1;
                if(!merImage[0].isNull() && !merImage[1].isNull()){
//                    QImage image(ui->Width_C1->text().toInt(),ui->Height_C1->text().toInt(),QImage::Format_RGB32);
//                    for(int i=0;i<ui->Width_C1->text().toInt();i++){
//                        for(int j=0;j<ui->Height_C1->text().toInt();j++){
//                            image.setPixel(i,j,qRgb(merImage[0].pixel(i,j),merImage[1].pixel(i,j),0));
//                        }
//                    }
//                    QImage result =image.scaled(ui->screen->mergelabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
//                    ui->screen->mergelabel->setPixmap(QPixmap::fromImage(result));
                    chuandiImage(merImage[0],merImage[1]);
                }

            },Qt::BlockingQueuedConnection);
            connect(cam2,&Camera::transImage,this,[=](QImage image2){
                merImage[1] =image2.mirrored(false,true);
//                QImage image(2304,1024,QImage::Format_RGB32);
//                for(int i=0;i<2304;i++){
//                    for(int j=0;j<1024;j++){
//                        image.setPixel(i,j,qRgb(0,merImage[1].pixel(i,j),0));
//                    }
//                }
//                QImage result =image.scaled(ui->screen->mergelabel->width(), ui->screen->mergelabel->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
//                ui->screen->mergelabel->setPixmap(QPixmap::fromImage(result));
            },Qt::BlockingQueuedConnection);

            connect(MP,&MergePlay::display,ui->screen->mergelabel,[=](QImage image){
                //QImage result =image.scaled(2304,1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                //ui->screen->mergelabel->setPixmap(QPixmap::fromImage(result));
                ui->screen->mergelabel->setPixmap(QPixmap::fromImage(image).scaled(ui->screen->mergelabel->size(),Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
                ui->screen->mergelabel->setScaledContents(true);
                ui->screen->mergelabel->resize(2304,1024);
            });
        }

        //
        imInfo.amplitude=ui->lineEdit_amplitude->text().toDouble();
        imInfo.offset=ui->lineEdit_offset->text().toDouble();
        imInfo.interval=ui->doubleSB_setInterval->text().toFloat();

        imInfo.wavelength_sel[404] =    (ui->pushButton_404OnOff->text()=="Stop")?true:false;
        imInfo.wavelength_sel[488] =    (ui->pushButton_488OnOff->text()=="Stop")?true:false;
        imInfo.wavelength_sel[561] =    (ui->pushButton_561OnOff->text()=="Stop")?true:false;
        imInfo.wavelength_sel[637] =    (ui->pushButton_637OnOff->text()=="Stop")?true:false;

        imInfo.m_laserInfo[404] =   ui->spinBox_404->text();
        imInfo.m_laserInfo[488] =   ui->spinBox_488->text();
        imInfo.m_laserInfo[561] =   ui->spinBox_561->text();
        imInfo.m_laserInfo[637] =   ui->spinBox_637->text();
        //

        getInitData(0);
        getInitData(1);

        m_NIctrl->stop();
        m_NIctrl->setInterval(imInfo.interval);
        m_NIctrl->NIinit(imInfo.amplitude,imInfo.offset,imInfo.wavelength_sel);
        emit startLive();
        m_NIctrl->NIstart();
        qDebug()<<"Camera start live!";
    }else if(ui->pushButton_Live->text() == "abort")
    {
        liveTimer->stop();
        ui->pushButton_Live->setText("live");
        Camera::stopflag=true;
        m_NIctrl->stop();
        qDebug()<<"Camera stop live!";
        if(ui->screen->currentIndex() == 0){
            cam1->disconnect(ui->screen->splitlabel1);
            cam2->disconnect(ui->screen->splitlabel2);
        }else{
//            cam1->disconnect(ui->screen->mergelabel);
//            cam2->disconnect(ui->screen->mergelabel);
            cam1->disconnect(this);
            cam2->disconnect(this);
            MP->disconnect(ui->screen->mergelabel);
        }

    }
}

void VISoR::paintEvent(QPaintEvent* event){
    if(ui->screen->splitW1->width() > ui->screen->splitW1->height()){
        float fScaleH1 = ui->screen->splitW1->height();
        float fScaleW1 = fScaleH1 * m_fScale;
        ui->screen->splitlabel1->resize(fScaleW1,fScaleH1);
    }else{
        float fScaleW1 = ui->screen->splitW1->width();
        float fScaleH1 = fScaleW1 * m_fScale;
        ui->screen->splitlabel1->resize(fScaleW1,fScaleH1);
    }

    if(ui->screen->splitW2->width() > ui->screen->splitW2->height()){
        float fScaleH2 = ui->screen->splitW2->height();
        float fScaleW2 = fScaleH2 * m_fScale;
        ui->screen->splitlabel2->resize(fScaleW2,fScaleH2);
    }else{
        float fScaleW2 = ui->screen->splitW2->width();
        float fScaleH2 = fScaleW2 * m_fScale;
        ui->screen->splitlabel2->resize(fScaleW2,fScaleH2);
    }

    if(ui->screen->mergeW->width() > ui->screen->mergeW->height()){
        float fScaleH3 = ui->screen->mergeW->height();
        float fScaleW3 = fScaleH3 * m_merge_fScale;
        ui->screen->mergelabel->resize(fScaleW3,fScaleH3);
    }else{
        float fScaleW3 = ui->screen->mergeW->width();
        float fScaleH3 = fScaleW3 * m_merge_fScale;
        ui->screen->mergelabel->resize(fScaleW3,fScaleH3);
    }
}

//void VISoR::wheelEvent(QWheelEvent* event){
//    QPoint sroll = event->angleDelta();
//    int step = 0;
//    if(!sroll.isNull()){
//        step = sroll.y();
//    }
//    int curwidth1 = ui->screen->splitlabel1->width();
//    int curheight1 = ui->screen->splitlabel1->height();
//    curwidth1 += step;
//    curheight1 += step;
//    ui->screen->splitlabel1->resize(curwidth1,curheight1);
//    QString imgsize = QString("图像缩放,尺寸为：%1 * %2")
//                    .arg(curwidth1).arg(curheight1);
//            qDebug() << imgsize;
//}

void VISoR::on_actionsplit_merge_triggered()
{
    if(ui->screen->currentIndex() == 0)
        ui->screen->setCurrentIndex(1);
    else
        ui->screen->setCurrentIndex(0);
}


//点击capture，将图片保存下来
void VISoR::on_pushButton_cap_clicked()
{
    const QPixmap* cap1 = ui->screen->splitlabel1->pixmap();
    const QPixmap* cap2 = ui->screen->splitlabel2->pixmap();
    QString dir1 = "D:/shot/shot561-" + QString::number(num)+ ".png";
    QString dir2 = "D:/shot/shot640-" + QString::number(num)+ ".png";
    cap1->save(dir1);
    cap2->save(dir2);
    qDebug()<<"capture image";
    num++;
}


