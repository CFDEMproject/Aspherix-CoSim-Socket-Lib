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

CoSimField::CoSimField(const std::string &name, const DataType &type,
           const size_t data_length, const DataObject &object,
           const CommStyle &comm) :
    name_(name),
    type_(type),
    type_string_(""),
    data_length_(data_length),
    object_(object),
    comm_(comm),
    offset_(0),
    index_(-1)
{};

CoSimField::CoSimField(const std::string &name, const std::string &type, const DataObject &object, const bool pull):
    type_(DataType::kDouble),
    type_string_(type),
    data_length_(1),
    object_(DataObject::Particle),
    offset_(0),
    index_(-1)
{
    const auto npos = std::string::npos;
    if (type.find("scalar-") != npos)
    {
        if ( name == "body" || name == "id" || name == "type" || name == "shapetype" || // particle
             name == "nrigid" || name == "clumptype" || name == "id_multisphere" )      // MS
            type_ = DataType::kInteger;
        data_length_ = 1;
    }
    else if (type.find("vector-") != npos)
        data_length_ = 3;
    else if (type.find("vector2D-") != npos)
        data_length_ = 2;
    else if (type.find("quaternion-") != npos)
        data_length_ = 4;

    if (type.find("-atom") != npos)
        object_ = DataObject::Particle;
    else if (type.find("-multisphere") != npos)
        object_ = DataObject::Multisphere;
    else if (type.find("-pointcloud") != npos)
        object_ = DataObject::PointCloud;
    else
        object_ = DataObject::Boundary;
}

void CoSimField::setTypeString()
{
    switch (data_length_)
    {
        case 1:
            type_string_ = "scalar-";
            break;
        case 2:
            type_string_ = "vector2D-";
            break;
        case 3:
            type_string_ = "vector-";
            break;
        case 4:
            type_string_ = "quaternion-";
            break;
    }

    switch (object_)
    {
        case DataObject::Particle:
            type_string_ += "atom";
            break;
        case DataObject::Multisphere:
            type_string_ += "multisphere";
            break;
        case DataObject::PointCloud:
            type_string_ += "pointcloud";
            break;
    }
}

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
