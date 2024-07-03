#include "addimage.h"

addimage::addimage(cv::Mat image,bool bDummpy,FILE* &file,QList<Image*>& images,size_t& totalOffset,QObject *parent)
    : m_image(image),m_bDummpy(bDummpy),m_file(file),m_images(images),m_totalOffset(totalOffset),QObject{parent}
{
    // 任务执行完毕,该对象自动销毁
    setAutoDelete(true);
}

void addimage::run(){
    adim();
}

bool addimage::isopen()
{
    return m_bDummpy||nullptr!=m_file;
}

bool addimage::adim()
{
//    if(m_bDummpy){return cv::imwrite(m_filePath.toStdString(),m_image);}
    if(m_image.empty()||!isopen()){return false;}
    Image *ptr=new Image;
    ptr->size=m_image.size();
    ptr->compression=5;
    int w=m_image.cols,h=std::min(m_dataStrip,m_image.rows);
    ptr->dataStrip=h;

    const size_t bufferSize=w*h*2;
    uchar *pData=m_image.data,*dst=(uchar*)malloc(bufferSize);
    for(int i=0;i<m_image.rows;i+=h,pData+=bufferSize)
    {
        int h1=std::min(h,m_image.rows-i);
        //...............................................size_t dstLen2=1.41*bufferSize+3
        size_t dstLen2=bufferSize,srcLen=w*h1*2;
        LZWCodeC::compress(pData,srcLen,dst,dstLen2);
        uchar *pDst=dst;
        fwrite(pDst,dstLen2,1,m_file);
        ptr->stripOffsets.append(m_totalOffset);
        ptr->stripLengths.append(dstLen2);
        m_totalOffset+=dstLen2;
    }
    m_images.append(ptr);
    free(dst);
    return true;
}
