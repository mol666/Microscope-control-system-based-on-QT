#include "aerotech.h"
#include "direction.h"
#include "visor.h"
#include <QDebug>
#include <QThread>
#include <QMessageBox>

bool Aerotech::bStageAbortFlag = true;

Aerotech::Aerotech(QObject *parent)
    : QObject{parent}
{

}

Aerotech::~Aerotech()
{
    if(A3200MotionDisable(handle, TASKID_Library, axisMask)
       &&A3200Disconnect(handle))
        qDebug()<<"A3200 disconnected";
    else
        qDebug()<<"failed to disconnect A3200";
}

bool Aerotech::aerotechQuery(AXISINDEX index, STATUSITEM item, DWORD status)
{
    double res;
    A3200StatusGetItem(handle,index,item,status,&res);
    //用直接执行机器代码的方式也可以，例如：
    //A3200CommandExecute(aerotech->handle, TASKID_Library, "AxisStatus(X, DATAITEM_AxisStatus) & 0x00002000", &result)
    return (bool)res;
}

int Aerotech::aerotechWait(struct axisInfo& axis, int moveType)
{
    bool inPosition = false;
    do
    {
       inPosition = aerotechQuery(axis.axis_index,STATUSITEM_AxisStatus,AXISSTATUS_WaitDone);
       //inPosition = aerotechQuery(axis.axis_index,STATUSITEM_DriveStatus,DRIVESTATUS_InPosition);
    }while(!(inPosition||Aerotech::bStageAbortFlag||VISoR::bStopFlag));
    //正常到达
    if(inPosition)
    {
        switch(moveType)
        {
        case moveToStartPos:
            axis.nowPosition = axis.startPosition;
            //qDebug().noquote()<<axis.axisName<<"reached start position!";
            break;
        case moveToEndPos:
            axis.nowPosition = axis.endPosition;
            //qDebug().noquote()<<axis.axisName<<"reached end position!";
            break;
        case moveToSetPos:
            axis.nowPosition = axis.setPosition;
            //qDebug().noquote()<<axis.axisName<<"reached set position!";
            break;
        default:break;
        }
        return 1;
    }else   //意外终止
    {
        A3200MotionAbort(handle,axis.axis_mask);
        axis.nowPosition = axis.positionFeedback;
        qWarning()<<axis.axisName<<"abort!";
        return 0;
    }
}

bool Aerotech::aerotechPosLock(int moveType,double paraX,double paraY,double paraZ)
{
    Q_UNUSED(paraZ);
    Q_UNUSED(moveType);
    Q_UNUSED(paraX);
    Q_UNUSED(paraY);
//    static const double limitZ_up = -2;                        //高于这个值会有问题
//    static const double limitX_outer = 50,limitX_inner = 40;
//    static const double limitY_outer = 50,limitY_inner = 40;
//    //避免Z轴上升时，载物台边缘与物镜直接撞击
//    if(qAbs(paraX)>limitX_inner&&qAbs(paraX)<limitX_outer&&qAbs(paraY)>limitY_inner&&qAbs(paraY)<limitY_outer&&paraZ<limitZ_up)
//        return false;
//    else
//    //避免Z轴上升后，XY方向的撞击
//    switch(moveType)
//    {
//    //moveToEnd要参考startPosition
//    case moveToEndPos:
//        if(axisInfo_Z.startPosition<limitZ_up
//        &&((qAbs(axisInfo_X.startPosition)<qAbs(limitX_inner)&&qAbs(axisInfo_Y.startPosition)<qAbs(limitY_inner)&&qAbs(paraX)>=qAbs(limitX_inner))
//         ||(qAbs(axisInfo_X.startPosition)>qAbs(limitX_outer)&&qAbs(axisInfo_Y.startPosition)<qAbs(limitY_outer)&&qAbs(paraX)<=qAbs(limitX_outer))
//         ||(qAbs(axisInfo_Y.startPosition)<qAbs(limitY_inner)&&qAbs(axisInfo_X.startPosition)<qAbs(limitX_inner)&&qAbs(paraY)>=qAbs(limitY_inner))
//         ||(qAbs(axisInfo_Y.startPosition)>qAbs(limitY_outer)&&qAbs(axisInfo_X.startPosition)<qAbs(limitX_outer)&&qAbs(paraY)<=qAbs(limitY_outer))))
//        {
//            return false;
//        }
//        break;
//    //以下2种直接参考nowPosition
//    case moveToStartPos:
//    case moveToSetPos:
//        if(axisInfo_Z.nowPosition<limitZ_up
//        &&((qAbs(axisInfo_X.nowPosition)<qAbs(limitX_inner)&&qAbs(axisInfo_Y.nowPosition)<qAbs(limitY_inner)&&qAbs(paraX)>=qAbs(limitX_inner))
//         ||(qAbs(axisInfo_X.nowPosition)>qAbs(limitX_outer)&&qAbs(axisInfo_Y.nowPosition)<qAbs(limitY_outer)&&qAbs(paraX)<=qAbs(limitX_outer))
//         ||(qAbs(axisInfo_Y.nowPosition)<qAbs(limitX_inner)&&qAbs(axisInfo_X.nowPosition)<qAbs(limitX_inner)&&qAbs(paraY)>=qAbs(limitX_inner))
//         ||(qAbs(axisInfo_Y.nowPosition)>qAbs(limitX_outer)&&qAbs(axisInfo_X.nowPosition)<qAbs(limitX_outer)&&qAbs(paraY)<=qAbs(limitX_outer))))
//        {
//            return false;
//        }
//        break;
//    default:break;
//    }
    return true;
}


//囊括3种运动，本质都是定位运动，但细节不同，可以手动中断
void Aerotech::aerotechRun(int moveType)
{
    switch(moveType)
    {
    case moveToStartPos:
        A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_X.axis_index,axisInfo_X.startPosition,axisInfo_X.setVelocity);
        A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_Y.axis_index,axisInfo_Y.startPosition,axisInfo_Y.setVelocity);
        //qDebug()<<"Axis_X and Axis_Y are moving to start position...";

        //定位运动期间，不能接受点动指令
        emit setStepButton(false);
        Aerotech::bStageAbortFlag = false;

        //XY运动到位，Z开始运动
        if(aerotechWait(axisInfo_X,moveType)&aerotechWait(axisInfo_Y,moveType))
        {
            A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_Z.axis_index,axisInfo_Z.startPosition,axisInfo_Z.setVelocity);
            //qDebug()<<"Axis_Z is moving to start position...";
            if(aerotechWait(axisInfo_Z,moveType))
            {
                emit aerotechReachStartSignal(true);
                qDebug()<<"Aerotech reached start position!";
                break;
            }
        }
        //XYZ并没有运动到位
        emit aerotechReachStartSignal(false);
        qWarning()<<"Aerotech stopped reaching start position!";
        break;
    case moveToEndPos:
        A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_X.axis_index,axisInfo_X.endPosition,axisInfo_X.setVelocity);
        A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_Y.axis_index,axisInfo_Y.endPosition,axisInfo_Y.setVelocity);
        A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_Z.axis_index,axisInfo_Z.endPosition,axisInfo_Z.setVelocity);
        //qDebug()<<"Axis_Z is moving to end position...";
        //qDebug()<<"Axis_X and Axis_Y are moving to end position...";
        //定位运动期间，不能接受点动指令
        emit setStepButton(false);
        Aerotech::bStageAbortFlag = false;
        if(aerotechWait(axisInfo_X,moveType)&aerotechWait(axisInfo_Y,moveType)&aerotechWait(axisInfo_Z,moveType))
        {
            emit aerotechOneColumnDoneSignal(true);
            qDebug()<<"Aerotech reached end position!";
            break;

        }
        emit aerotechOneColumnDoneSignal(false);
        qWarning()<<"Aerotech stopped reaching end position!";
        break;
    case moveToSetPos:
        A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_X.axis_index,axisInfo_X.setPosition,20);
        A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_Y.axis_index,axisInfo_Y.setPosition,20);
        //qDebug()<<"Axis_X and Axis_Y are moving to set position...";

        //定位运动期间，不能接受点动指令
        emit setStepButton(false);
        Aerotech::bStageAbortFlag = false;

        if(aerotechWait(axisInfo_X,moveType)&aerotechWait(axisInfo_Y,moveType))
        {
            A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_Z.axis_index,axisInfo_Z.setPosition,20);
            //qDebug()<<"Axis_Z is moving to set position...";
            if(aerotechWait(axisInfo_Z,moveType))
            {
                qDebug()<<"Aerotech reached set position!";
                break;
            }
        }
        qWarning()<<"Aerotech stopped reaching set position!";
        break;
    default:break;
    }
    emit setStepButton(true);
    Aerotech::bStageAbortFlag = true;
}


//点开Locate窗口后的处理逻辑，没有abort运动的功能
void Aerotech::aerotechGotoWork(double desti,int axis)
{
    //限位锁依赖输入UI控件
    //不提供外部速度设置接口，goto速度恒定为20mm/s
    switch(axis)
    {
    case X:
        if(aerotechPosLock(moveToSetPos,desti,axisInfo_Y.nowPosition,axisInfo_Z.nowPosition))
        {
            A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_X.axis_index,desti,20);
            axisInfo_X.nowPosition = desti;
        }else
        {qDebug()<<"X axis limit!";emit gotoDialogRenew(axisInfo_X.nowPosition,axisInfo_Y.nowPosition,axisInfo_Z.nowPosition);}
        break;
    case Y:
        if(aerotechPosLock(moveToSetPos,axisInfo_X.nowPosition,desti,axisInfo_Z.nowPosition))
        {
            A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_Y.axis_index,desti,20);
            axisInfo_Y.nowPosition = desti;
        }else
        {qDebug()<<"Y axis limit!";emit gotoDialogRenew(axisInfo_X.nowPosition,axisInfo_Y.nowPosition,axisInfo_Z.nowPosition);}
        break;
    case Z:
        A3200MotionMoveAbs(handle,TASKID_Library,axisInfo_Z.axis_index,desti,20);
        axisInfo_Z.nowPosition = desti;
        break;
    default:break;
    }


}

//点击前后左右上下按键的处理逻辑，支持连按，没有abort运动的功能
void Aerotech::aerotechStepWork(double distance, double velocity, int axis,int type)
{
    //限位锁依赖内部软件limit
    if(type == nega) distance = -distance;

    switch(axis)
    {
    case X:
        if(axisInfo_X.nowPosition+distance>axisInfo_X.positiveLimit
        ||axisInfo_X.nowPosition+distance<axisInfo_X.negativeLimit
        ||!aerotechPosLock(moveToSetPos,axisInfo_X.nowPosition+distance,axisInfo_Y.nowPosition,axisInfo_Z.nowPosition))
        {
            qWarning()<<"X axis limit!";
        }else
        {
            axisInfo_X.nowPosition += distance;
            qDebug()<<"X axis move"<<((type==nega)?"left":"right")<<distance<<"mm";
            A3200MotionMoveInc(handle,TASKID_Library,axisInfo_X.axis_index,distance,velocity);
        }
        break;
    case Y:
        if(axisInfo_Y.nowPosition+distance>axisInfo_Y.positiveLimit
        ||axisInfo_Y.nowPosition+distance<axisInfo_Y.negativeLimit
        ||!aerotechPosLock(moveToSetPos,axisInfo_X.nowPosition,axisInfo_Y.nowPosition+distance,axisInfo_Z.nowPosition))
        {
            qWarning()<<"Y axis limit!";
        }else
        {
            axisInfo_Y.nowPosition += distance;
            qDebug()<<"Y axis move"<<((type==nega)?"back":"forward")<<distance<<"mm";
            A3200MotionMoveInc(handle,TASKID_Library,axisInfo_Y.axis_index,distance,velocity);
        }
        break;
    case Z:
        if(axisInfo_Z.nowPosition+distance>axisInfo_Z.positiveLimit
        ||axisInfo_Z.nowPosition+distance<axisInfo_Z.negativeLimit)
        {
            qWarning()<<"Z axis limit!";
        }else
        {
            axisInfo_Z.nowPosition += distance;
            qDebug()<<"Z axis move"<<((type==nega)?"up":"down")<<distance<<"mm";
            A3200MotionMoveInc(handle,TASKID_Library,axisInfo_Z.axis_index,distance,velocity);
        }
        break;
    default:break;
    }
}

void Aerotech::aerotechInit()
{
    if(A3200Connect(&handle)
        &&A3200MotionEnable(handle, TASKID_Library, axisMask)
        &&A3200MotionMoveAbs(handle, TASKID_Library, axisInfo_Z.axis_index, 50, 20)
        &&A3200MotionWaitForMotionDone(handle,axisInfo_Z.axis_mask,WAITOPTION_MoveDone,-1,NULL)
        &&A3200MotionHome(handle, TASKID_Library, (AXISMASK)(axisInfo_X.axis_mask|axisInfo_Y.axis_mask))
        //会阻塞进程
        &&A3200MotionHome(handle, TASKID_Library, axisInfo_Z.axis_mask))
    {
        if(aerotechQuery(axisInfo_X.axis_index,STATUSITEM_AxisStatus,AXISSTATUS_NotVirtual))
        {
            qDebug()<<"A3200 real mode ready";
            emit aerotechInitSuccessSignal(true);
        }else
        {
            qDebug()<<"A3200 virtual mode ready";
            emit aerotechInitSuccessSignal(false);
        }
    }else qDebug()<<"failed to connect A3200";
}

void Aerotech::aerotechHomeAgain()
{
    //停止
    A3200MotionAbort(handle,axisMask);
    axisInfo_X.nowPosition = axisInfo_X.positionFeedback;
    axisInfo_Y.nowPosition = axisInfo_Y.positionFeedback;
    axisInfo_Z.nowPosition = axisInfo_Z.positionFeedback;
    QThread::msleep(10);

    //home
    A3200MotionMoveAbs(handle, TASKID_Library, axisInfo_Z.axis_index, 50, 20);
    A3200MotionWaitForMotionDone(handle,axisInfo_Z.axis_mask,WAITOPTION_MoveDone,-1,NULL);
    A3200MotionHome(handle, TASKID_Library, (AXISMASK)(axisInfo_X.axis_mask|axisInfo_Y.axis_mask));
    A3200MotionHome(handle, TASKID_Library, axisInfo_Z.axis_mask);

    axisInfo_X.nowPosition = 0;
    axisInfo_Y.nowPosition = 0;
    axisInfo_Z.nowPosition = 0;
    qDebug()<<"Aerotech homed!";

    emit aerotechHomeSuccessSignal();
}

void Aerotech::aerotechAbort()
{
    if(aerotechQuery(axisInfo_X.axis_index,STATUSITEM_AxisStatus,AXISSTATUS_Jogging)
     ||aerotechQuery(axisInfo_Y.axis_index,STATUSITEM_AxisStatus,AXISSTATUS_Jogging)
     ||aerotechQuery(axisInfo_Z.axis_index,STATUSITEM_AxisStatus,AXISSTATUS_Jogging))
    {
        //停止
        A3200MotionAbort(handle,axisMask);
        QThread::msleep(10);
        axisInfo_X.nowPosition = axisInfo_X.positionFeedback;
        axisInfo_Y.nowPosition = axisInfo_Y.positionFeedback;
        axisInfo_Z.nowPosition = axisInfo_Z.positionFeedback;
    }
}





