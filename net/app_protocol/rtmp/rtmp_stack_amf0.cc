#include "rtmp_stack_amf0.h"

#include <utility>
#include <vector>
#include <sstream>
#include "netio.h"
#include "string.h"

using namespace std;
using namespace rtmp_internal;

// AMF0 marker
#define RTMP_AMF0_Number                     0x00
#define RTMP_AMF0_Boolean                     0x01
#define RTMP_AMF0_String                     0x02
#define RTMP_AMF0_Object                     0x03
#define RTMP_AMF0_MovieClip                 0x04 // reserved, not supported
#define RTMP_AMF0_Null                         0x05
#define RTMP_AMF0_Undefined                 0x06
#define RTMP_AMF0_Reference                 0x07
#define RTMP_AMF0_EcmaArray                 0x08
#define RTMP_AMF0_ObjectEnd                 0x09
#define RTMP_AMF0_StrictArray                 0x0A
#define RTMP_AMF0_Date                         0x0B
#define RTMP_AMF0_LongString                 0x0C
#define RTMP_AMF0_UnSupported                 0x0D
#define RTMP_AMF0_RecordSet                 0x0E // reserved, not supported
#define RTMP_AMF0_XmlDocument                 0x0F
#define RTMP_AMF0_TypedObject                 0x10
// AVM+ object is the AMF3 object.
#define RTMP_AMF0_AVMplusObject             0x11
// origin array whos data takes the same form as LengthValueBytes
#define RTMP_AMF0_OriginStrictArray         0x20

// User defined
#define RTMP_AMF0_Invalid                     0x3F

RtmpAmf0Any::RtmpAmf0Any()
{
    marker = RTMP_AMF0_Invalid;
}

RtmpAmf0Any::~RtmpAmf0Any()
{
}

bool RtmpAmf0Any::is_string()
{
    return marker == RTMP_AMF0_String;
}

bool RtmpAmf0Any::is_boolean()
{
    return marker == RTMP_AMF0_Boolean;
}

bool RtmpAmf0Any::is_number()
{
    return marker == RTMP_AMF0_Number;
}

bool RtmpAmf0Any::is_null()
{
    return marker == RTMP_AMF0_Null;
}

bool RtmpAmf0Any::is_undefined()
{
    return marker == RTMP_AMF0_Undefined;
}

bool RtmpAmf0Any::is_object()
{
    return marker == RTMP_AMF0_Object;
}

bool RtmpAmf0Any::is_ecma_array()
{
    return marker == RTMP_AMF0_EcmaArray;
}

bool RtmpAmf0Any::is_strict_array()
{
    return marker == RTMP_AMF0_StrictArray;
}

bool RtmpAmf0Any::is_date()
{
    return marker == RTMP_AMF0_Date;
}

bool RtmpAmf0Any::is_complex_object()
{
    return is_object() || is_object_eof() || is_ecma_array() || is_strict_array();
}

string RtmpAmf0Any::to_str()
{
    RtmpAmf0String* p = dynamic_cast<RtmpAmf0String*>(this);
    return p->value;
}

const char* RtmpAmf0Any::to_str_raw()
{
    RtmpAmf0String* p = dynamic_cast<RtmpAmf0String*>(this);
    return p->value.data();
}

bool RtmpAmf0Any::to_boolean()
{
    RtmpAmf0Boolean* p = dynamic_cast<RtmpAmf0Boolean*>(this);
    return p->value;
}

double RtmpAmf0Any::to_number()
{
    RtmpAmf0Number* p = dynamic_cast<RtmpAmf0Number*>(this);
    return p->value;
}

int64_t RtmpAmf0Any::to_date()
{
    RtmpAmf0Date* p = dynamic_cast<RtmpAmf0Date*>(this);
    return p->date();
}

int16_t RtmpAmf0Any::to_date_time_zone()
{
    RtmpAmf0Date* p = dynamic_cast<RtmpAmf0Date*>(this);
    return p->time_zone();
}

RtmpAmf0Object* RtmpAmf0Any::to_object()
{
    RtmpAmf0Object* p = dynamic_cast<RtmpAmf0Object*>(this);
    return p;
}

RtmpAmf0EcmaArray* RtmpAmf0Any::to_ecma_array()
{
    RtmpAmf0EcmaArray* p = dynamic_cast<RtmpAmf0EcmaArray*>(this);
    return p;
}

RtmpAmf0StrictArray* RtmpAmf0Any::to_strict_array()
{
    RtmpAmf0StrictArray* p = dynamic_cast<RtmpAmf0StrictArray*>(this);
    return p;
}

void RtmpAmf0Any::set_number(double value)
{
    RtmpAmf0Number* p = dynamic_cast<RtmpAmf0Number*>(this);
    p->value = value;
}

bool RtmpAmf0Any::is_object_eof()
{
    return marker == RTMP_AMF0_ObjectEnd;
}

void rtmp_fill_level_spaces(stringstream& ss, int level)
{
    for (int i = 0; i < level; i++) {
        ss << "    ";
    }
}
void rtmp_amf0_do_print(RtmpAmf0Any* any, stringstream& ss, int level)
{
    std::ios_base::fmtflags oflags = ss.flags();
    
    if (any->is_boolean()) {
        ss << "Boolean " << (any->to_boolean()? "true":"false") << endl;
    } else if (any->is_number()) {
        ss << "Number " << std::fixed << any->to_number() << endl;
    } else if (any->is_string()) {
        ss << "String " << any->to_str() << endl;
    } else if (any->is_date()) {
        ss << "Date " << std::hex << any->to_date()
        << "/" << std::hex << any->to_date_time_zone() << endl;
    } else if (any->is_null()) {
        ss << "Null" << endl;
    } else if (any->is_undefined()) {
        ss << "Undefined" << endl;
    } else if (any->is_ecma_array()) {
        RtmpAmf0EcmaArray* obj = any->to_ecma_array();
        ss << "EcmaArray " << "(" << obj->count() << " items)" << endl;
        for (int i = 0; i < obj->count(); i++) {
            rtmp_fill_level_spaces(ss, level + 1);
            ss << "Elem '" << obj->key_at(i) << "' ";
            if (obj->value_at(i)->is_complex_object()) {
                rtmp_amf0_do_print(obj->value_at(i), ss, level + 1);
            } else {
                rtmp_amf0_do_print(obj->value_at(i), ss, 0);
            }
        }
    } else if (any->is_strict_array()) {
        RtmpAmf0StrictArray* obj = any->to_strict_array();
        ss << "StrictArray " << "(" << obj->count() << " items)" << endl;
        for (int i = 0; i < obj->count(); i++) {
            rtmp_fill_level_spaces(ss, level + 1);
            ss << "Elem ";
            if (obj->at(i)->is_complex_object()) {
                rtmp_amf0_do_print(obj->at(i), ss, level + 1);
            } else {
                rtmp_amf0_do_print(obj->at(i), ss, 0);
            }
        }
    } else if (any->is_object()) {
        RtmpAmf0Object* obj = any->to_object();
        ss << "Object " << "(" << obj->count() << " items)" << endl;
        for (int i = 0; i < obj->count(); i++) {
            rtmp_fill_level_spaces(ss, level + 1);
            ss << "Property '" << obj->key_at(i) << "' ";
            if (obj->value_at(i)->is_complex_object()) {
                rtmp_amf0_do_print(obj->value_at(i), ss, level + 1);
            } else {
                rtmp_amf0_do_print(obj->value_at(i), ss, 0);
            }
        }
    } else {
        ss << "Unknown" << endl;
    }
    
    ss.flags(oflags);
}

char* RtmpAmf0Any::human_print(char** pdata, int* psize)
{
    stringstream ss;
    
    ss.precision(1);
    
    rtmp_amf0_do_print(this, ss, 0);
    
    string str = ss.str();
    if (str.empty()) {
        return NULL;
    }
    
    char* data = new char[str.length() + 1];
    memcpy(data, str.data(), str.length());
    data[str.length()] = 0;
    
    if (pdata) {
        *pdata = data;
    }
    if (psize) {
        *psize = (int)str.length();
    }
    
    return data;
}

RtmpAmf0Any* RtmpAmf0Any::str(const char* value)
{
    return new RtmpAmf0String(value);
}

RtmpAmf0Any* RtmpAmf0Any::boolean(bool value)
{
    return new RtmpAmf0Boolean(value);
}

RtmpAmf0Any* RtmpAmf0Any::number(double value)
{
    return new RtmpAmf0Number(value);
}

RtmpAmf0Any* RtmpAmf0Any::null()
{
    return new RtmpAmf0Null();
}

RtmpAmf0Any* RtmpAmf0Any::undefined()
{
    return new RtmpAmf0Undefined();
}

RtmpAmf0Object* RtmpAmf0Any::object()
{
    return new RtmpAmf0Object();
}

RtmpAmf0Any* RtmpAmf0Any::object_eof()
{
    return new RtmpAmf0ObjectEOF();
}

RtmpAmf0EcmaArray* RtmpAmf0Any::ecma_array()
{
    return new RtmpAmf0EcmaArray();
}

RtmpAmf0StrictArray* RtmpAmf0Any::strict_array()
{
    return new RtmpAmf0StrictArray();
}

RtmpAmf0Any* RtmpAmf0Any::date(int64_t value)
{
    return new RtmpAmf0Date(value);
}

int RtmpAmf0Any::discovery(uint8_t *data, int len, RtmpAmf0Any** ppvalue)
{
    int err = 0;

    // detect the object-eof specially
    if (rtmp_amf0_is_object_eof(data, len)) {
        *ppvalue = new RtmpAmf0ObjectEOF();
        return err;
    }

    // marker
    if (len < 1) {
        return -1;
    }

    char marker = data[0];

    switch (marker) {
        case RTMP_AMF0_String: {
            *ppvalue = RtmpAmf0Any::str();
            return err;
        }
        case RTMP_AMF0_Boolean: {
            *ppvalue = RtmpAmf0Any::boolean();
            return err;
        }
        case RTMP_AMF0_Number: {
            *ppvalue = RtmpAmf0Any::number();
            return err;
        }
        case RTMP_AMF0_Null: {
            *ppvalue = RtmpAmf0Any::null();
            return err;
        }
        case RTMP_AMF0_Undefined: {
            *ppvalue = RtmpAmf0Any::undefined();
            return err;
        }
        case RTMP_AMF0_Object: {
            *ppvalue = RtmpAmf0Any::object();
            return err;
        }
        case RTMP_AMF0_EcmaArray: {
            *ppvalue = RtmpAmf0Any::ecma_array();
            return err;
        }
        case RTMP_AMF0_StrictArray: {
            *ppvalue = RtmpAmf0Any::strict_array();
            return err;
        }
        case RTMP_AMF0_Date: {
            *ppvalue = RtmpAmf0Any::date();
            return err;
        }
        case RTMP_AMF0_Invalid:
        default: {
            return -2;
        }
    }
}

UnSortedHashtable::UnSortedHashtable()
{
}

UnSortedHashtable::~UnSortedHashtable()
{
    clear();
}

int UnSortedHashtable::count()
{
    return (int)properties.size();
}

void UnSortedHashtable::clear()
{
    std::vector<RtmpAmf0ObjectPropertyType>::iterator it;
    for (it = properties.begin(); it != properties.end(); ++it) {
        RtmpAmf0ObjectPropertyType& elem = *it;
        RtmpAmf0Any* any = elem.second;
        delete any;
    }
    properties.clear();
}

string UnSortedHashtable::key_at(int index)
{
    RtmpAmf0ObjectPropertyType& elem = properties[index];
    return elem.first;
}

const char* UnSortedHashtable::key_raw_at(int index)
{
    RtmpAmf0ObjectPropertyType& elem = properties[index];
    return elem.first.data();
}

RtmpAmf0Any* UnSortedHashtable::value_at(int index)
{
    RtmpAmf0ObjectPropertyType& elem = properties[index];
    return elem.second;
}

void UnSortedHashtable::set(string key, RtmpAmf0Any* value)
{
    std::vector<RtmpAmf0ObjectPropertyType>::iterator it;
    
    for (it = properties.begin(); it != properties.end(); ++it) {
        RtmpAmf0ObjectPropertyType& elem = *it;
        std::string name = elem.first;
        RtmpAmf0Any* any = elem.second;
        if (key == name) {
            delete any;
            properties.erase(it);
            break;
        }
    }
    
    if (value) {
        properties.push_back(std::make_pair(key, value));
    }
}

RtmpAmf0Any* UnSortedHashtable::get_property(string name)
{
    std::vector<RtmpAmf0ObjectPropertyType>::iterator it;
    
    for (it = properties.begin(); it != properties.end(); ++it) {
        RtmpAmf0ObjectPropertyType& elem = *it;
        std::string key = elem.first;
        RtmpAmf0Any* any = elem.second;
        if (key == name) {
            return any;
        }
    }
    
    return NULL;
}

RtmpAmf0Any* UnSortedHashtable::ensure_property_string(string name)
{
    RtmpAmf0Any* prop = get_property(name);
    
    if (!prop) {
        return NULL;
    }
    
    if (!prop->is_string()) {
        return NULL;
    }
    
    return prop;
}

RtmpAmf0Any* UnSortedHashtable::ensure_property_number(string name)
{
    RtmpAmf0Any* prop = get_property(name);
    
    if (!prop) {
        return NULL;
    }
    
    if (!prop->is_number()) {
        return NULL;
    }
    
    return prop;
}

void UnSortedHashtable::remove(string name)
{
    std::vector<RtmpAmf0ObjectPropertyType>::iterator it;
    
    for (it = properties.begin(); it != properties.end();) {
        std::string key = it->first;
        RtmpAmf0Any* any = it->second;
        
        if (key == name) {
            delete any;
            
            it = properties.erase(it);
        } else {
            ++it;
        }
    }
}

void UnSortedHashtable::copy(UnSortedHashtable* src)
{
    std::vector<RtmpAmf0ObjectPropertyType>::iterator it;
    for (it = src->properties.begin(); it != src->properties.end(); ++it) {
        RtmpAmf0ObjectPropertyType& elem = *it;
        std::string key = elem.first;
        RtmpAmf0Any* any = elem.second;
        set(key, any->copy());
    }
}

RtmpAmf0ObjectEOF::RtmpAmf0ObjectEOF()
{
    marker = RTMP_AMF0_ObjectEnd;
}

RtmpAmf0ObjectEOF::~RtmpAmf0ObjectEOF()
{
}

int RtmpAmf0ObjectEOF::total_size()
{
    return RtmpAmf0Size::object_eof();
}

int RtmpAmf0ObjectEOF::read(uint8_t *data, int len)
{
    int offset = 0;
    // value
    if (len < 3) {
        return -1;
    }
    int16_t temp = 0;
    offset += read_int16(data+offset, &temp);
    if (temp != 0x00) {
        //return srs_error_new(ERROR_RTMP_AMF0_DECODE, "EOF invalid marker=%#x", temp);
        return -2;
    }

    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_ObjectEnd) {
        //return srs_error_new(ERROR_RTMP_AMF0_DECODE, "EOF invalid marker=%#x", marker);
        return -2;
    }

    return offset;
}

int RtmpAmf0ObjectEOF::write(uint8_t *data, int len)
{
    int err = 0;
    int offset = 0;

    if (len < 3)
    {
        return -1;
    }
    // value
    data[offset++] = 0x00;
    data[offset++] = 0x00;
    data[offset++] = RTMP_AMF0_ObjectEnd;

    return offset;
}

RtmpAmf0Any* RtmpAmf0ObjectEOF::copy()
{
    return new RtmpAmf0ObjectEOF();
}

RtmpAmf0Object::RtmpAmf0Object()
{
    properties = new UnSortedHashtable();
    eof = new RtmpAmf0ObjectEOF();
    marker = RTMP_AMF0_Object;
}

RtmpAmf0Object::~RtmpAmf0Object()
{
    delete (properties);
    delete (eof);
}

int RtmpAmf0Object::total_size()
{
    int size = 1;
    
    for (int i = 0; i < properties->count(); i++){
        std::string name = key_at(i);
        RtmpAmf0Any* value = value_at(i);
        
        size += RtmpAmf0Size::utf8(name);
        size += RtmpAmf0Size::any(value);
    }
    
    size += RtmpAmf0Size::object_eof();
    
    return size;
}

int RtmpAmf0Object::read(uint8_t *data, int len)
{
    int offset = 0;
    int ret;

    if (len < 1) {
        return 1;
    }
    // marker
    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_Object) {
        return -2;
    }

    // value
    while (offset < len) {
        // detect whether is eof.
        if (rtmp_amf0_is_object_eof(data+offset, len-offset)) {
            RtmpAmf0ObjectEOF pbj_eof;
            if ((ret = pbj_eof.read(data+offset, len-offset)) < 0) {
                return ret;
            }
            offset += ret;
            break;
        }

        // property-name: utf8 string
        std::string property_name;
        if ((ret = rtmp_amf0_read_utf8(data+offset, len-offset, property_name)) < 0) {
            return ret;
        }
        offset += ret;
        // property-value: any
        RtmpAmf0Any* property_value = nullptr;
        if ((ret = rtmp_amf0_read_any(data+offset, len-offset, &property_value)) < 0) {
            if (property_value) {
                delete property_value;
            }
            return ret;
        }
        offset += ret;

        // add property
        this->set(property_name, property_value);
    }

    return offset;
}

int RtmpAmf0Object::write(uint8_t *data, int len)
{
    int offset = 0;
    int ret;

    if (len < 1) {
        return -1;
    }
    // marker
    offset += write_int8(data+offset, RTMP_AMF0_Object);

    // value
    for (int i = 0; i < properties->count(); i++) {
        std::string name = this->key_at(i);
        RtmpAmf0Any* any = this->value_at(i);

        if ((ret = rtmp_amf0_write_utf8(data+offset, len-offset, name)) < 0) {
            return ret;
        }
        offset += ret;
        if ((ret = rtmp_amf0_write_any(data+offset, len-offset, any)) < 0) {
            return ret;
        }
        offset += ret;
    }

    if ((ret = eof->write(data+offset, len-offset)) < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpAmf0Any* RtmpAmf0Object::copy()
{
    RtmpAmf0Object* copy = new RtmpAmf0Object();
    copy->properties->copy(properties);
    return copy;
}

void RtmpAmf0Object::clear()
{
    properties->clear();
}

int RtmpAmf0Object::count()
{
    return properties->count();
}

string RtmpAmf0Object::key_at(int index)
{
    return properties->key_at(index);
}

const char* RtmpAmf0Object::key_raw_at(int index)
{
    return properties->key_raw_at(index);
}

RtmpAmf0Any* RtmpAmf0Object::value_at(int index)
{
    return properties->value_at(index);
}

void RtmpAmf0Object::set(string key, RtmpAmf0Any* value)
{
    properties->set(key, value);
}

RtmpAmf0Any* RtmpAmf0Object::get_property(string name)
{
    return properties->get_property(name);
}

RtmpAmf0Any* RtmpAmf0Object::ensure_property_string(string name)
{
    return properties->ensure_property_string(name);
}

RtmpAmf0Any* RtmpAmf0Object::ensure_property_number(string name)
{
    return properties->ensure_property_number(name);
}

void RtmpAmf0Object::remove(string name)
{
    properties->remove(name);
}

RtmpAmf0EcmaArray::RtmpAmf0EcmaArray()
{
    _count = 0;
    properties = new UnSortedHashtable();
    eof = new RtmpAmf0ObjectEOF();
    marker = RTMP_AMF0_EcmaArray;
}

RtmpAmf0EcmaArray::~RtmpAmf0EcmaArray()
{
    delete (properties);
    delete (eof);
}

int RtmpAmf0EcmaArray::total_size()
{
    int size = 1 + 4;
    
    for (int i = 0; i < properties->count(); i++){
        std::string name = key_at(i);
        RtmpAmf0Any* value = value_at(i);
        
        size += RtmpAmf0Size::utf8(name);
        size += RtmpAmf0Size::any(value);
    }
    
    size += RtmpAmf0Size::object_eof();
    
    return size;
}

int RtmpAmf0EcmaArray::read(uint8_t *data, int len)
{
    int offset = 0;
    int ret;

    if (len < 5) {
        return -1;
    }
    // marker
    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_EcmaArray) {
        return -2;
    }

    // count
    int32_t count = 0;
    offset += read_int32(data+offset, &count);
    // value
    this->_count = count;

    while (offset < len) {
        // detect whether is eof.

        if (rtmp_amf0_is_object_eof(data+offset, len-offset)) {
            RtmpAmf0ObjectEOF pbj_eof;
            if ((ret = pbj_eof.read(data+offset, len-offset)) < 0) {
                return ret;
            }
            offset += ret;
            break;
        }

        // property-name: utf8 string
        std::string property_name;
        if ((ret =rtmp_amf0_read_utf8(data+offset, len-offset, property_name)) < 0) {
            return ret;
        }
        offset += ret;
        // property-value: any
        RtmpAmf0Any* property_value = NULL;
        if ((ret = rtmp_amf0_read_any(data+offset, len-offset, &property_value)) < 0) {
            return ret;
        }
        offset += ret;
        // add property
        this->set(property_name, property_value);
    }

    return offset;
}

int RtmpAmf0EcmaArray::write(uint8_t *data, int len)
{
    int offset = 0;
    int ret;

    if (len < 5) {
        return -1;
    }
    // marker
    offset += write_int8(data+offset, RTMP_AMF0_EcmaArray);

    // count
    offset += write_int32(data+offset, this->_count);

    // value
    for (int i = 0; i < properties->count(); i++) {
        std::string name = this->key_at(i);
        RtmpAmf0Any* any = this->value_at(i);

        ret = rtmp_amf0_write_utf8(data+offset, len-offset, name);
        if (ret < 0) {
            return ret;
        }
        offset += ret;
        ret = rtmp_amf0_write_any(data+offset, len-offset, any);
        if (ret < 0) {
            return ret;
        }
        offset += ret;
    }

    ret = eof->write(data+offset, len-offset);
    if (ret < 0) {
        return ret;
    }
    offset += ret;
    return offset;
}

RtmpAmf0Any* RtmpAmf0EcmaArray::copy()
{
    RtmpAmf0EcmaArray* copy = new RtmpAmf0EcmaArray();
    copy->properties->copy(properties);
    copy->_count = _count;
    return copy;
}

void RtmpAmf0EcmaArray::clear()
{
    properties->clear();
}

int RtmpAmf0EcmaArray::count()
{
    return properties->count();
}

string RtmpAmf0EcmaArray::key_at(int index)
{
    return properties->key_at(index);
}

const char* RtmpAmf0EcmaArray::key_raw_at(int index)
{
    return properties->key_raw_at(index);
}

RtmpAmf0Any* RtmpAmf0EcmaArray::value_at(int index)
{
    return properties->value_at(index);
}

void RtmpAmf0EcmaArray::set(string key, RtmpAmf0Any* value)
{
    properties->set(key, value);
}

RtmpAmf0Any* RtmpAmf0EcmaArray::get_property(string name)
{
    return properties->get_property(name);
}

RtmpAmf0Any* RtmpAmf0EcmaArray::ensure_property_string(string name)
{
    return properties->ensure_property_string(name);
}

RtmpAmf0Any* RtmpAmf0EcmaArray::ensure_property_number(string name)
{
    return properties->ensure_property_number(name);
}

RtmpAmf0StrictArray::RtmpAmf0StrictArray()
{
    marker = RTMP_AMF0_StrictArray;
    _count = 0;
}

RtmpAmf0StrictArray::~RtmpAmf0StrictArray()
{
    clear();
}

int RtmpAmf0StrictArray::total_size()
{
    int size = 1 + 4;
    
    for (int i = 0; i < (int)properties.size(); i++){
        RtmpAmf0Any* any = properties[i];
        size += any->total_size();
    }
    
    return size;
}

int RtmpAmf0StrictArray::read(uint8_t *data, int len)
{
    int offset = 0;
    int ret;

    if (len < 5) {
        return -1;
    }
    // marker
    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_StrictArray) {
        return -2;
    }

    // count
    int32_t count = 0;
    offset += read_int32(data+offset, &count);

    // value
    this->_count = count;

    for (int i = 0; i < count && offset < len; i++) {
        // property-value: any
        RtmpAmf0Any* elem = NULL;
        ret = rtmp_amf0_read_any(data+offset, len-offset, &elem);
        if (ret < 0) {
            return ret;
        }
        offset += ret;
        // add property
        properties.push_back(elem);
    }

    return offset;
}

int RtmpAmf0StrictArray::write(uint8_t *data, int len)
{
    int offset = 0;
    int ret;

    if (len < 5) {
        return -1;
    }
    // marker
    offset += write_int8(data+offset, RTMP_AMF0_StrictArray);

    // count
    offset += write_int32(data+offset, this->_count);

    // value
    for (int i = 0; i < (int)properties.size(); i++) {
        RtmpAmf0Any* any = properties[i];

        ret = rtmp_amf0_write_any(data+offset, len-offset, any);
        if (ret < 0) {
            return -1;
        }
        offset += ret;
    }

    return offset;
}

RtmpAmf0Any* RtmpAmf0StrictArray::copy()
{
    RtmpAmf0StrictArray* copy = new RtmpAmf0StrictArray();
    
    std::vector<RtmpAmf0Any*>::iterator it;
    for (it = properties.begin(); it != properties.end(); ++it) {
        RtmpAmf0Any* any = *it;
        copy->append(any->copy());
    }
    
    copy->_count = _count;
    return copy;
}

void RtmpAmf0StrictArray::clear()
{
    std::vector<RtmpAmf0Any*>::iterator it;
    for (it = properties.begin(); it != properties.end(); ++it) {
        RtmpAmf0Any* any = *it;
        delete any;
    }
    properties.clear();
}

int RtmpAmf0StrictArray::count()
{
    return (int)properties.size();
}

RtmpAmf0Any* RtmpAmf0StrictArray::at(int index)
{
    //srs_assert(index < (int)properties.size());
    return properties.at(index);
}

void RtmpAmf0StrictArray::append(RtmpAmf0Any* any)
{
    properties.push_back(any);
    _count = (int32_t)properties.size();
}

int RtmpAmf0Size::utf8(string value)
{
    return (int)(2 + value.length());
}

int RtmpAmf0Size::str(string value)
{
    return 1 + RtmpAmf0Size::utf8(value);
}

int RtmpAmf0Size::number()
{
    return 1 + 8;
}

int RtmpAmf0Size::date()
{
    return 1 + 8 + 2;
}

int RtmpAmf0Size::null()
{
    return 1;
}

int RtmpAmf0Size::undefined()
{
    return 1;
}

int RtmpAmf0Size::boolean()
{
    return 1 + 1;
}

int RtmpAmf0Size::object(RtmpAmf0Object* obj)
{
    if (!obj) {
        return 0;
    }
    
    return obj->total_size();
}

int RtmpAmf0Size::object_eof()
{
    return 2 + 1;
}

int RtmpAmf0Size::ecma_array(RtmpAmf0EcmaArray* arr)
{
    if (!arr) {
        return 0;
    }
    
    return arr->total_size();
}

int RtmpAmf0Size::strict_array(RtmpAmf0StrictArray* arr)
{
    if (!arr) {
        return 0;
    }
    
    return arr->total_size();
}

int RtmpAmf0Size::any(RtmpAmf0Any* o)
{
    if (!o) {
        return 0;
    }
    
    return o->total_size();
}

RtmpAmf0String::RtmpAmf0String(const char* _value)
{
    marker = RTMP_AMF0_String;
    if (_value) {
        value = _value;
    }
}

RtmpAmf0String::~RtmpAmf0String()
{
}

int RtmpAmf0String::total_size()
{
    return RtmpAmf0Size::str(value);
}

int RtmpAmf0String::read(uint8_t *data, int len)
{
    return rtmp_amf0_read_string(data, len, value);
}

int RtmpAmf0String::write(uint8_t *data, int len)
{
    return rtmp_amf0_write_string(data, len, value);
}

RtmpAmf0Any* RtmpAmf0String::copy()
{
    RtmpAmf0String* copy = new RtmpAmf0String(value.c_str());
    return copy;
}

RtmpAmf0Boolean::RtmpAmf0Boolean(bool _value)
{
    marker = RTMP_AMF0_Boolean;
    value = _value;
}

RtmpAmf0Boolean::~RtmpAmf0Boolean()
{
}

int RtmpAmf0Boolean::total_size()
{
    return RtmpAmf0Size::boolean();
}

int RtmpAmf0Boolean::read(uint8_t *data, int len)
{
    return rtmp_amf0_read_boolean(data, len, value);
}

int RtmpAmf0Boolean::write(uint8_t *data, int len)
{
    return rtmp_amf0_write_boolean(data, len, value);
}

RtmpAmf0Any* RtmpAmf0Boolean::copy()
{
    RtmpAmf0Boolean* copy = new RtmpAmf0Boolean(value);
    return copy;
}

RtmpAmf0Number::RtmpAmf0Number(double _value)
{
    marker = RTMP_AMF0_Number;
    value = _value;
}

RtmpAmf0Number::~RtmpAmf0Number()
{
}

int RtmpAmf0Number::total_size()
{
    return RtmpAmf0Size::number();
}

int RtmpAmf0Number::read(uint8_t *data, int len)
{
    return rtmp_amf0_read_number(data, len, value);
}

int RtmpAmf0Number::write(uint8_t *data, int len)
{
    return rtmp_amf0_write_number(data, len, value);
}

RtmpAmf0Any* RtmpAmf0Number::copy()
{
    RtmpAmf0Number* copy = new RtmpAmf0Number(value);
    return copy;
}

RtmpAmf0Date::RtmpAmf0Date(int64_t value)
{
    marker = RTMP_AMF0_Date;
    _date_value = value;
    _time_zone = 0;
}

RtmpAmf0Date::~RtmpAmf0Date()
{
}

int RtmpAmf0Date::total_size()
{
    return RtmpAmf0Size::date();
}

int RtmpAmf0Date::read(uint8_t *data, int len)
{
    int offset = 0;

    if (len < 11) {
        return -1;
    }
    // marker
    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_Date) {
        return -2;
    }

    // date value
    // An ActionScript Date is serialized as the number of milliseconds
    // elapsed since the epoch of midnight on 1st Jan 1970 in the UTC
    // time zone.
    offset += read_int64(data+offset, &_date_value);

    // time zone
    // While the design of this type reserves room for time zone offset
    // information, it should not be filled in, nor used, as it is unconventional
    // to change time zones when serializing dates on a network. It is suggested
    // that the time zone be queried independently as needed.
    offset += read_int16(data+offset, &_time_zone);

    return offset;
}

int RtmpAmf0Date::write(uint8_t *data, int len)
{
    int offset = 0;

    // marker
    if (len < 11) {
        return -1;
    }
    offset += write_int8(data+offset, RTMP_AMF0_Date);

    // date value
    offset += write_int64(data+offset, _date_value);

    // time zone
    offset += write_int16(data+offset, _time_zone);

    return offset;
}

RtmpAmf0Any* RtmpAmf0Date::copy()
{
    RtmpAmf0Date* copy = new RtmpAmf0Date(0);
    
    copy->_date_value = _date_value;
    copy->_time_zone = _time_zone;
    
    return copy;
}

int64_t RtmpAmf0Date::date()
{
    return _date_value;
}

int16_t RtmpAmf0Date::time_zone()
{
    return _time_zone;
}

RtmpAmf0Null::RtmpAmf0Null()
{
    marker = RTMP_AMF0_Null;
}

RtmpAmf0Null::~RtmpAmf0Null()
{
}

int RtmpAmf0Null::total_size()
{
    return RtmpAmf0Size::null();
}

int RtmpAmf0Null::read(uint8_t *data, int len)
{
    return rtmp_amf0_read_null(data, len);
}

int RtmpAmf0Null::write(uint8_t *data, int len)
{
    return rtmp_amf0_write_null(data, len);
}

RtmpAmf0Any* RtmpAmf0Null::copy()
{
    RtmpAmf0Null* copy = new RtmpAmf0Null();
    return copy;
}

RtmpAmf0Undefined::RtmpAmf0Undefined()
{
    marker = RTMP_AMF0_Undefined;
}

RtmpAmf0Undefined::~RtmpAmf0Undefined()
{
}

int RtmpAmf0Undefined::total_size()
{
    return RtmpAmf0Size::undefined();
}

int RtmpAmf0Undefined::read(uint8_t *data, int len)
{
    return rtmp_amf0_read_undefined(data, len);
}

int RtmpAmf0Undefined::write(uint8_t *data, int len)
{
    return rtmp_amf0_write_undefined(data, len);
}

RtmpAmf0Any* RtmpAmf0Undefined::copy()
{
    RtmpAmf0Undefined* copy = new RtmpAmf0Undefined();
    return copy;
}

int rtmp_amf0_read_any(uint8_t *data, int len, RtmpAmf0Any** ppvalue)
{
    int ret;
    int offset = 0;

    if ((ret = RtmpAmf0Any::discovery(data+offset, len-offset, ppvalue)) != 0) {
        return -1;
    }

    if ((ret = (*ppvalue)->read(data+offset, len-offset)) < 0) {
        delete (*ppvalue);
        return -1;
    }
    offset += ret;
    return offset;
}

int rtmp_amf0_read_string(uint8_t *data, int len, string& value)
{
    // marker
    int offset = 0;
    int ret = 0;

    if (len < 1)
    {
        return -1;
    }

    int8_t marker;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_String) {
        //return srs_error_new(ERROR_RTMP_AMF0_DECODE, "String invalid marker=%#x", marker);
        return -1;
    }
    ret = rtmp_amf0_read_utf8(data+offset, len-offset, value);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    return offset;
}

int rtmp_amf0_write_string(uint8_t *data, int len, string value)
{   
    // marker
    int offset = 0;
    int ret = 0;
    if (len < 1)
    {
        return -1;
    }
    data[offset] = RTMP_AMF0_String;
    offset++;
    ret = rtmp_amf0_write_utf8(data+offset, len-offset, value);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    return offset;
}

int rtmp_amf0_read_boolean(uint8_t *data, int len, bool& value)
{
    int offset = 0;

    // marker
    if (len < 2) {
        return -1;
    }

    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_Boolean) {
        return -2;
    }

    int8_t val = 0;
    offset += read_int8(data+offset, &val);
    value = (val != 0);

    return offset;
}

int rtmp_amf0_write_boolean(uint8_t *data, int len, bool value)
{
    int offset = 0;

    // marker
    if (len < 2) {
        return -1;
    }
    offset += write_int8(data+offset, RTMP_AMF0_Boolean);

    // value
    if (value) {
        offset += write_int8(data+offset, 0x01);
    } else {
        offset += write_int8(data+offset, 0x00);
    }

    return offset;
}

int rtmp_amf0_read_number(uint8_t *data, int len, double& value)
{
    int offset = 0;

    // marker
    if (len < 9) {
        return -1;
    }

    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_Number) {
        return -1;
    }


    int64_t temp = 0;
    offset += read_int64(data+offset, &temp);
    memcpy(&value, &temp, 8);

    return offset;
}

int rtmp_amf0_write_number(uint8_t *data, int len, double value)
{
    int offset = 0;

    // marker
    if (len < 9) {
        return -1;
    }

    offset += write_int8(data+offset, RTMP_AMF0_Number);

    int64_t temp = 0x00;
    memcpy(&temp, &value, 8);
    offset += write_int64(data+offset, temp);

    return offset;
}

int rtmp_amf0_read_null(uint8_t *data, int len)
{
    int offset = 0;

    // marker
    if (len < 1) {
        return -1;
    }

    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_Null) {
        return -2;
    }

    return offset;
}

int rtmp_amf0_write_null(uint8_t *data, int len)
{
    int offset = 0;

    // marker
    if (len < 1) {
        return -1;
    }

    offset += write_int8(data+offset, RTMP_AMF0_Null);

    return offset;
}

int rtmp_amf0_read_undefined(uint8_t *data, int len)
{
    int offset = 0;

    // marker
    if (len < 1) {
        return -1;
    }

    int8_t marker = 0;
    offset += read_int8(data+offset, &marker);
    if (marker != RTMP_AMF0_Undefined) {
        return -2;
    }

    return offset;
}

int rtmp_amf0_write_undefined(uint8_t *data, int len)
{
    int offset = 0;

    // marker
    if (len < 1) {
        return -1;
    }

    data[offset] = RTMP_AMF0_Undefined;
    offset++;

    return offset;
}

namespace rtmp_internal
{
    int rtmp_amf0_read_utf8(uint8_t *data, int len, string& value)
    {
        int offset = 0;
        // len
        if (len < 2) {
            //return srs_error_new(ERROR_RTMP_AMF0_DECODE, "requires 2 only %d bytes", stream->left());
            return -1;
        }
        int16_t length = 0;
        offset += read_int16(data+offset, &length);
        // empty string
        if (length <= 0) {
            return offset;
        }

        // data
        if (len - offset < length)
        {
            return -1;
        }

        std::string str;
        str.append((char*)data+offset, length);
        offset += length;
        // support utf8-1 only
        // 1.3.1 Strings and UTF-8
        // UTF8-1 = %x00-7F
        // TODO: support other utf-8 strings
        /*for (int i = 0; i < len; i++) {
         char ch = *(str.data() + i);
         if ((ch & 0x80) != 0) {
         ret = ERROR_RTMP_AMF0_DECODE;
         srs_error("ignored. only support utf8-1, 0x00-0x7F, actual is %#x. ret=%d", (int)ch, ret);
         ret = ERROR_SUCCESS;
         }
         }*/

        value = str;

        return offset;
    }
    
    int rtmp_amf0_write_utf8(uint8_t *data, int len, string value)
    {
        int offset = 0;
        int16_t valuelen = 0;
        // len
        valuelen = (int16_t)value.length();
        if (len < 2 + valuelen) {
            return -1;
        }
        offset += write_int16(data+offset, valuelen);

        // empty string
        if (valuelen <= 0) {
            return offset;
        }

        // data
        memcpy(data+offset, value.c_str(), valuelen);
        offset += valuelen;
        return offset;
    }
    
    bool rtmp_amf0_is_object_eof(uint8_t *data, int len)
    {
        // detect the object-eof specially
        if (len >= 3) {
            int32_t flag = data[0] << 16 | data[1] << 8 | data[2];
            return 0x09 == flag;
        }

        return false;
    }
    
    int rtmp_amf0_write_object_eof(uint8_t *data, int len, RtmpAmf0ObjectEOF* value)
    {
        //srs_assert(value != NULL);
        return value->write(data, len);
    }
    
    int rtmp_amf0_write_any(uint8_t *data, int len, RtmpAmf0Any* value)
    {
        //srs_assert(value != NULL);
        return value->write(data, len);
    }
}

