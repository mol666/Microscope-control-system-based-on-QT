#ifndef STAGEDIALOG_GOTO_H
#define STAGEDIALOG_GOTO_H

#include <QDialog>
namespace Ui {
class StageDialog_Goto;
}


class StageDialog_Goto : public QDialog
{
    Q_OBJECT

public:
    explicit StageDialog_Goto(QWidget *parent = nullptr);
    ~StageDialog_Goto();

signals:
    void setAerotechGoToPara(double,int);
    void setAerotechGoToPara(double,double,double);


public slots:
    void gotoDialogRenew(double X_pos,double Y_pos,double Z_pos);



private:
    Ui::StageDialog_Goto *ui;
};

#endif
