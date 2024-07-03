#ifndef OBISCTRL_H
#define OBISCTRL_H

#include <QObject>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QMap>
#include <QMutex>
/*
    SET_OFF             SOURce:AM:STATe OFF
    SET_ON              SOURce:AM:STATe ON
    GET_ONOFF           SOURce:AM:STATe?
    GET_WAVELENGTH      SYSTem:INFormation:WAVelength?
    SET_POWER           SOURce:POWer:LEVel:IMMediate:AMPLitude <value>
    GET_MAXPOWER        SOURce:POWer:LIMit:HIGH?
    GET_POWER           SOURce:POWer:LEVel?
                        SOURce:POWer:LEVel:IMMediate:AMPLitude?
    SET_MODE            SOURce:AM:EXTernal DIGSO        404/637
    SET_MODE_LS         SOURce:AM:EXTernal DIGital      488/561
    GET_SOURCE          SOURce:AM:SOURce?

*/
enum OBISCTRL_MSGTYPE
{
    SET_OFF,SET_ON,GET_ONOFF,GET_WAVELENGTH,SET_POWER,GET_MAXPOWER,GET_POWER,SET_MODE,SET_MODE_LS
};

class OBISCtrl : public QObject
{
    Q_OBJECT
public:
    explicit OBISCtrl(const QString &portName,QMap<int,QString>m_laserInfo);
    ~OBISCtrl();

public slots:
    void OBISonoff(int w,bool state);

    void OBISinit(QString name);

    void OBISsetPower(int w, QString val);

    void OBISstop();

signals:
    void OBISInitSuccessSignal(int);

private:
    QMap<int,QString>m_command;         //指令表（通用）
    QMap<int,QString>m_laserInfo;       //波长和功率（通用）
    QString m_lineEnd = "\r\n";
    int waitTimeout = 150;


    QSerialPort *m_serial = nullptr;
    QList<int>commandToSend;
    int waveLength;
    QString portName;
    QString power;
    bool initSuccessFlag = true;

    void handleReceiveData(QString r, int type);

    void sendCommand();

};

#endif // OBISCTRL_H
