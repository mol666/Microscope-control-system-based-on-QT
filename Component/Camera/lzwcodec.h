#ifndef LZWCODEC_H
#define LZWCODEC_H

#include <cstdint>
#include <memory.h>
//#include <QRunnable>

class LZWCodeC
{
public:
    LZWCodeC();
//    LZWCodeC(void *buffer, size_t srcLength, void *dst, size_t &outSize);
    static void compress(void *src, size_t srcLength, void *dst, size_t &dstLength);

//    void run() override;
//    void* LZWbuffer;
//    void* LZWdst;
//    size_t LZWsrcLength;
//    size_t LZWoutSize;
};

#endif // LZWCODEC_H
