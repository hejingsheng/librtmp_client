#ifndef _DATA_BUF_H_
#define _DATA_BUF_H_

const int MAX_DATA_LEN = 65535;
const int MAX_CACHE_LEN = (8 * 1024);

enum RingBufErrorCode
{
	ERROR_NOT_HAVE_SPACE = -2,
	ERROR_NOT_HAVE_DATA = -1,
};

class DataRingBuf
{
public:
	DataRingBuf(int size = 0);
	virtual ~DataRingBuf();

public:
	int writeData(const char *data, int len);
	int readData(char *data, int size);
	char* getWritePtrAndLeft(int &left);
	int getUsed() const;
	bool isFull() const;
	bool isEmpty() const;
	int addRingBufSpace(int addSize);
	void clear();

private:
	char *buf_;
	int r_Pos_;
	int w_Pos_;
	int length_;
};

class DataCacheBuf
{
public:
    DataCacheBuf();
    ~DataCacheBuf();

public:
    char *data();
    int size();
    int len();
    bool require(int required_size);

public:
    void push_data(char *data, int size);
    void pop_data(int len);

private:
    char *bytes;
    char *pos;
    int nbytes;
    int length;
};

#endif
