#ifndef ADDIMAGE_H
#define ADDIMAGE_H

#include <QObject>
#include<QRunnable>
#include <opencv2/opencv.hpp>
#include "lzwcodec.h"

struct Image{
    cv::Size size;
    int compression,dataStrip;
    size_t dataOffset,dataLength;
    QList<size_t> stripFileOffsets,stripLengths,stripOffsets;
};

class addimage : public QObject,public QRunnable
{
    Q_OBJECT
public:
    explicit addimage(cv::Mat image,bool bDummpy,FILE* &file,QList<Image*>& images,size_t& totalOffset,
                      QObject *parent = nullptr);
    //~addimage();
    void run() override;;   
    bool isopen();
    bool adim();
    cv::Mat m_image;
    bool m_bDummpy;
    FILE *m_file;
    size_t m_totalOffset;
    QList<Image*> m_images;
    const int m_dataStrip = 512,m_compression = 5;

signals:

};

#endif // ADDIMAGE_H
