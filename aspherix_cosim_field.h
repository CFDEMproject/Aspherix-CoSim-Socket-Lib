#ifndef _WIN32

#ifndef ASPHERIX_COSIM_DATA_H
#define ASPHERIX_COSIM_DATA_H

#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <typeinfo>
#include <vector>

static constexpr size_t NBYTES_INT = 4; //sizeof(int);
static constexpr size_t NBYTES_SCALAR = 8; //sizeof(double);

#ifndef ASPHERIX_COSIM_ENUM_H
#define ASPHERIX_COSIM_ENUM_H

enum class DataType
{
    Integer,
    Scalar
};

enum class DataObject
{
    Particle,
    Multisphere,    // MS / concave
    PointCloud,     // similar to MS?
    Boundary
};

enum class CommStyle
{
    Pull = 0,
    Push = 1
};

#endif

typedef std::map<DataType, std::string> DataTypeMap;
typedef std::map<DataObject, std::string> DataObjectMap;

typedef std::map<DataType, const char*> DataTypeMap2;

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
    CoSimField(const std::string &name_, const DataType &type = DataType::Scalar,
               const size_t data_length = 1, const DataObject &object_ = DataObject::Particle,
               const CommStyle &comm_ = CommStyle::Pull);

    CoSimField(const std::vector<uint8_t> &byte_vector)
    {
        fromByteVector(byte_vector);
    }

    int length() const
    {
        return (name.length() + 1) * sizeof(char) +
               sizeof(type) + sizeof(data_length) + sizeof(object) + sizeof(comm);
    }

    // Helper function to write bytes into the byte array
    template<typename T>
    int writeBytes(char* arr, int pos, const T& value) const
    {
        for (auto i = 0; i < data_length; ++i)
            
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
    std::vector<uint8_t> toByteVector() const
    {
        std::vector<uint8_t> result;
        result.reserve(length());

        result.reserve( length() );

        auto bytes = name.c_str();
        std::copy(bytes, bytes + sizeof(char)*(name.size()+1), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &type );
        std::copy(bytes, bytes + sizeof( typeid(type) ), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &data_length );
        std::copy(bytes, bytes + sizeof( typeid(data_length) ), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &object );
        std::copy(bytes, bytes + sizeof( typeid(object) ), std::back_inserter(result) );

        bytes = reinterpret_cast<const char*>( &comm );
        std::copy(bytes, bytes + sizeof( typeid(comm) ), std::back_inserter(result) );

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

    void fromByteVector(const std::vector<uint8_t> &byte_vector)
    {
        int offset = 0;

        for (int i = 0; byte_vector[i] != '\0'; ++i)
            name += byte_vector[i];
        name[ name.size()+1 ] = '\0';
        offset += name.size()+1;

        std::vector<uint8_t> temp( &byte_vector[offset], &byte_vector[offset + sizeof( DataType )] );
        type = *reinterpret_cast<DataType*>(temp.data());
        offset += sizeof( DataType );

        temp = { &byte_vector[offset], &byte_vector[offset + sizeof( size_t )] };
        data_length = *reinterpret_cast<size_t*>(temp.data());
        offset += sizeof( size_t );

        temp = { &byte_vector[offset], &byte_vector[offset + sizeof( DataObject )] };
        data_length = *reinterpret_cast<size_t*>(temp.data());
        object = *reinterpret_cast<DataObject*>(temp.data());
        offset += sizeof( DataObject );

        temp = { &byte_vector[offset], &byte_vector[offset + sizeof( CommStyle )] };
        comm = *reinterpret_cast<CommStyle*>(temp.data());
        offset += sizeof( CommStyle );

        assert(offset == byte_vector.size());
    }

    size_t dataTypeSize() const
    {
        switch (type)
        {
            case DataType::Integer:
                return NBYTES_INT * data_length;
            case DataType::Scalar:
                return NBYTES_SCALAR * data_length;
            default:
                return 0;
        }
    }

    void setOffset(const size_t offset_in)
    {
        offset = offset_in;
    }

    void setIndex(const int index_in)
    {
        index = index_in;
    }

static std::string toString(const DataType value)
{
    static DataTypeMap to_string;
    to_string[DataType::Integer] = "integer";
    to_string[DataType::Scalar] = "scalar";
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

    std::string name;
    DataType type;
    size_t data_length;
    DataObject object;
    CommStyle comm;
    size_t offset;
    int index;
};

#endif
#endif
