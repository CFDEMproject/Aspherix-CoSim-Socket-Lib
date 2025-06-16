// #include "aspherix_cosim_interface.h"
#ifndef _WIN32

#include "aspherix_cosim_field.h"

namespace CoSimSocket
{
CoSimField::CoSimField(std::string name, std::string container, const DataType& type,
                       const size_t data_length, const DataObject& object,
                       const SyncDirection& direction) :
    field_name_(std::move(name)),
    container_name_(std::move(container)),
    type_(type),
    data_length_(data_length),
    object_(object),
    direction_(direction),
    offset_(0),
    index_(-1),
    ptr_(nullptr)
{
    setTypeString();
};

CoSimField::CoSimField(std::string name, std::string type,
                       const bool server_to_client) :
    field_name_(std::move(name)),
    type_(DataType::kDouble),
    type_string_(std::move(type)),
    data_length_(1),
    object_(DataObject::kUndefined),
    direction_(),
    offset_(0),
    index_(-1),
    ptr_(nullptr)
{
    const auto npos = std::string::npos;
    if (type_string_.find("scalar-") != npos)
    {
        if (field_name_ == "body" || field_name_ == "id" || field_name_ == "type"
            || field_name_ == "shapetype" || // particle
            field_name_ == "nrigid" || field_name_ == "clumptype"
            || field_name_ == "id_multisphere") // MS
        {
            type_ = DataType::kInteger;
        }
        data_length_ = 1;
    }
    else if (type_string_.find("vector-") != npos)
    {
        data_length_ = 3;
    }
    else if (type_string_.find("vector2D-") != npos)
    {
        data_length_ = 2;
    }
    else if (type_string_.find("quaternion-") != npos)
    {
        data_length_ = 4;
    }

    if (type_string_.find("-atom") != npos)
    {
        container_name_ = "particles";
        object_ = DataObject::kParticle;
    }
    else if (type_string_.find("-multisphere") != npos)
    {
        container_name_ = "multisphere";
        object_ = DataObject::kMultisphere;
    }
    else if (type_string_.find("-pointcloud") != npos)
    {
        container_name_ = "pointcloud";
        object_ = DataObject::kPointCloud;
    }
    else if (type_string_.find("-boundary") != npos)
    {
        container_name_ = "boundary";
        object_ = DataObject::kBoundary;
    }

    direction_ = server_to_client ? SyncDirection::kServerToClient : SyncDirection::kClientToServer;
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
    case DataObject::kParticle:
        type_string_ += "atom";
        break;
    case DataObject::kMultisphere:
        type_string_ += "multisphere";
        break;
    case DataObject::kPointCloud:
        type_string_ += "pointcloud";
        break;
    case DataObject::kGlobal:
        type_string_ += "global";
        break;
    case DataObject::kBoundary:
        type_string_ += "boundary";
        break;
    case DataObject::kUndefined:
    default:
        type_string_ += "undefined";
        break;
    }
}

} // namespace CoSimSocket

#endif
