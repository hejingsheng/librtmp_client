#include "DataBuf.h"
#include "logger.h"
#include <string.h>

DataRingBuf::DataRingBuf(int size)
{
	if (size == 0)
	{
		length_ = MAX_DATA_LEN;
	}
	else
	{
		length_ = size;
	}
	buf_ = new char[length_];
	r_Pos_ = length_;
	w_Pos_ = 0;
}

DataRingBuf::~DataRingBuf()
{
	if (buf_)
	{
		delete[] buf_;
		buf_ = nullptr;
	}
}

int DataRingBuf::writeData(const char *data, int len)
{
	if (w_Pos_ > r_Pos_)
	{
		int left = length_ - (w_Pos_ - r_Pos_);
		if (len > left)
		{
			//left space is less than len
			return ERROR_NOT_HAVE_SPACE;
		}
		else
		{
			if (length_ - w_Pos_ >= len)
			{
				memcpy(buf_ + w_Pos_, data, len);
				w_Pos_ += len;
			}
			else
			{
				int l = length_ - w_Pos_;
				memcpy(buf_ + w_Pos_, data, l);
				memcpy(buf_, data + l, len - l);
				w_Pos_ = (len - l);
			}
			return len;
		}
	}
	else if (w_Pos_ < r_Pos_)
	{
		int left = r_Pos_ - w_Pos_;
		if (len > left)
		{
			//left space is less than len
			return ERROR_NOT_HAVE_SPACE;
		}
		else
		{
			memcpy(buf_ + w_Pos_, data, len);
			w_Pos_ += len;
			return len;
		}
	}
	else
	{
		return ERROR_NOT_HAVE_SPACE;
	}
}

int DataRingBuf::readData(char *data, int size)
{
	int used;
	int len;
	if (w_Pos_ > r_Pos_)
	{
		used = w_Pos_ - r_Pos_;
		len = (used >= size) ? size : used;
		memcpy(data, buf_ + r_Pos_, len);
		r_Pos_ += len;
	}
	else if (w_Pos_ < r_Pos_)
	{
		used = length_ - (r_Pos_ - w_Pos_);
		len = (used >= size) ? size : used;
		if (len <= (length_ - r_Pos_))
		{
			memcpy(data, buf_ + r_Pos_, len);
			r_Pos_ += len;
		}
		else
		{
			int t = length_ - r_Pos_;
			memcpy(data, buf_ + r_Pos_, t);
			memcpy(data + t, buf_, len - t);
			r_Pos_ = (len - t);
		}
	}
	else
	{
		return ERROR_NOT_HAVE_DATA;
	}
	if (r_Pos_ == w_Pos_)
	{
		r_Pos_ = length_;
		w_Pos_ = 0;
	}
	return len;
}

char* DataRingBuf::getWritePtrAndLeft(int &left)
{
	left = length_ - getUsed();
	return buf_ + w_Pos_;
}

int DataRingBuf::getUsed() const
{
	int used;
	if (w_Pos_ > r_Pos_)
	{
		used = w_Pos_ - r_Pos_;
	}
	else if (w_Pos_ < r_Pos_)
	{
		used = length_ - (r_Pos_ - w_Pos_);
	}
	else
	{
		return 0;
	}
	return used;
}

bool DataRingBuf::isFull() const
{
	if (r_Pos_ == w_Pos_)
	{
		return true;
	}
	return false;
}

bool DataRingBuf::isEmpty() const
{
	if (r_Pos_ == length_ && w_Pos_ == 0)
	{
		return true;
	}
	return false;
}

int DataRingBuf::addRingBufSpace(int addSize)
{
	int len = getUsed();
	char *tmp = nullptr;
	if (len > 0)
	{
		tmp = new char[len];
		readData(tmp, len);
	}
	delete[] buf_;
	buf_ = new char[length_ + addSize];
	length_ += addSize;
	r_Pos_ = length_;
	w_Pos_ = 0;
	if (tmp != nullptr)
	{
		writeData(tmp, len);
		delete[] tmp;
	}
	return 0;
}

void DataRingBuf::clear()
{
	r_Pos_ = length_;
	w_Pos_ = 0;
}

DataCacheBuf::DataCacheBuf()
{
    bytes = new char[MAX_CACHE_LEN];
    nbytes = (MAX_CACHE_LEN);
    //pos = bytes;
    length = 0;
}

DataCacheBuf::~DataCacheBuf()
{
    if (bytes)
    {
        delete [] bytes;
    }
}

char* DataCacheBuf::data()
{
    return bytes;
}

int DataCacheBuf::size()
{
    return nbytes;
}

int DataCacheBuf::len()
{
    return length;
}

bool DataCacheBuf::require(int required_size)
{
    if (required_size < 0)
    {
        return false;
    }
    return required_size <= length;
}

void DataCacheBuf::push_data(char *data, int size)
{
    if (length + size > nbytes)
    {
        //need realloc memory
        ILOG("need realloc buffer\n");
        char *temp = new char[length + size + 2048];
        nbytes = (length + size + 2048);
        memcpy(temp, bytes, length);
        memcpy(temp+length, data, size);
        length += size;
        delete[] bytes;
        bytes = temp;
        return;
    }
    memcpy(bytes+length, data, size);
    length += size;
}

void DataCacheBuf::pop_data(int len)
{
    memmove(bytes, bytes+len, length-len);
    length -= len;
}

//char DataCacheBuf::read_1byte()
//{
//    return *p++;
//}
//
//short DataCacheBuf::read_2byte()
//{
//    short value;
//    char *pp = (char*)&value;
//    pp[1] = *p++;
//    pp[0] = *p++;
//    return value;
//}
//
//int DataCacheBuf::read_4byte()
//{
//    int value;
//    char *pp = (char*)&value;
//    pp[3] = *p++;
//    pp[2] = *p++;
//    pp[1] = *p++;
//    pp[0] = *p++;
//    return value;
//}
//
//void DataCacheBuf::read_bytes(char *data, int size)
//{
//    memcpy(data, p, size);
//    p += size;
//}
//
//
//void DataCacheBuf::write_1byte(char val)
//{
//    *p++ = val;
//}
//
//void DataCacheBuf::write_2byte(short val)
//{
//    char *pp = (char*)&val;
//    *p++ = pp[1];
//    *p++ = pp[0];
//}
//
//void DataCacheBuf::write_4byte(int val)
//{
//    char *pp = (char*)&val;
//    *p++ = pp[3];
//    *p++ = pp[2];
//    *p++ = pp[1];
//    *p++ = pp[0];
//}
//
//void DataCacheBuf::write_bytes(char *data, int size)
//{
//    memcpy(p, data, size);
//    p += size;
//}
