#ifndef DIRECTION_H
#define DIRECTION_H

#include <QWidget>
#include <QPushButton>

enum{
    posi,nega,
};

namespace Ui {
class Direction;
}

class Direction : public QWidget
{
    Q_OBJECT

public:
    explicit Direction(QWidget *parent = nullptr);
    ~Direction();

    void StepButton(bool bBtn);   //接收从aerotech发来的信号，暂时关闭/使能点动按键
signals:
    void setAerotechStepPara(double,double,int,int);

private slots:


private:
    Ui::Direction *ui;

};

#endif // DIRECTION_H
