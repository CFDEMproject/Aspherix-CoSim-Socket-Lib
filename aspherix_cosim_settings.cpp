#include "aspherix_cosim_interface.h"
#ifndef _WIN32

#include "aspherix_cosim_settings.h"

#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <type_traits>
#include <variant>
#include <vector>

namespace CoSimSocket
{

using SettingValueType =
    std::variant<bool, int, std::size_t, double, std::string, std::vector<double>, std::vector<int>,
                 std::array<double, 3>, std::array<double, 6>, std::vector<std::string>,
                 std::vector<CoSimField>>; //, BoundaryData, std::vector<BoundaryData>>;

struct SettingInfo
{
    std::string name;
    SettingValueType value;
    SyncDirection direction;
};

class CoSimSettingsImpl : public CoSimInterface {

public:
    CoSimSettingsImpl(std::shared_ptr<AspherixCoSimSocket> socket) :
        CoSimInterface(socket),
        items_()
    // internal_data_("empty (default)") {} // Initialize variant
    {
    }

    bool operator==(const CoSimSettingsImpl& other) const
    {
        if (items_.size() != other.items_.size())
        {
            return false;
        }

        auto item_comparator = [](const auto& item_a, const auto& item_b) {
            return item_a.name == item_b.name && item_a.value == item_b.value;
        };

        return std::equal(items_.begin(), items_.end(), other.items_.begin(), item_comparator);
    }

    void clearSettings() { items_.clear(); }

    template <typename T>
    void addSetting(const std::string& name, T value,
                    SyncDirection direction = SyncDirection::kUndefined)
    {
        items_.push_back({name, value, direction});
    }

    template <typename T> void setSetting(const std::string& name, T new_value)
    {
        bool found = false;
        for (auto& item : items_)
        {
            if (item.name == name)
            {
                std::visit(
                    [&found](auto&& value) {
                        using Tvar = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, Tvar>)
                        {
                            found = true;
                        }
                    },
                    item.value);
                if (found)
                {
                    item.value = new_value;
                    return;
                }
            }
        }
        std::cout << "There is no setting '" + name + "' registered.\n";
    }

    template <typename T> auto getSetting(const std::string& name) const -> T
    {
        T result;
        bool found = false;
        std::string found_type;
        for (const auto& item : items_)
        {
            if (item.name == name)
            {
                std::visit(
                    [&found, &result, &found_type](auto&& value) {
                        using Tvar = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, Tvar>)
                        {
                            found = true;
                            result = value;
                        }
                        else
                        {
                            found_type = typeid(Tvar).name();
                        }
                    },
                    item.value);
                if (found)
                {
                    return result;
                }
            }
        }
        std::cout << "There is no setting '" + name + "' registered.\n";
        return result;
    }

    std::size_t length(const SyncDirection& direction = SyncDirection::kUndefined) const override
    {
        std::size_t length = 0;

        for (const auto& item : items_)
        {
            if (item.direction == direction || direction == SyncDirection::kUndefined)
            {
                std::visit(
                    [&length](auto&& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (IsStdArrayDouble<T>::value)
                        {
                            length += value.size() * sizeof(double);
                        }
                        else if constexpr (IsStdVector<T>::value)
                        {
                            using D = typename T::value_type;
                            const std::size_t vector_length = value.size();

                            std::size_t total_length = sizeof(vector_length);
                            if constexpr (std::is_same_v<D, std::string>)
                            {
                                // Compute the total length of all strings in the vector
                                // c_str is '\0' terminated, so +1
                                total_length += std::accumulate(value.begin(), value.end(), 0U,
                                                                [](size_t sum,
                                                                   const std::string& str) {
                                                                    return sum + stringSize(str);
                                                                });
                            }
                            else if constexpr (std::is_same_v<D, CoSimField>)
                            {
                                total_length += std::accumulate(value.begin(), value.end(), 0U,
                                                                [](size_t sum, const D& item) {
                                                                    return sum + item.length();
                                                                });
                            }
                            else
                            {
                                total_length += vector_length * sizeof(D);
                            }

                            length += total_length;
                        }
                        else if constexpr (std::is_same_v<T, std::string>)
                        {
                            length += stringSize(value);
                        }
                        else
                        {
                            length += sizeof(T);
                        }
                    },
                    item.value);
            }
        }
        return length;
    }

    void fromByteVector(const std::vector<char>& byte_array, std::size_t offset = 0,
                        const SyncDirection& direction = SyncDirection::kUndefined) override
    {
        for (auto& item : items_)
        {
            if (item.direction == direction || direction == SyncDirection::kUndefined)
            {
                std::visit(
                    [&offset, byte_array, this](auto&& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (IsStdArrayDouble<T>::value)
                        {
                            for (auto& array_value : value)
                            {
                                array_value = extract<double>(byte_array, offset);
                            }
                        }
                        else if constexpr (IsStdVector<T>::value)
                        {
                            using D = typename T::value_type;
                            const auto vector_length = extract<std::size_t>(byte_array, offset);
                            value.reserve(vector_length);
                            value.clear();
                            for (std::size_t i = 0; i < vector_length; ++i)
                            {
                                const D item = extract<D>(byte_array, offset);
                                value.push_back(item);
                            }
                        }
                        else if constexpr (std::is_same_v<T, std::string>)
                        {
                            value = extract<std::string>(byte_array, offset);
                        }
                        else if constexpr (HasSerializeMethod<T>::value)
                        {
                            value.fromByteVector(&byte_array, offset);
                            offset += value.length();
                        }
                        else
                        {
                            value = extract<T>(byte_array, offset);
                        }
                    },
                    item.value);
            }
        }
        assert(offset == byte_array.size());
    }

    std::vector<char> toByteVector(const SyncDirection& direction = SyncDirection::kUndefined) const override
    {
        std::vector<char> result;
        result.reserve(length(direction));

        for (const auto& item : items_)
        {
            if (item.direction == direction || direction == SyncDirection::kUndefined)
            {
                std::visit(
                    [&result, this](auto&& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (IsStdArrayDouble<T>::value)
                        {
                            for (const auto& array_value : value)
                            {
                                const auto* bytes = reinterpret_cast<const char*>(&array_value);
                                std::copy(bytes, bytes + sizeof(double),
                                          std::back_inserter(result));
                            }
                        }
                        else if constexpr (std::is_same_v<T, std::string>)
                        {
                            const std::size_t str_length = value.size();
                            const auto* bytes_len = reinterpret_cast<const char*>(&str_length);
                            std::copy(bytes_len, bytes_len + sizeof(std::size_t),
                                      std::back_inserter(result));
                            const auto* bytes = value.c_str(); // c_str is '\0' terminated, so +1
                            std::copy(bytes, bytes + str_length + 1, std::back_inserter(result));
                        }
                        else if constexpr (IsStdVector<T>::value)
                        {
                            // using D = typename IsStdVector<T>::value_type;
                            std::size_t vector_length = value.size();
                            insert(vector_length, result);

                            for (const auto& item : value)
                            {
                                insert(item, result);
                            }
                        }
                        else if constexpr (HasSerializeMethod<T>::value)
                        {
                            auto data = value.toByteVector();
                            const auto* bytes = reinterpret_cast<const char*>(&data);
                            std::copy(bytes, bytes + sizeof(data), std::back_inserter(data));
                        }
                        else
                        {
                            const auto* bytes = reinterpret_cast<const char*>(&value);
                            std::copy(bytes, bytes + sizeof(T), std::back_inserter(result));
                        }
                    },
                    item.value);
            }
        }

        // auto length_direction = length(direction);
        assert(result.size() == length(direction));
        return result;
    }

    void printInfo() const
    {
        std::cout << "The following properties have been defined:\n";
        std::string type = "undefined";
        std::string direction = "undefined";

        for (const auto& item : items_)
        {
            std::visit(
                [&type, &direction, &item, this](const auto& value) {
                    using T = std::decay_t<decltype(value)>;

                    if constexpr (std::is_same_v<T, int> || std::is_same_v<T, double>)
                    {
                        type = std::to_string(value);
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        type = value ? "true" : "false";
                    }
                    else if constexpr (std::is_same_v<T, std::string>)
                    {
                        type = value;
                    }
                    else if constexpr (IsStdVector<T>::value)
                    {
                        using D = typename T::value_type;
                        if constexpr (std::is_same_v<D, double> || std::is_same_v<D, int>)
                        {
                            type = printContainer(value);
                        }
                        else
                        {
                            type = "vector";
                        }
                    }
                    else if constexpr (IsStdArrayDouble<T>::value)
                    {
                        using D = typename T::value_type;
                        if constexpr (std::is_same_v<D, double> || std::is_same_v<D, int>)
                        {
                            type = printContainer(value);
                        }
                        else
                        {
                            type = "array";
                        }
                    }

                    if (item.direction == SyncDirection::kClientToServer)
                    {
                        direction = "client to server";
                    }
                    else if (item.direction == SyncDirection::kServerToClient)
                    {
                        direction = "server to client";
                    }
                },
                item.value);
            std::cout << "  " << item.name << " = " << type << " (" << direction << ")\n";
        }
        std::cout << "================================\n";
    }

private:
    template <typename Container> std::string printContainer(const Container& container) const
    {
        std::ostringstream oss;
        oss << "(";

        std::size_t index = 0; // Initialize an index to keep track of the position
        for (const auto& value : container)
        {
            // oss << "val[" << index << "] = " << value;
            oss << value;
            if (index < container.size() - 1)
            {
                oss << ", "; // Add comma except for the last element
            }
            ++index; // Increment the index
        }

        oss << ")";
        return oss.str();
    }

    std::vector<SettingInfo> items_;
};

CoSimSettings::CoSimSettings() {};
CoSimSettings::CoSimSettings(std::shared_ptr<AspherixCoSimSocket> socket) :
    CoSimInterface(socket),
    pimpl_(std::make_unique<CoSimSettingsImpl>(socket))
{
}
CoSimSettings::~CoSimSettings() = default;
// Move constructor
CoSimSettings::CoSimSettings(CoSimSettings&& other) noexcept :
    pimpl_(std::move(other.pimpl_))
{
}

// Move assignment
CoSimSettings& CoSimSettings::operator=(CoSimSettings&& other) noexcept
{
    if (this != &other)
    {
        CoSimInterface::operator=(std::move(other)); // move base
        pimpl_ = std::move(other.pimpl_);
    }
    return *this;
}

bool CoSimSettings::operator==(const CoSimSettings& other) const
{
    return *pimpl_ == *other.pimpl_;
}

void CoSimSettings::clearSettings()
{
    pimpl_->clearSettings();
}

template <typename T>
void CoSimSettings::addSetting(const std::string& name, T value, SyncDirection direction)
{
    pimpl_->addSetting(name, value, direction);
}

template <typename T> void CoSimSettings::setSetting(const std::string& name, T new_value)
{
    pimpl_->setSetting(name, new_value);
}

template <typename T> auto CoSimSettings::getSetting(const std::string& name) const -> T
{
    return pimpl_->getSetting<T>(name);
}

std::size_t CoSimSettings::length(const SyncDirection& direction) const
{
    return pimpl_->length(direction);
}

void CoSimSettings::fromByteVector(const std::vector<char>& byte_array, std::size_t offset,
                                   const SyncDirection& direction)
{
    pimpl_->fromByteVector(byte_array, offset, direction);
}

std::vector<char> CoSimSettings::toByteVector(const SyncDirection& direction) const
{
    return pimpl_->toByteVector(direction);
}

void CoSimSettings::printInfo() const
{
    pimpl_->printInfo();
}

// Explicit instantiations for the types the C++14 client is expected to use.
// This ensures these versions of the templated functions are compiled into the library.
// clang-format off
#define INSTANTIATE_SETTING(type) \
template void CoSimSettings::setSetting<type>(const std::string&, type); \
template auto CoSimSettings::getSetting<type>(const std::string&) const -> type; \
template void CoSimSettings::addSetting<type>(const std::string&, type, SyncDirection);

using Array3 = std::array<double,3>;
using Array6 = std::array<double,6>;
using CharPtr = char const*;

INSTANTIATE_SETTING(bool)
INSTANTIATE_SETTING(int)
INSTANTIATE_SETTING(std::size_t)
INSTANTIATE_SETTING(double)
INSTANTIATE_SETTING(std::string)
INSTANTIATE_SETTING(CharPtr)
INSTANTIATE_SETTING(std::vector<int>)
INSTANTIATE_SETTING(std::vector<double>)
INSTANTIATE_SETTING(Array3)
INSTANTIATE_SETTING(Array6)
INSTANTIATE_SETTING(std::vector<std::string>)
INSTANTIATE_SETTING(std::vector<CoSimField>)

// clang-format on

// template void CoSimSettings::setSetting<int>(const std::string &name,
//                                              int new_value);
// template void MyLibrary::set_data<std::string>(std::string data);
// template void MyLibrary::set_data<double>(double data);
// template void MyLibrary::set_data<float>(float data);
// If the client uses set_data with another type not listed here (e.g., char*),
// and MyLibraryImpl::store_data can handle it (e.g., by converting to
// std::string), you might need to add more explicit instantiations or ensure
// the linker can find it. For char*, std::string has a constructor, so
// internal_data_ = data would work if T is char* and internal_data_ can be
// assigned a std::string. Let's add one for const char* as it's common.
// template void MyLibrary::set_data<const char *>(const char *data);

// Implementation of MyLibrary's public non-template methods
// void MyLibrary::process_stored_data() { pimpl_->perform_processing(); }

} // namespace CoSimSocket

#endif
