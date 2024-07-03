#ifndef AEROTECH_H
#define AEROTECH_H
#include "A3200.h"
#include <QObject>


/*  query表
 *  STATUSITEM_DriveStatus
 *  1.  DRIVESTATUS_Enabled
 *  2.  DRIVESTATUS_InPosition
 *  3.  DRIVESTATUS_MoveActive
 *
 *  STATUSITEM_AxisStatus
 *  1.  AXISSTATUS_Homed
 *  2.  AXISSTATUS_WaitDone 在WAIT MODE MOVEDONE模式下和MoveDone一样
 *  在WAIT MODE INPOS模式下，受MoveDone和InPosition同时影响
 *  3.  AXISSTATUS_Jogging
 *  4.  AXISSTATUS_NotVirtual
 *  5.  AXISSTATUS_MoveDone
 *  6.  AXISSTATUS_Homing
 *
 *  STATUSITEM_AxisFault
 *  1.AXISFAULT_CwEOTLimit
 *  2.AXISFAULT_CcwEOTLimit
 *  3.AXISFAULT_CwSoftLimit
 *  4.AXISFAULT_CcwSoftLimit
 */

/*
 *  X轴，向左是nega，向右是posi  100
 *  Y轴，向后是nega，向前是posi  100
 *  Z轴，向上是nega，向下是posi  75
 */
struct axisInfo
{
    QString axisName;
    DOUBLE positiveLimit;
    DOUBLE negativeLimit;

    AXISMASK axis_mask;
    AXISINDEX axis_index;

    DOUBLE nowPosition = 0;     //代码内部认为平台所在的位置，每次运动完后这个值会更新，和feedback值相近，但不完全相同
    DOUBLE setVelocity = 5;   //只有fromtoDialog会修改这个值，goto和step点动不会

    DOUBLE startPosition = 0;
    DOUBLE endPosition = 0;
    DOUBLE setPosition = 0;

    DOUBLE positionFeedback;
    DOUBLE velocityFeedback;

    axisInfo(QString name,DOUBLE posi,DOUBLE nega,AXISMASK mask,AXISINDEX index):axisName(name),positiveLimit(posi),negativeLimit(nega),axis_mask(mask),axis_index(index) {}
};

enum{
    X = 0,Y = 1,Z = 2,
};

enum{
  moveToStartPos,moveToEndPos,moveToSetPos
};


struct motionPara
{
    double motionStart[3];
    double motionEnd[3];
    double velocity[3];

    //快扫的数据，只有XY轴
    motionPara(double sX,double sY,double eX,double eY)
    {motionStart[X]=sX,motionEnd[X]=eX,motionStart[Y]=sY,motionEnd[Y]=eY;}
    //慢扫的数据，有XYZ三轴
    motionPara(double sX,double sY,double sZ,double eX,double eY,double eZ)
    {motionStart[X]=sX,motionEnd[X]=eX,motionStart[Y]=sY,motionEnd[Y]=eY,motionStart[Z]=sZ,motionEnd[Z]=eZ;}
    //fromtoDialog传参用
    motionPara(double sX,double eX,double vX,double sY,double eY,double vY,double sZ,double eZ,double vZ)
    {motionStart[X]=sX,motionEnd[X]=eX,motionStart[Y]=sY,motionEnd[Y]=eY,motionStart[Z]=sZ,motionEnd[Z]=eZ,velocity[X]=vX,velocity[Y]=vY,velocity[Z]=vZ;}
    motionPara(){}
};


class Aerotech : public QObject
{
    Q_OBJECT
public:
    explicit Aerotech(QObject *parent = nullptr);
    ~Aerotech();

    bool aerotechQuery(AXISINDEX index,STATUSITEM item, DWORD status);
    int aerotechWait(struct axisInfo& axis, int movetype);
    bool aerotechPosLock(int moveType,double posX,double posY,double posZ);

    //全局变量，可以让平台停下来，只对平台有效
    static bool bStageAbortFlag;

    A3200Handle handle = NULL;
    //AXISMASK_00 和 AXISMASK_01 和 AXISMASK_02 分别对应X轴和Y轴和Z轴
    AXISMASK axisMask = (AXISMASK)(AXISMASK_00 | AXISMASK_01 | AXISMASK_02);

    //限位参数
    axisInfo axisInfo_X = axisInfo("Axis_X",100,-100,(AXISMASK)(AXISMASK_00),(AXISINDEX)(AXISINDEX_00));
    axisInfo axisInfo_Y = axisInfo("Axis_Y",100,-100,(AXISMASK)(AXISMASK_01),(AXISINDEX)(AXISINDEX_01));
    axisInfo axisInfo_Z = axisInfo("Axis_Z",75,-21,(AXISMASK)(AXISMASK_02),(AXISINDEX)(AXISINDEX_02));



signals:
    void setStepButton(bool);   //发信号给direction类，暂时关闭/使能点动按键

    void aerotechInitSuccessSignal(bool);   //初始化成功，enable所有stage相关按键

    void aerotechHomeSuccessSignal();

    void aerotechReachStartSignal(bool);    //平台到达设定起点

    void aerotechOneColumnDoneSignal(bool);       //一条col拍完

    void gotoDialogRenew(double,double,double); //在gotoDialog限位情况下，让UI和实际数值一致


public slots:
    void aerotechRun(int moveType);
    //如果start时，平台没有到起点，也会先在主线程执行这个

    void aerotechGotoWork(double desti,int axis);
    void aerotechStepWork(double distance,double velocity,int axis,int type);

    //将平台初始化HOME这个阻塞主线程极长时间的操作放在子线程中
    void aerotechInit();

    //初始化之后，响应再次HOME需求
    void aerotechHomeAgain();

    //停止StepWork和GotoWork发给A3200的运动指令
    void aerotechAbort();



};

#endif // AEROTECH_H
