#ifndef _WIN32

#pragma once

// #ifndef ASPHERIX_COSIM_FIELD_H
// #define ASPHERIX_COSIM_FIELD_H

#include "aspherix_cosim_interface.h"

#include <cassert>
#include <cstring>
#include <map>
#if __cplusplus >= 202002L
#include <span>
#endif
#include <string>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

namespace CoSimSocket
{

using DataTypeMap = std::map<DataType, std::string>;
using DataObjectMap = std::map<DataObject, std::string>;

class CoSimField : public CoSimInterface {

public:
    enum class DataMode : std::uint8_t
    {
        kSum,
        kAverage
    };

    CoSimField(std::shared_ptr<AspherixCoSimSocket> socket = nullptr) :
        CoSimInterface(std::move(socket)),
        type_(DataType::kDouble),
        data_length_(0),
        object_(DataObject::kUndefined),
        direction_(SyncDirection::kUndefined),
        offset_(0),
        index_(-1),
        ptr_(nullptr) {};

    // legacy CoSimField constructor
    CoSimField(std::string name, std::string type, bool client_to_server);
    CoSimField(const std::size_t size, const char* byte_array)
    {
        const std::vector<char> data(byte_array, byte_array + size);
        fromByteVector(data);
        setTypeString();
    }

    CoSimField(const CoSimField&) = default;
    CoSimField(CoSimField&&) = default;
    ~CoSimField() = default;
    CoSimField& operator=(const CoSimField&) = default;
    CoSimField& operator=(CoSimField&&) = default;
    CoSimField(std::string name, const DataType& type = DataType::kDouble,
               std::size_t data_length = 1, const DataObject& object = DataObject::kUndefined,
               const SyncDirection& direction = SyncDirection::kUndefined);

    bool operator==(const CoSimField& other) const
    {
        return length() == other.length() && name_ == other.name_ && type_ == other.type_
               && data_length_ == other.data_length_ && object_ == other.object_
               && direction_ == other.direction_;
    }

    [[nodiscard]] std::size_t length(
        const SyncDirection& direction = SyncDirection::kUndefined) const override
    {
        return (name_.length() + 1) * sizeof(char) + sizeof(type_) + sizeof(data_length_)
               + sizeof(object_) + sizeof(direction_) + sizeof(offset_) + sizeof(index_);
    }

    [[nodiscard]] std::vector<char> toByteVector(
        const SyncDirection& direction = SyncDirection::kUndefined) const override
    {
        std::vector<char> result;
        result.reserve(length());

        const auto* bytes = name_.c_str();
        std::copy(bytes, bytes + (sizeof(char) * (name_.size() + 1)), std::back_inserter(result));

        bytes = reinterpret_cast<const char*>(&type_);
        std::copy(bytes, bytes + sizeof(type_), std::back_inserter(result));

        bytes = reinterpret_cast<const char*>(&data_length_);
        std::copy(bytes, bytes + sizeof(data_length_), std::back_inserter(result));

        bytes = reinterpret_cast<const char*>(&object_);
        std::copy(bytes, bytes + sizeof(object_), std::back_inserter(result));

        bytes = reinterpret_cast<const char*>(&direction_);
        std::copy(bytes, bytes + sizeof(direction_), std::back_inserter(result));

        bytes = reinterpret_cast<const char*>(&offset_);
        std::copy(bytes, bytes + sizeof(offset_), std::back_inserter(result));

        bytes = reinterpret_cast<const char*>(&index_);
        std::copy(bytes, bytes + sizeof(index_), std::back_inserter(result));

        assert(result.size() == length());

        return result;
    }

    void fromByteVector(const std::vector<char>& byte_array, const std::size_t offset = 0,
                        const SyncDirection& direction = SyncDirection::kUndefined) override
    {
        std::size_t offset_current = offset;

        for (int i = 0; byte_array[offset_current + i] != '\0'; ++i)
        {
            name_ += byte_array[offset_current + i];
        }
        name_[name_.size() + 1] = '\0';
        offset_current += name_.size() + 1;

        std::vector<char> temp(&byte_array[offset_current],
                               &byte_array[offset_current + sizeof(DataType)]);
        type_ = *reinterpret_cast<DataType*>(temp.data());
        offset_current += sizeof(DataType);

        temp = {&byte_array[offset_current], &byte_array[offset_current + sizeof(std::size_t)]};
        data_length_ = *reinterpret_cast<std::size_t*>(temp.data());
        offset_current += sizeof(std::size_t);

        temp = {&byte_array[offset_current], &byte_array[offset_current + sizeof(DataObject)]};
        object_ = *reinterpret_cast<DataObject*>(temp.data());
        offset_current += sizeof(DataObject);

        temp = {&byte_array[offset_current], &byte_array[offset_current + sizeof(SyncDirection)]};
        // direction_ = *reinterpret_cast<SyncDirection*>(temp.data());
        std::memcpy(&direction_, temp.data(), sizeof(SyncDirection));
        offset_current += sizeof(SyncDirection);

        temp = {&byte_array[offset_current], &byte_array[offset_current + sizeof(std::size_t)]};
        offset_ = *reinterpret_cast<std::size_t*>(temp.data());
        offset_current += sizeof(std::size_t);

        temp = {&byte_array[offset_current], &byte_array[offset_current + sizeof(int)]};
        index_ = *reinterpret_cast<int*>(temp.data());
        offset_current += sizeof(int);

        setTypeString();

        assert((offset_current - offset) == length());
    }

    [[nodiscard]] std::size_t dataTypeSize() const
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

    [[nodiscard]] bool isServerToClient() const
    {
        return direction_ == SyncDirection::kServerToClient;
    }

    [[nodiscard]] bool isClientToServer() const
    {
        return direction_ == SyncDirection::kClientToServer;
    }

    void setOffset(const std::size_t offset) { offset_ = offset; }

    void setIndex(const int index) { index_ = index; }

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
        to_string[DataObject::kParticle] = "particle";
        to_string[DataObject::kMultisphere] = "multisphere";
        to_string[DataObject::kPointCloud] = "pointCloud";
        to_string[DataObject::kBoundary] = "boundary";
        to_string[DataObject::kGlobal] = "global";
        to_string[DataObject::kUndefined] = "undefined";
        return to_string.at(value);
    }

    [[nodiscard]] auto name() const { return name_; };

    [[nodiscard]] auto type() const { return type_; };

    [[nodiscard]] auto data_length() const { return data_length_; };

    [[nodiscard]] auto direction() const { return direction_; };

    [[nodiscard]] auto index() const { return index_; };

    [[nodiscard]] auto offset() const { return offset_; };

    [[nodiscard]] auto object() const { return object_; };

    [[nodiscard]] auto info() const
    {
        const std::string sync_direction = direction() == SyncDirection::kServerToClient
                                               ? "server to client"
                                               : "client to server";
        return std::string("name : ") + name() + " [ " + toString(type()) + " " + toString(object())
               + " data of length " + std::to_string(data_length()) + " -- " + sync_direction + "]";
    }

    [[nodiscard]] bool isParticleData() const { return object_ == DataObject::kParticle; }
    [[nodiscard]] bool isMultisphereData() const { return object_ == DataObject::kMultisphere; }
    [[nodiscard]] bool isPointcloudData() const { return object_ == DataObject::kPointCloud; }
    [[nodiscard]] bool isBoundaryData() const { return object_ == DataObject::kBoundary; }
    [[nodiscard]] bool isGlobalData() const { return object_ == DataObject::kGlobal; }

    [[nodiscard]] bool isScalarData() const { return data_length_ == 1; }

    [[nodiscard]] bool isArrayData() const { return data_length_ > 1; }

    void setTypeString();

    [[nodiscard]] std::string getTypeString() const { return type_string_; }

    void setPtr(void* const ptr) { ptr_ = ptr; }
    [[nodiscard]] void* getPtr() const { return ptr_; }

    [[nodiscard]] char* getPtr(const int index) const
    {
        return static_cast<char*>(ptr_ + index * dataTypeSize());
    }

#if __cplusplus >= 202002L
    std::size_t processValue(const int index, std::span<char> property_data)
    {
        auto* data = static_cast<char*>(ptr_ + index * dataTypeSize());
        memcpy(data, property_data.data(), dataTypeSize());
        return dataTypeSize();
    }
#endif

private:
    std::string name_;
    DataType type_;
    std::string type_string_;
    std::size_t data_length_;
    DataObject object_;
    SyncDirection direction_;
    std::size_t offset_;
    int index_;
    void* ptr_;

}; // class CoSimField

} // namespace CoSimSocket

#endif
// #endif
