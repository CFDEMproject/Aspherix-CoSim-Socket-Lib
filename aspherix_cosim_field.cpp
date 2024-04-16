#ifndef _WIN32

#include "aspherix_cosim_field.h"

//typedef std::map<DataType, std::string> DataTypeMap;

/*
DataTypeMap DataTypeToString =
{
    { DataType::Integer, "Integer" },
    { DataType::Scalar , "Scalar"  }
};

//typedef std::map<DataObject, std::string> DataObjectMap;

DataObjectMap DataObjectToString =
{
    { DataObject::Particle   , "Particle"    },
    { DataObject::Multisphere, "Multisphere" },
    { DataObject::PointCloud , "PointCloud"  },
    { DataObject::Boundary   , "Boundary"    }
};
*/

CoSimField::CoSimField(const std::string &name_, const DataType &type,
           const size_t data_length, const DataObject &object_,
           const CommStyle &comm_) :
    offset(0),
    index(-1)
{};

/*
int CoSimField::length() const
{
    return name.length() * sizeof(char) + 1 +
           sizeof(type) + sizeof(data_length) + sizeof(object) + sizeof(comm);
}

// Helper function to write bytes into the byte array
template<typename T>
int writeBytes(char* arr, int pos, const T& value) const
{
    memcpy(arr + pos, &value, sizeof(value));
    return sizeof(value);
}

char* toByteArray() const
{
    char *res = new char[length()];
    int pos = 0;
    pos += writeBytes(res, pos, name.c_str());     //string version should append zero char after string
    pos += writeBytes(res, pos, type);
    pos += writeBytes(res, pos, data_length);
    pos += writeBytes(res, pos, object);
    pos += writeBytes(res, pos, comm);
    return res;
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

};
*/

#endif
