#include "printerror.h"
#include<string>

inline const int my_dcamdev_string(DCAMERR& err,HDCAM hdcam,int32 idStr,char* text,int32 textbytes){
    DCAMDEV_STRING param;
    memset(&param,0,sizeof(param));
    param.size = sizeof(param);
    param.text = text;
    param.textbytes = textbytes;
    param.iString = idStr;

    err = dcamdev_getstring(hdcam,&param);
    return !failed(err);
}

void PrintError(HDCAM hdcam,DCAMERR errid,const char* apiname,const char* fmt,...){
    char errtext[256];
    DCAMERR err;
    my_dcamdev_string(err,hdcam,errid,errtext,sizeof(errtext));
    qDebug("FALLED:(DCAMERR)0x%08X %s @ %s", errid, errtext, apiname);
    if(fmt!=NULL){
        //qDebug()<<":";
        va_list arg;
        va_start(arg,fmt);
        vprintf(fmt,arg);
        va_end(arg);
    }
}

//有error则返回true
bool PrintError(){
    static QString lastError = "No Error";
    CHAR data[256];
    QChar h0 = 0x00;
    A3200GetLastErrorString(data,256);
    QString s = QString::fromLocal8Bit(data,256).remove(h0);
    if(s!="No Error"&&s!=lastError)
    {
        lastError = s;
        qWarning("A3200 ERROR : %s",data);
        return true;
    }
    return false;
}
