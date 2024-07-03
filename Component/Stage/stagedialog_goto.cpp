#include "stagedialog_goto.h"
#include "ui_stagedialog_goto.h"
#include "aerotech.h"
StageDialog_Goto::StageDialog_Goto(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StageDialog_Goto)
{
    ui->setupUi(this);

    //X参数变化，先发到UI线程
    connect(ui->doubleSB_X_desti,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[=](double value)
    {
        if(ui->checkBox->isChecked())
        emit setAerotechGoToPara(value,X);
    });
    //Y参数变化，先发到UI线程
    connect(ui->doubleSB_Y_desti,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[=](double value)
    {
        if(ui->checkBox->isChecked())
        emit setAerotechGoToPara(value,Y);
    });
    //Z参数变化，先发到UI线程
    connect(ui->doubleSB_Z_desti,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[=](double value)
    {
        if(ui->checkBox->isChecked())
        emit setAerotechGoToPara(value,Z);
    });

    //当设定为非realtime模式，点击Ok平台才会开始运动
    connect(ui->buttonBox,&QDialogButtonBox::clicked,this,[=]()
    {
        if(!ui->checkBox->isChecked())
        {
            emit setAerotechGoToPara(ui->doubleSB_X_desti->value(),
                                     ui->doubleSB_Y_desti->value(),
                                     ui->doubleSB_Z_desti->value());
        }

    });

}


StageDialog_Goto::~StageDialog_Goto()
{
    delete ui;

}


//更新参数值
void StageDialog_Goto::gotoDialogRenew(double X_pos, double Y_pos, double Z_pos)
{
    ui->doubleSB_X_desti->setValue(X_pos);
    ui->doubleSB_Y_desti->setValue(Y_pos);
    ui->doubleSB_Z_desti->setValue(Z_pos);
}




