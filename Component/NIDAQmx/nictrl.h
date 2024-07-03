#ifndef NICTRL_H
#define NICTRL_H

#include <QObject>

typedef void*              TaskHandle;
typedef unsigned __int64   uInt64;
typedef double             float64;
typedef signed long        int32;

class NIctrl : public QObject
{
    Q_OBJECT

    TaskHandle m_htaskCtr,m_htaskAO,m_htaskDO,test_htaskAO,test_htaskDO;;
    uInt64 m_sampleCount;
    const float64 m_rate;

    bool m_bChanged,m_bUsetrigger,m_bShowSlash;

    void closeTask(TaskHandle &);bool failed(int32 e);
    void setSampleCount(uInt64 sampleCount);

public:
    explicit NIctrl(QObject *parent = nullptr);
    ~NIctrl();

    bool NIinit(double amplitude, double offset, QMap<int,bool> laserTriggerPort);
    bool openAO(double amplitude, double offset);
    void NIstart();
    void NIstart(uInt64 FiniteSampsCount);
    void pauseNICtr();
    void continueNICtr();
    void stop();
    bool stopClock();
    bool startClock();
    void useTrigger(bool);

    void setInterval(float time);
    bool changed();

signals:

};

#endif // NICTRL_H
