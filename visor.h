#ifndef VISOR_H
#define VISOR_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QMap>
#include <QThread>
#include "Component/Camera/camera.h"
#include "Component/Stage/aerotech.h"
#include "Component/Stage/stagedialog_from.h"
#include "Component/Stage/stagedialog_goto.h"
#include "Component/NIDAQmx/nictrl.h"
#include "Component/Stage/direction.h"
#include "Component/Laser/obisctrl.h"
#include <QFileDialog>
#include "splittomerge.h"
#include "Component/Camera/mergeplay.h"
//#include <QWheelEvent>
#include <QMutex>
QT_BEGIN_NAMESPACE
namespace Ui { class VISoR; }
QT_END_NAMESPACE


class VISoR : public QMainWindow
{
    Q_OBJECT
public:
    VISoR(QWidget *parent = nullptr);
    ~VISoR();

    Camera* cam1 = nullptr;
    Camera* cam2 = nullptr;

    //Camera* cam[4] = {nullptr,nullptr,nullptr,nullptr};
    //全局变量，可以让系统停下来
    static bool bStopFlag;
    static int num;

    void testImagewriterInit(QString path,int i,Camera* &cam);

    flsmio::ImageWriter* writer[20] = {nullptr};
    QThread* thread[20] = {nullptr};

    //uchar* AVpData8 = nullptr;
    //uchar* BVpData8 = nullptr;
    QTimer *liveTimer = nullptr;
    QImage merImage[2];

    MergePlay *MP = nullptr;
    void MPinit();
    //ushort* ApData16 = nullptr;
    //ushort* BpData16 = nullptr;

//    void savedata1(QString path);
//    void savedata2(QString path);


protected:
    // 重写事件处理器函数
    //窗口关闭
    void closeEvent(QCloseEvent* ev) Q_DECL_OVERRIDE;
    //拖入文件
    void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
    //拖拽文件并释放
    void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;

    void paintEvent(QPaintEvent* event) override;
    //void wheelEvent(QWheelEvent* event) override;

private:
    Ui::VISoR *ui;
    void Camerainit();
    void laserInit();
    void aerotechInit();
    void stageDialogFromInit();
    void stageDialogGotoInit();

    //按钮实现文件选择框弹出
    void getOutputPath(int pathflags);
    //messageBox显示信息数据
    void showMes();
    void getInitData(int camflag);


    QMap<QString,OBISCtrl*>m_laserSerial;       //串口名和对应的串口任务
    QMap<QString,QThread*>m_serialThread;       //串口名和对应的子线程



    StageDialog_From *fromDialog = nullptr;
    StageDialog_Goto *gotoDialog = nullptr;
    Direction *directionBtn = nullptr;
    Aerotech* aerotech = nullptr;
    NIctrl * m_NIctrl = nullptr;



    static void debugInfoOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static VISoR *m_ptr;
    static QFile file;


    struct imageInfo{
        //相机参数
        cameraInfo CamInfo[4];
        //其他部分的初始化参数
        float interval;             //拍照时间间隔，单位ms
        double amplitude,offset;    //扫描振镜部分的电压变化幅度和偏置，单位V
        unsigned long long int count;
        QMap<int,bool> wavelength_sel;//NI控制卡与各波长的激光器的信号触发的标志
        QMap<int,QString>m_laserInfo;           //输入的激光器波长和功率
        int totalColums = -1;    //这次要拍的col数
        int nowCol = 0;     //记录col数
        //三维平台
        double startPos_X,endPos_X,velocity_X;  //单条colunm测试用

        QList<struct motionPara>paraFromBorder;

    }imInfo;
    QString m_Message;

    float m_fScale;	 //图片宽高比
    float m_merge_fScale = 2304/1024;
signals:
    //gotoDialog中显示刷新的位置数据
    void gotoDialogRenew(double,double,double);

    void startCap1(int);
    void startCap2(int);

    void runAerotech(int);   //平台到起点准备、从起点往终点走、运动到指定位置用于调试
    void initAerotech();
    void abortAerotech();

    void debugInfoOutputed(QString);

    void initOBIS(QString);
    void onoffOBIS(int,bool);
    void setPowerOBIS(int,QString);
    void stopOBIS();

    void setCamVal1(cameraInfo);
    void setCamVal2(cameraInfo);

    void startLive();

    void chuandiImage(QImage image1,QImage image2);

private slots:
    void on_pushButton_NIPause_clicked();
    void on_pushButton_NIStop_clicked();
    void on_pushButton_NIstartFiniteCount_clicked();
    void on_pushButton_setInterval_clicked();
    void on_pushButton_NIInit_clicked();

    void on_pushButton_start_clicked();
    void on_pushButton_stop_clicked();

    void on_pushButton_404OnOff_clicked();
    void on_pushButton_488OnOff_clicked();
    void on_pushButton_561OnOff_clicked();
    void on_pushButton_637OnOff_clicked();
    void on_spinBox_404_valueChanged(int arg1);
    void on_spinBox_488_valueChanged(int arg1);
    void on_spinBox_561_valueChanged(int arg1);
    void on_spinBox_637_valueChanged(int arg1);

    void on_fileBtn_C1_clicked();
    void on_fileBtn_C2_clicked();

    void on_pushButton_Live_clicked();

    void goOneColumn(bool sig);
    void doneOneColumn(bool sig);

    //void showImage1(QImage image);
    //void showImage2(QImage image);
    void on_actionsplit_merge_triggered();

    void on_pushButton_cap_clicked();
    //void TostartToLive(void* data);
    void TostartToLive1(QImage image);
    void TostartToLive2(QImage image);

};



#endif // VISOR_H
