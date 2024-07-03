#include "imagewriter.h"
//#include "lzwcodec.h"

//#include <zlib.h>

//#include <opencv2/opencv.hpp>
#include <thread>
#include<QDebug>
#include<QThread>
namespace flsmio{

ImageWriter::ImageWriter(const QString &filePath,int totalcolumns, size_t imageNumber):m_dataStrip(512),m_compression(5){
    pool.setMaxThreadCount(5);
    column = totalcolumns;
    m_filePath=filePath;
    m_bDummpy=(imageNumber==1);
    if(m_bDummpy){return;}
    //只写打开或新建一个二进制文件；只允许写数据。
    m_file=fopen(filePath.toStdString().c_str(),"wb");
    if(nullptr==m_file){return;}
    //II+
    static const char header[16]={0x49,0x49,0x2b,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    fwrite(header,16,1,m_file);
    m_totalOffset=16;
}

ImageWriter::~ImageWriter(){dumpImages();}

bool ImageWriter::isOpen(){return m_bDummpy||nullptr!=m_file;}

bool ImageWriter::addImage(const cv::Mat &image){
    if(m_bDummpy){return cv::imwrite(m_filePath.toStdString(),image);}
    if(image.empty()||!isOpen()){
        //qDebug()<<"image is empty";
        return false;
    }

    Image *ptr=new Image;
    ptr->size=image.size();
    ptr->compression=m_compression;
    int w=image.cols,h=std::min(m_dataStrip,image.rows);
    ptr->dataStrip=h;
    //  ptr->dataStrip=image.rows;

    const size_t bufferSize=w*h*2;

    uchar *pData=image.data,*dst=(uchar*)malloc(bufferSize);
    for(int i=0;i<image.rows;i+=h,pData+=bufferSize)
    {
        int h1=std::min(h,image.rows-i);
        //...............................................size_t dstLen2=1.41*bufferSize+3
        size_t dstLen2=bufferSize,srcLen=w*h1*2;
        LZWCodeC::compress(pData,srcLen,dst,dstLen2);
        uchar *pDst=dst;
        fwrite(pDst,dstLen2,1,m_file);
        ptr->stripOffsets.append(m_totalOffset);
        ptr->stripLengths.append(dstLen2);
        m_totalOffset+=dstLen2;
    }

//    fwrite(pData,image.rows*image.cols*2,1,m_file);
//    ptr->stripOffsets.append(m_totalOffset);
//    ptr->stripLengths.append(image.rows*image.cols*2);
//    m_totalOffset+=image.rows*image.cols*2;

    m_images.append(ptr);
    free(dst);
    return true;
}

void ImageWriter::dumpImages(){
    if(!isOpen()||m_bDummpy){return;}
    if(m_images.empty()){fclose(m_file);return;}

    static const char footer[]={
        0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//8 directory entries in total

        0x00,0x01,//1st,image width
        0x10,0x00,//unsigned short
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//just one number in this directory entry
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//value of image width

        0x01,0x01,//2nd,image height
        0x10,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//value of image height

        0x02,0x01,//3rd,color depth
        0x03,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//16

        0x03,0x01,//4th,compressed
        0x03,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//1->no,8->deflate

        0x06,0x01,//5th,invert color
        0x03,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//no

        0x16,0x01,//6th,strip
        0x10,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//image height too

        0x17,0x01,//7th,total byte number
        0x10,0x00,//unsigned int
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//2*width*height

        0x11,0x01,//8th,image data's relative position to the beginning of the file
        0x10,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//16,length of the header

        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,//offset to next directory or zero
    };
    static const char description[]={
        0x0E,0x01,//9th,ImageDescription
        0x02,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    };
    const static QString metadata="<?xml version='1.0' encoding='UTF-8' standalone='no'?>"
                                  "<OME xmlns='http://www.openmicroscopy.org/Schemas/OME/2016-06' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' "
                                  "UUID='urn:uuid:4207e728-45ff-4f39-a4d8-556c1f90aa4c' "
                                  "xsi:schemaLocation='http://www.openmicroscopy.org/Schemas/OME/2016-06 http://www.openmicroscopy.org/Schemas/OME/2016-06/ome.xsd'>"
                                  "<Image ID='Image:0'>"
                                  "<Pixels DimensionOrder='XYZTC' ID='Pixels:0' SizeX='%1' SizeY='%2' SizeZ='%3' SizeT='%4' SizeC='1' Type='uint16'>"
                                  "<TiffData/></Pixels></Image></OME>";

    const Image *lastPtr=m_images.last();

    int stripNumber=lastPtr->stripLengths.length();bool bStrip=stripNumber>1;

    size_t footerSize=sizeof(footer),bufferSize=footerSize+(bStrip?stripNumber*16:0);
    char *buffer=(char*)malloc(bufferSize);memcpy(buffer,footer,footerSize);

    *(size_t*)(buffer+20)=lastPtr->size.width;*(size_t*)(buffer+40)=lastPtr->size.height;
    *(size_t*)(buffer+80)=lastPtr->compression;*(size_t*)(buffer+120)=lastPtr->dataStrip;
    *(size_t*)(buffer+132)=stripNumber;*(size_t*)(buffer+152)=stripNumber;
    size_t *pNext=(size_t*)(buffer+168);//*pLength=(size_t*)(buffer+140),*pOffset=(size_t*)(buffer+160),

    size_t firstIfdPos=m_totalOffset;

    //add ImageDescription for the first image
    QString metadata1=metadata.arg(QString::number(lastPtr->size.width),QString::number(lastPtr->size.height),QString::number(column),QString::number(m_images.length()));
    size_t descLength=metadata1.length()+1,firstBufferSize=bufferSize+20+descLength;//qDebug()<<footerSize<<descLength<<metadata1;

    char *firstBuffer=(char*)malloc(firstBufferSize);//sizeof(description)
    memcpy(firstBuffer,buffer,footerSize-8);memcpy(firstBuffer+footerSize-8,description,4);
    memcpy(firstBuffer+bufferSize+20,metadata1.toStdString().c_str(),descLength);

    firstBuffer[0]=0x09;*(size_t*)(firstBuffer+172)=descLength;
    size_t *pFirstDescription=(size_t*)(firstBuffer+180);
    bool bFirst=true;

    foreach(Image *ptr,m_images){
        char *buffer1=buffer;
        size_t footerSize1=footerSize,*pNext1=pNext,bufferSize1=bufferSize;
        if(bFirst){
            footerSize1=footerSize+20;pNext1=pFirstDescription+1;
            buffer1=firstBuffer;bufferSize1=firstBufferSize;
            *pFirstDescription=m_totalOffset+bufferSize1-descLength;bFirst=false;
        }

        size_t *pOffset=(size_t*)(buffer1+160),*pLength=(size_t*)(buffer1+140);

        if(bStrip){
            size_t *pStrip=(size_t*)(buffer1+footerSize1);//qDebug()<<ptr->stripOffsets;
            foreach(size_t v,ptr->stripOffsets){*pStrip=v;pStrip++;}
            foreach(size_t v,ptr->stripLengths){*pStrip=v;pStrip++;}
            *pOffset=m_totalOffset+footerSize1;*pLength=m_totalOffset+footerSize1+stripNumber*8;
        }else{*pLength=ptr->stripLengths[0];*pOffset=ptr->stripOffsets[0];}

        m_totalOffset+=bufferSize1;*pNext1=(lastPtr==ptr?0:m_totalOffset);
        fwrite(buffer1,bufferSize1,1,m_file);
    }

    _fseeki64(m_file,8,SEEK_SET);
    fwrite(&firstIfdPos,8,1,m_file);
    free(buffer);
    free(firstBuffer);
    qDeleteAll(m_images);
    fclose(m_file);
}

}
