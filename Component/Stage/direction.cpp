#include "direction.h"
#include "ui_direction.h"
#include "stagedialog_goto.h"
#include "aerotech.h"
Direction::Direction(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Direction)
{
    ui->setupUi(this);

    connect(ui->step_Y_forward,&QPushButton::pressed,this,[=]()
    {
       emit setAerotechStepPara(ui->doubleSB_step_distance->value(),
                                ui->doubleSB_step_vel->value(),Y,posi);
    });
    connect(ui->step_Y_back,&QPushButton::pressed,this,[=]()
    {
       emit setAerotechStepPara(ui->doubleSB_step_distance->value(),
                                ui->doubleSB_step_vel->value(),Y,nega);
    });
    connect(ui->step_X_left,&QPushButton::pressed,this,[=]()
    {
       emit setAerotechStepPara(ui->doubleSB_step_distance->value(),
                                ui->doubleSB_step_vel->value(),X,nega);
    });
    connect(ui->step_X_right,&QPushButton::pressed,this,[=]()
    {
       emit setAerotechStepPara(ui->doubleSB_step_distance->value(),
                                ui->doubleSB_step_vel->value(),X,posi);
    });
    connect(ui->step_Z_up,&QPushButton::pressed,this,[=]()
    {
       emit setAerotechStepPara(ui->doubleSB_step_distance->value(),
                                ui->doubleSB_step_vel->value(),Z,nega);
    });
    connect(ui->step_Z_down,&QPushButton::pressed,this,[=]()
    {
       emit setAerotechStepPara(ui->doubleSB_step_distance->value(),
                                ui->doubleSB_step_vel->value(),Z,posi);
    });

}

Direction::~Direction()
{
    delete ui;
}

void Direction::StepButton(bool bBtn)
{
    ui->step_X_left->setEnabled(bBtn);
    ui->step_X_right->setEnabled(bBtn);
    ui->step_Y_back->setEnabled(bBtn);
    ui->step_Y_forward->setEnabled(bBtn);
    ui->step_Z_down->setEnabled(bBtn);
    ui->step_Z_up->setEnabled(bBtn);
}




