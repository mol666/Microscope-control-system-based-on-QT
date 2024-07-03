#ifndef CAMERACONTRAST_H
#define CAMERACONTRAST_H

#include <QWidget>

namespace Ui {
class CameraContrast;
}

class CameraContrast : public QWidget
{
    Q_OBJECT

public:
    explicit CameraContrast(QWidget *parent = nullptr);
    ~CameraContrast();

private:
    Ui::CameraContrast *ui;
};

#endif // CAMERACONTRAST_H
