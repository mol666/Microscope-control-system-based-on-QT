#include "cameracontrast.h"
#include "ui_cameracontrast.h"

CameraContrast::CameraContrast(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CameraContrast)
{
    ui->setupUi(this);
}

CameraContrast::~CameraContrast()
{
    delete ui;
}
