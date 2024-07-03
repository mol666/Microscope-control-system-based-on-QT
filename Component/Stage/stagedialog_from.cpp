#include "stagedialog_from.h"
#include "ui_stagedialog_from.h"
#include <QPushButton>
#include <QDebug>

StageDialog_From::StageDialog_From(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StageDialog_From)
{
    ui->setupUi(this);

    connect(ui->buttonBox,&QDialogButtonBox::clicked,this,[=](QAbstractButton *button)
    {

        if(button == ui->buttonBox->button(QDialogButtonBox::Save))
        {
            motionPara para(ui->doubleSB_X_start->value(),
                            ui->doubleSB_X_end->value(),
                            ui->doubleSB_from_X_vel->value(),
                            ui->doubleSB_Y_start->value(),
                            ui->doubleSB_Y_end->value(),
                            ui->doubleSB_from_Y_vel->value(),
                            ui->doubleSB_Z_start->value(),
                            ui->doubleSB_Z_end->value(),
                            ui->doubleSB_from_Z_vel->value());
            emit setAerotechFromToPara(para);
        }
    });
}

StageDialog_From::~StageDialog_From()
{
    delete ui;
}
