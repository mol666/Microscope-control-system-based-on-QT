#ifndef STAGEDIALOG_FROM_H
#define STAGEDIALOG_FROM_H

#include <QDialog>
#include "aerotech.h"
namespace Ui {
class StageDialog_From;
}


class StageDialog_From : public QDialog
{
    Q_OBJECT

public:
    explicit StageDialog_From(QWidget *parent = nullptr);
    ~StageDialog_From();


private:
    Ui::StageDialog_From *ui;

signals:
    void setAerotechFromToPara(motionPara para);
};

#endif
