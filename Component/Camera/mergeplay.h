#ifndef MERGEPLAY_H
#define MERGEPLAY_H

#include <QObject>
#include <QImage>
#include <QThread>

class MergePlay : public QObject
{
    Q_OBJECT
public:
    explicit MergePlay(QObject *parent = nullptr);

signals:
    void display(QImage image);
public slots:
    void rongheImage(QImage image1,QImage image2);

};

#endif // MERGEPLAY_H
