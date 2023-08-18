#ifndef BUFFER_H
#define BUFFER_H

#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/uio.h>
#include <cstring>
#include <atomic>
#include <assert.h>

class Buffer {
public:
    Buffer(int initBuffSize=1024);
    ~Buffer()=default;

    size_t WritableBytes() const;//表示不会修改类中的数据成员
    size_t ReadableBytes() const;
    size_t PrependableBytes() const;

    const char* Peak() const;//buffer中有数据的开头的指针
    char* BeginWrite();
    const char* BeginWriteConst() const;

    ssize_t ReadFd(int fd, int* Errno);//将外来fd数据写到buffer_里
    ssize_t WriteFd(int fd,int* Errno);//将buffer_中的数据写到外界fd中

    void Append(const std::string& str);
    void Append(const Buffer& buff);
    void Append(const void* data,size_t len);
    void Append(const char* str,size_t len);//Append主要实现的函数

    void EnsureWritable(size_t len); 
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);
    void RetrieveAll();
    std::string RetrieveAllToStr();

private:
    const char* BeginPtr_() const;//buffer的开头的指针
    char* BeginPtr_();
    void MakeSpace_(size_t len); 
    std::vector<char> buffer_;
    std::atomic<std::size_t> readPos_;
    std::atomic<std::size_t> writePos_; 

};//原子变量，保证了数据访问的互斥性




#endif