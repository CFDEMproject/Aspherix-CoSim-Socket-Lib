#ifndef _WIN32

#ifndef ASPHERIX_COSIM_DATA_H
#define ASPHERIX_COSIM_DATA_H

#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <typeinfo>
#include <vector>

//static constexpr size_t NBYTES_INT = sizeof(int);
//static constexpr size_t NBYTES_SCALAR = sizeof(double);

#ifndef ASPHERIX_COSIM_ENUM_H
#define ASPHERIX_COSIM_ENUM_H

enum class DataType
{
    kNone,
    kBool,
    kInteger,
    kDouble
};

enum class DataObject
{
    Particle,
    Multisphere,    // MS / concave
    PointCloud,     // similar to MS?
    Boundary,
    Global
};

enum class CommStyle
{
    Pull = 0,   // receive
    Push = 1    // send
};

#endif

typedef std::map<DataType, std::string> DataTypeMap;
typedef std::map<DataObject, std::string> DataObjectMap;

//typedef std::map<DataType, const char*> DataTypeMap2;

/*
constexpr std::map<DataType, const char*> data_type_map =
    {
        { DataType::Integer, "integer" },
        { DataType::Scalar, "scalar" }
    };
*/

class CoSimField
{

public:
    CoSimField(const std::string &name, const DataType &type = DataType::kDouble,
               const size_t data_length = 1, const DataObject &object = DataObject::Particle,
               const CommStyle &comm = CommStyle::Pull);

    // legacy CoSimField constructor
    CoSimField(const std::string &name, const std::string &type, const DataObject &object, const bool pull);

    CoSimField(const size_t size, const char* byte_array)
    {
        fromByteVector(size, byte_array);
        setTypeString();
    }

    size_t length() const
    {
        return (name_.length() + 1) * sizeof(char) +
               sizeof(type_) + sizeof(data_length_) + sizeof(object_) + sizeof(comm_) + sizeof(offset_) + sizeof(index_);
    }

    // Helper function to write bytes into the byte array
    template<typename T>
    int writeBytes(char* arr, int pos, const T& value) const
    {
        for (auto i = 0; i < data_length_; ++i)
            
        memcpy(arr + pos, &value, sizeof(value));
        return sizeof(value);
    }
/*
    std::string readBytes(const char *arr, size_t num_bytes = -1) const
    {
        if (num_bytes == -1)
        {
            std::string result;
            for (int i = 0; charArray[i] != '\0'; ++i)
            {
                result += charArray[i];
            }
            return result;
        }
        else
            
    }
*/
    std::vector<char> toByteVector() const
    {
        std::vector<char> result;
        result.reserve(length());

        result.reserve( length() );

        auto bytes = name_.c_str();
        std::copy(bytes, bytes + sizeof(char)*(name_.size()+1), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &type_ );
        std::copy(bytes, bytes + sizeof(type_), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &data_length_ );
        std::copy(bytes, bytes + sizeof(data_length_), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &object_ );
        std::copy(bytes, bytes + sizeof(object_), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &comm_ );
        std::copy(bytes, bytes + sizeof(comm_), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &offset_ );
        std::copy(bytes, bytes + sizeof(offset_), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &index_ );
        std::copy(bytes, bytes + sizeof(index_), std::back_inserter(result) );

        assert(result.size() == length());

        return result;
    }
    /*
        std::copy(name.c_str())
        
        char *res = new char[length()];
        int pos = 0;
        pos += writeBytes(res, pos, name.c_str());     //string version should append zero char after string
        pos += writeBytes(res, pos, type);
        pos += writeBytes(res, pos, data_length);
        pos += writeBytes(res, pos, object);
        pos += writeBytes(res, pos, comm);
        return res;
    }*/

    void fromByteVector(const size_t size, const char* byte_array)
    {
        size_t offset = 0;

        for (int i = 0; byte_array[i] != '\0'; ++i)
            name_ += byte_array[i];
        name_[ name_.size()+1 ] = '\0';
        offset += name_.size()+1;

        std::vector<char> temp( &byte_array[offset], &byte_array[offset + sizeof( DataType )] );
        type_ = *reinterpret_cast<DataType*>(temp.data());
        offset += sizeof( DataType );

        temp = { &byte_array[offset], &byte_array[offset + sizeof( size_t )] };
        data_length_ = *reinterpret_cast<size_t*>(temp.data());
        offset += sizeof( size_t );

        temp = { &byte_array[offset], &byte_array[offset + sizeof( DataObject )] };
        object_ = *reinterpret_cast<DataObject*>(temp.data());
        offset += sizeof( DataObject );

        temp = { &byte_array[offset], &byte_array[offset + sizeof( CommStyle )] };
        comm_ = *reinterpret_cast<CommStyle*>(temp.data());
        offset += sizeof( CommStyle );

        if (comm_ == CommStyle::Pull)
            comm_ = CommStyle::Push;
        else if (comm_ == CommStyle::Push)
            comm_ = CommStyle::Pull;

        temp = { &byte_array[offset], &byte_array[offset + sizeof( size_t )] };
        offset_ = *reinterpret_cast<size_t*>(temp.data());
        offset += sizeof( size_t );

        temp = { &byte_array[offset], &byte_array[offset + sizeof( int )] };
        index_ = *reinterpret_cast<int*>(temp.data());
        offset += sizeof( int );

        assert(offset == size);
    }

    size_t dataTypeSize() const
    {
        switch (type_)
        {
            case DataType::kInteger:
                return data_length_ * sizeof(int);
            case DataType::kDouble:
                return data_length_ * sizeof(double);
            case DataType::kBool:
                return data_length_ * sizeof(bool);
            case DataType::kNone:
            default:
                return 0;
        }
    }

    bool isCommStylePush() const
    { return comm_ == CommStyle::Push; }

    bool isCommStylePull() const
    { return comm_ == CommStyle::Pull; }

    void setOffset(const size_t offset)
    {
        offset_ = offset;
    }

    void setIndex(const int index)
    {
        index_ = index;
    }

static std::string toString(const DataType value)
{
    static DataTypeMap to_string;
    to_string[DataType::kInteger] = "integer";
    to_string[DataType::kDouble] = "double";
    to_string[DataType::kNone] = "none";
    to_string[DataType::kBool] = "bool";
    return to_string.at(value);
}

static std::string toString(const DataObject value)
{
    static DataObjectMap to_string;
    to_string[DataObject::Particle]    = "Particle";
    to_string[DataObject::Multisphere] = "Multisphere";
    to_string[DataObject::PointCloud]  = "PointCloud";
    to_string[DataObject::Boundary]    = "Boundary";
    return to_string.at(value);
}

auto name() const
{ return name_; };

auto type() const
{ return type_; };

auto data_length() const
{ return data_length_; };

auto comm() const
{ return comm_; };

auto index() const
{ return index_; };

auto offset() const
{ return offset_; };

auto object() const
{ return object_; };

auto info() const
{
    const std::string comm_style = comm() == CommStyle::Push ? "push" : "pull";
    return std::string("name : ") + name() + " [ " + toString(type()) + " " + toString(object())
                     + " data of length " + std::to_string(data_length()) + " -- " + comm_style + "]";
}

void setTypeString();

std::string getTypeString() const
{ return type_string_; }

private:
    std::string name_;
    DataType type_;
    std::string type_string_;
    size_t data_length_;
    DataObject object_;
    CommStyle comm_;
    size_t offset_;
    int index_;
};

#endif
#endif
