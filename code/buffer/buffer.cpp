#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize),readPos_(0),writePos_(0) {}

size_t Buffer::ReadableBytes() const {
    return writePos_-readPos_;
}

size_t Buffer::WritableBytes() const{
    return buffer_.size()-writePos_;
}

size_t Buffer:: PrependableBytes() const{
    return readPos_;
}


const char* Buffer::BeginPtr_() const{
    return &*buffer_.begin();
}
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::Peak() const{
    return BeginPtr_() + readPos_;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_()+writePos_;
}
char* Buffer::BeginWrite(){
    return BeginPtr_()+writePos_;
}

//向buffer_的writeable区写
void Buffer::Append(const std::string& str){
    Append(str.data(),str.length());
}
void Buffer::Append(const Buffer& buff){
    //把buff中可读部分传入
    Append(buff.Peak(),buff.ReadableBytes());
}
void Buffer::Append(const void* data,size_t len){
    assert(data);
    Append(static_cast<const char*>(data),len);
}
void Buffer::Append(const char* str,size_t len){
    assert(str);
    EnsureWritable(len);//先确保可写长度
    std::copy(str,str+len,BeginWrite());
    HasWritten(len);
}

void Buffer::HasWritten(size_t len) {
    writePos_ += len;
}

void Buffer::EnsureWritable(size_t len) {
    if (len>WritableBytes()) {
        MakeSpace_(len);
    }
    assert(WritableBytes()>=len);
} 

void Buffer::MakeSpace_(size_t len) {
    //buffer空间不够了，resize
    if (WritableBytes()+PrependableBytes() < len) {
        buffer_.resize(writePos_+len+1);
    }
    //buffer空间还够，只需挪一下数据的位置,相当于把prependable()这块儿与writable()合并了
    else {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_()+readPos_,BeginPtr_()+writePos_,BeginPtr_());
        readPos_=0;
        writePos_=readPos_+readable;
        assert(readable == ReadableBytes());
    }
}

//读buffer_的readable区
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}
void Buffer::RetrieveUntil(const char* end){
    assert(Peak() <= end);
    Retrieve(end-Peak());
}
void Buffer::RetrieveAll(){
    bzero(&buffer_[0],buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}
std::string Buffer::RetrieveAllToStr(){
    std::string str(Peak(),ReadableBytes());
    RetrieveAll();
    return str;
}

// 主要实现的函数
ssize_t Buffer::ReadFd(int fd,int* saveErrno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    //分散读，保证数据全部读完
    iov[0].iov_base = BeginPtr_()+ writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    //readv读多个非连续缓冲区
    const ssize_t len = readv(fd,iov,2);
    if (len < 0) {
        *saveErrno = errno;
    }
    else if (static_cast<size_t>(len)<=writable) {
        writePos_ += len;
    }
    else {
        writePos_ = buffer_.size();//writePos_指向了buffer_的末尾，剩下的在buff中，要添加在writePos_后面
        Append(buff,len-writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd,int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd,Peak(),readSize);
    if (len<0) {
        *saveErrno = errno;
        return len;
    }
    readPos_ += len;
    return len;
}