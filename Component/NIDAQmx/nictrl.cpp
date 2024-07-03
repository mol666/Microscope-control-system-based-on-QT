#include "nictrl.h"

#include "NIDAQmx.h"
#include <QDebug>
#include <QMap>
#include <atomic>



//定义采样元素1.2，所有计算的采样数量为其数目的1.2倍，用于调整硬件中的采样速率
//调整时间中最大采样数目不高于MAX_COUNT
static const float64 SAMPLE_FACTOR=1.2;//1
static const uInt64 MAX_COUNT=900000*SAMPLE_FACTOR;
//采样率m_rate为2000000.0*SAMPLE_FACTOR
NIctrl::NIctrl(QObject *parent):m_rate(2000000.0*SAMPLE_FACTOR)
{
    //控制信号任务句柄
    m_htaskCtr=NULL;
    //AO信号任务句柄,用于对控制扫描振镜的模拟信号输出
    m_htaskAO=NULL;
    //DO信号任务句柄,用于对激光器和相机出发的数字信号输出
    m_htaskDO=NULL;
//    //测试用的AO和DO任务句柄
//    test_htaskAO=NULL;test_htaskDO=NULL;
    //采样数：默认采样数为20006*SAMPLE_FACTOR
    m_sampleCount=20006*SAMPLE_FACTOR;

    m_bUsetrigger=false;
}

NIctrl::~NIctrl()
{
    stop();
}

bool NIctrl::failed(int32 err)
{
    if(DAQmxFailed(err)){
            char errBuff[2048]={'\0'};
            DAQmxGetExtendedErrorInfo(errBuff,2048);
            qDebug()<<"DAQmx Error:"<<errBuff;
    }
    return false;
}



void NIctrl::stop()
{
    qDebug()<<"NI stop all";
    closeTask(m_htaskCtr);
    closeTask(m_htaskAO);
    closeTask(m_htaskDO);
//    closeTask(test_htaskAO);
//    closeTask(test_htaskDO);
}

void NIctrl::closeTask(TaskHandle &t)
{
    if(NULL!=t){
        TaskHandle _t=t;t=NULL;
        DAQmxStopTask(_t);
        DAQmxClearTask(_t);
    }
}


//NI控制卡初始化，配置AO，DO输出端口，并初始化Ctr任务
bool NIctrl::NIinit(double amplitude, double offset,QMap<int,bool> laserTriggerPort)
{
    qDebug()<<"init NI \namplitude:"<<amplitude<<"offset:"<<offset;

    //创建任务句柄
    if(  failed(DAQmxCreateTask("",&m_htaskCtr))
       ||failed(DAQmxCreateTask("",&m_htaskAO))
       ||failed(DAQmxCreateTask("",&m_htaskDO))){
            qWarning()<<"failed to create task";return false;
    }

    /*
     * 配置AO口，其中AO1为扫描振镜的模拟控制信号
     * amplitude为扫描振镜振幅控制的电压，offset为振幅偏置的电压
     * 根据/Dev1/PFI12的时钟信号，以m_rate采样率，进行sampleCount采样数的模拟采样
    */
    uInt64 i=0;
    int32 written;
    uInt64 sampleCount=m_sampleCount;
    float64 riseCount=sampleCount/2;

    static float64 *dataAO=(float64*)malloc(MAX_COUNT*2);

    for(i=0;i<sampleCount;i++){
        float64 v=0;
        if(i<riseCount){
            v=(i+0.0)*2/riseCount-1;
        }else{
            v=-1*(i+0.0-riseCount)*2/(sampleCount-1-riseCount)+1;
        }
        v*=amplitude;
        v+=offset;
        dataAO[i]=v;
    }
    /*
     * 配置AO任务m_htaskAO，其中Dev1/ao1为扫描振镜的模拟控制信号口
     * 以/Dev1/PFI12端的信号为AO任务的时钟，以m_rate为采样率，以sampleCount为采样数
     *
    */
    if(failed(DAQmxCreateAOVoltageChan(m_htaskAO,"Dev1/ao1","",-10.0,10.0,DAQmx_Val_Volts,NULL))
     ||failed(DAQmxCfgSampClkTiming(m_htaskAO,"/Dev1/PFI12",m_rate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,sampleCount))
     ||failed(DAQmxWriteAnalogF64(m_htaskAO,sampleCount,0,-1,DAQmx_Val_GroupByChannel,dataAO,&written,NULL))
     ||failed(DAQmxStartTask(m_htaskAO)))
    {
        qWarning()<<"failed to create AO channel";
        return false;
    }

    uInt64 laserCount=riseCount-400*SAMPLE_FACTOR,cameraCount=riseCount+2000*SAMPLE_FACTOR;
    uInt64 laserStart=200*SAMPLE_FACTOR,cameraStart=riseCount+100*SAMPLE_FACTOR;
    uInt64 sampleCountDO=sampleCount;
    static uInt8 *dataDO=(uInt8*)malloc(MAX_COUNT*5*2);//uInt8 dataDO[MAX_COUNT*5*2];

    /*
     * 配置DO口，其中port0中的line4-7为Laser的通断信号，p0.4--[404],p0.5--[488],p0.6--[561],p0.7--[637]
     * QMap laserTriggerPort中的key为波长，value为是否选用此波长
     * port0中的line0为Camera的通断信号
    */
    int port[4];
    port[0]=laserTriggerPort[404];
    port[1]=laserTriggerPort[488];
    port[2]=laserTriggerPort[561];
    port[3]=laserTriggerPort[637];
    qDebug()<<"laser404:"<<port[0]<<"laser488:"<<port[1]<<"laser561:"<<port[2]<<"laser637:"<<port[3];
    for(i=0;i<sampleCount;i++){
        for(int j=0;j<4;j++){
            uInt8 v=((i>laserStart&&i<laserCount)?1:0);
            v = v * port[j];
            dataDO[5*i+j]=v;
        }
        dataDO[5*i+4]=((i>cameraStart&&i<cameraCount)?1:0);
    }

    if(failed(DAQmxCreateDOChan(m_htaskDO,"Dev1/port0/line4:7,Dev1/port0/line0","",DAQmx_Val_ChanForAllLines))
            ||failed(DAQmxCfgSampClkTiming(m_htaskDO,"/Dev1/PFI12",m_rate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,sampleCountDO))
            ||failed(DAQmxWriteDigitalLines(m_htaskDO,sampleCountDO,0,-1,DAQmx_Val_GroupByChannel, dataDO,&written,NULL))
            ||failed(DAQmxStartTask(m_htaskDO))){
        qWarning()<<"failed to create DO channel";
        return false;
    }
    /*
     * 配置Ctr任务m_htaskCtr，其中Dev1/ctr0为提供给AO和DO信号的时钟信号端口，物理端口为/Dev1/PFI12
     * 时钟的时间与m_rate相关
    */
    static const float64 highTime=0.0000003/SAMPLE_FACTOR;
    static const float64 lowTime=0.0000002/SAMPLE_FACTOR;

    if(       failed(DAQmxCreateCOPulseChanTime(m_htaskCtr,"Dev1/ctr0","",DAQmx_Val_Seconds,DAQmx_Val_Low,0,highTime,lowTime))
//            ||failed(DAQmxCfgImplicitTiming(m_htaskCtr,DAQmx_Val_ContSamps,100000))
              //将Dev1/ctr0的内部输出口与Dev1/PFI12连起来（）
            ||failed(DAQmxConnectTerms("/Dev1/Ctr0InternalOutput","/Dev1/PFI12",DAQmx_Val_DoNotInvertPolarity))
              //使用外部触发模式开启去掉下面部分的注释，触发方式是给NI采集卡的PFI0口上升沿信号触发
//            ||(m_bUsetrigger&&failed(DAQmxCfgDigEdgeStartTrig(m_htaskCtr,"/Dev1/PFI0",DAQmx_Val_Rising)))
//            ||failed(DAQmxStartTask(m_htaskCtr))
              ){
        qWarning()<<"failed to create counter";
        return false;
    }

    return true;
}

//不限次数的打开Ctr时钟
void NIctrl::NIstart()
{
    DAQmxStopTask(m_htaskCtr);//开始前给Ctr任务停止，防止多按
    qDebug()<<"NIstart Ctrtask ContSamps";
    failed(DAQmxCfgImplicitTiming(m_htaskCtr,DAQmx_Val_ContSamps,100000));
    failed(DAQmxStartTask(m_htaskCtr));
}

//以FiniteSampsCount为次数，打开Ctr时钟，即打开若干数量拍照数量的时间的时钟信号
void NIctrl::NIstart(uInt64 FiniteSampsCount)
{
    DAQmxStopTask(m_htaskCtr);//开始前给Ctr任务停止，防止多按
    FiniteSampsCount++;//全局曝光模式，两个上升沿之间为曝光时间（间隔时间），拍照总数+1
    failed(DAQmxCfgImplicitTiming(m_htaskCtr,DAQmx_Val_FiniteSamps,FiniteSampsCount*m_sampleCount));
    failed(DAQmxStartTask(m_htaskCtr));
    qDebug()<<"NIstart Ctrtask FiniteSamps"<<(FiniteSampsCount-1);
}

//暂停Ctr时钟的输出
void NIctrl::pauseNICtr()
{
    failed(DAQmxStopTask(m_htaskCtr));
    qDebug()<<"NICtr pause ";
}

//继续Ctr时钟的输出
void NIctrl::continueNICtr()
{
    failed(DAQmxStartTask(m_htaskCtr));
    qDebug()<<"NICtr cintinue ";
}

//设置拍摄单张照片的周期内的采样数
void NIctrl::setSampleCount(uInt64 sampleCount)
{
    static const uInt64 MIN_COUNT=3000*SAMPLE_FACTOR;
    //qDebug()<<sampleCount<<MAX_COUNT<<MIN_COUNT;
    if(sampleCount>MAX_COUNT){sampleCount=MAX_COUNT;}
    if(sampleCount<MIN_COUNT){sampleCount=MIN_COUNT;}

    m_sampleCount=sampleCount;
    m_bChanged=true;
}

//设置两张照片之间的间隔时间，即单张照片的时间
void NIctrl::setInterval(float time)
{
    uInt64 t=time*20000.0*SAMPLE_FACTOR/10.0;
    setSampleCount(t);
}


void NIctrl::useTrigger(bool bUse)
{
    if(bUse==m_bUsetrigger){return;}
    qDebug()<<(bUse?"":"do not")<<"use ni trigger";
    m_bUsetrigger=bUse;
}




