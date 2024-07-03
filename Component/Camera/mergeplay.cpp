#include "mergeplay.h"

MergePlay::MergePlay(QObject *parent)
    : QObject{parent}
{

}

void MergePlay::rongheImage(QImage image1, QImage image2)
{
    QImage image(2304,1024,QImage::Format_RGB32);
    for(int i=0;i<2304;i++){
        for(int j=0;j<1024;j++){
            image.setPixel(i,j,qRgb(image1.pixel(i,j),image2.pixel(i,j),0));
        }
    }
    emit display(image);
}
