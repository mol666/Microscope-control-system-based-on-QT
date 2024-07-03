#include "obisctrl.h"
#include <QDebug>
#include <QString>
#include <QThread>
OBISCtrl::OBISCtrl(const QString &portName,QMap<int,QString>m_laserInfo)
{
    m_command[SET_ON] = "SOURce:AM:STATe ON";
    m_command[SET_OFF] = "SOURce:AM:STATe OFF";
    m_command[GET_ONOFF] = "SOURce:AM:STATe?";
    m_command[GET_WAVELENGTH] = "SYSTem:INFormation:WAVelength?";
    m_command[SET_POWER] = "SOURce:POWer:LEVel:IMMediate:AMPLitude ";
    m_command[GET_MAXPOWER] = "SOURce:POWer:LIMit:HIGH?";
    m_command[GET_POWER] = "SOURce:POWer:LEVel:IMMediate:AMPLitude?";
    m_command[SET_MODE] = "SOURce:AM:EXTernal DIGSO";
    m_command[SET_MODE_LS] = "SOURce:AM:EXTernal DIGital";
//    m_command[SET_MODE] = "SOURce:AM:INTernal CWP";
//    m_command[SET_MODE_LS] = "SOURce:AM:INTernal CWP";


    this->portName = portName;
    this->m_laserInfo = m_laserInfo;


}

OBISCtrl::~OBISCtrl()
{
}


//初始化过程
void OBISCtrl::OBISinit(QString name)
{
    if(name!=portName) return;
    qDebug()<<portName<<":"<<"OBIS device";
    //qDebug()<<portName<<QThread::currentThread();
    m_serial = new QSerialPort(this);
    m_serial->setBaudRate(QSerialPort::Baud115200);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setPortName(portName);
    if(m_serial->open(QIODevice::ReadWrite))
        qDebug()<<m_serial->portName()<<"serial open";
    else qDebug()<<m_serial->portName()<<"serial cant open once";

    //1.state off
    //2.get wavelength
    //3.set power
    //4.set mode
    //5.set on
    commandToSend<<SET_OFF<<GET_WAVELENGTH<<SET_POWER<<SET_MODE;
    sendCommand();

    //如果初始化成功，发送信号，使能对应波长的start按键
    if(initSuccessFlag) emit OBISInitSuccessSignal(waveLength);
}

void OBISCtrl::OBISsetPower(int w,QString val)
{
    if(w!=waveLength) return;
    power = val;
    commandToSend<<SET_POWER;
    sendCommand();
    qDebug()<<w<<"power:"<<val;
}

void OBISCtrl::OBISstop()
{
    commandToSend<<SET_OFF;
    sendCommand();
    m_serial->close();
}



//开或者关
void OBISCtrl::OBISonoff(int w, bool state)
{
    if(w!=waveLength) return;
    commandToSend<<((state==true)?SET_ON:SET_OFF);
    sendCommand();
    qDebug()<<w<<((state==true)?"ON":"OFF");
}


void OBISCtrl::sendCommand()
{
    if(m_serial->isOpen())
    {
        while(!commandToSend.empty())
        {
            while(true) //没有发出去的消息不断尝试
            {
                QString s = m_command[commandToSend.first()];
                //对于488和561波长的激光器，由于是LS版，没有DIGSO模式
                if(commandToSend.first()==SET_MODE&&(waveLength==488||waveLength==561))
                    s = m_command[SET_MODE_LS];
                //设定功率的值
                if(commandToSend.first()==SET_POWER) s += power;
                s += m_lineEnd;
                //qDebug()<<s;
                m_serial->write(s.toLatin1());
                if(!m_serial->waitForBytesWritten(waitTimeout)||!m_serial->waitForReadyRead(waitTimeout))
                {
                    //qWarning()<<"failed to send data retrying...";
                    continue;
                }else
                {
                    handleReceiveData(QString::fromLatin1(m_serial->readAll()).replace(m_lineEnd,""),commandToSend.first());
                    commandToSend.pop_front();
                    break;
                }
            }

        }
    }else if(!m_serial->open(QIODevice::ReadWrite))
    {
        qDebug()<<m_serial->portName()<<"serial cant open twice";
        initSuccessFlag = false;
    }
}






void OBISCtrl::handleReceiveData(QString r, int type)
{
    //qDebug()<<r;
    switch(type)
    {
        case GET_WAVELENGTH:
            waveLength = r.leftRef(3).toInt();
            qDebug()<<m_serial->portName()<<"waveLength:"<<waveLength;
            power = m_laserInfo[waveLength];
            break;
        default:
            if(r!="OK")
            {
                qWarning()<<"OBIS"<<waveLength<<"ERROR!"<<m_serial->portName()<<QString::number(type);
                initSuccessFlag = false;
            }
            break;
    }
}


