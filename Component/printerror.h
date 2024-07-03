#ifndef PRINTERROR_H
#define PRINTERROR_H
#include"A3200.h"
#include"dcamapi4.h"
#include"dcamprop.h"
#include<QDebug>

//定义相机和Aerotech打印错误信息的接口
void PrintError(HDCAM hdcam,DCAMERR err,const char* apiname,const char* fmt=0,...);
bool PrintError();



#endif // PRINTERROR_H
