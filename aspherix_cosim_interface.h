#pragma once

#include "aspherix_cosim_socket.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#if __cplusplus >= 201703L
#include <cstddef>
#endif
#if __cplusplus >= 202002L
#include <span>
#endif
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if __cplusplus < 201703L
// std::void_t is only available in C++17 and later
namespace std
{
template <typename...> using void_t = void;

enum class byte : std::uint8_t
{
};
} // namespace std
#endif

namespace CoSimSocket
{

enum class DataType : std::uint8_t
{
    kNone,
    kBool,
    kInteger,
    kDouble
};

enum class DataObject : std::uint8_t
{
    kUndefined,
    kParticle,
    kMultisphere, // MS / concave
    kPointCloud,
    kBoundary,
    kGlobal
};

// Trait to check if T has a method named `interfaceMethod`
template <typename T, typename = void> struct HasSerializeMethod : std::false_type
{
};

template <typename T>
struct HasSerializeMethod<T, std::void_t<decltype(std::declval<T>().toByteVector())>>
    : std::true_type
{
};

template <typename T> constexpr bool kHasSerializeMethod = HasSerializeMethod<T>::value;

// Trait to detect if a type is a std::vector
template <typename T> struct IsStdVector : std::false_type
{
};

template <typename T, typename Alloc> struct IsStdVector<std::vector<T, Alloc>> : std::true_type
{
};

// Trait to check if T is std::array<double, N> for any size N
template <typename T> struct IsStdArrayDouble : std::false_type
{
};

template <std::size_t N> struct IsStdArrayDouble<std::array<double, N>> : std::true_type
{
};

class CoSimInterface {
public:
    CoSimInterface(std::shared_ptr<AspherixCoSimSocket> socket) :
        socket_(std::move(socket))
    {
    }

    CoSimInterface() = default;
    CoSimInterface(const CoSimInterface&) = default;
    CoSimInterface(CoSimInterface&&) = default;
    CoSimInterface& operator=(const CoSimInterface&) = default;
    CoSimInterface& operator=(CoSimInterface&&) = default;
    virtual ~CoSimInterface() = default;

    void setSocket(std::shared_ptr<AspherixCoSimSocket> socket) { socket_ = std::move(socket); }

    [[nodiscard]] virtual std::size_t length(
        const SyncDirection& direction = SyncDirection::kUndefined) const = 0;
    virtual void fromByteVector(const std::vector<char>& byte_array, std::size_t offset,
                                const SyncDirection& direction = SyncDirection::kUndefined) = 0;
    [[nodiscard]] virtual std::vector<char> toByteVector(
        const SyncDirection& direction = SyncDirection::kUndefined) const = 0;

    void sync()
    {
        if (socket_ == nullptr)
        {
            std::cout << "There is no socket assigned.\n";
        }

        socket_->exchangeStatus(SocketCodes::kStartExchange, SocketCodes::kStartExchange);

        if (socket_->isServer())
        {
            // send data
            const auto data_to_send = toByteVector(SyncDirection::kServerToClient);
            socket_->writeData(data_to_send);

            // receive data
            const auto data_to_recv = socket_->readData<char>();
            fromByteVector(data_to_recv, 0, SyncDirection::kClientToServer);
        }
        else
        {
            // send data
            const auto data_to_send = toByteVector(SyncDirection::kClientToServer);
            socket_->writeData(data_to_send);

            // receive data
            const auto data_to_recv = socket_->readData<char>();
            fromByteVector(data_to_recv, 0, SyncDirection::kServerToClient);
        }

        socket_->exchangeStatus(SocketCodes::kStopExchange, SocketCodes::kStopExchange);
    }

protected:
    static std::size_t stringSize(const std::string& string)
    {
        return sizeof(std::size_t) + string.size() + 1;
    }

    template <typename T>
    T extract(const std::vector<char>& byte_array_with_offset, std::size_t& offset);

    template <typename T> void insert(const T& value, std::vector<char>& result) const;

private:
    std::shared_ptr<AspherixCoSimSocket> socket_;
};

template <typename T>
auto CoSimInterface::extract(const std::vector<char>& byte_array_with_offset, std::size_t& offset)
    -> T
{
    if constexpr (kHasSerializeMethod<T>)
    {
        T temp;
        temp.fromByteVector(byte_array_with_offset, offset);
        offset += temp.length();
        return temp;
    }
    else
    {
        if (offset + sizeof(T) > byte_array_with_offset.size())
        {
            throw std::out_of_range("Not enough bytes to extract the object.");
        }

        T value;
        // Safely copy the bytes into the object
        std::memcpy(&value, &byte_array_with_offset[offset], sizeof(T));
        offset += sizeof(T);
        return value;

        // std::vector<char> temp(&byte_array_with_offset[offset],
        //                        &byte_array_with_offset[offset + sizeof(T)]);
        // offset += sizeof(T);
        // return *reinterpret_cast<T*>(temp.data());
    }
}

template <>
inline auto CoSimInterface::extract<std::string>(const std::vector<char>& byte_array_with_offset,
                                                 std::size_t& offset) -> std::string
{
    // std::vector<char> temp(&byte_array_with_offset[offset],
    //                        &byte_array_with_offset[offset + sizeof(std::size_t)]);
    // const auto str_length = *reinterpret_cast<std::size_t*>(temp.data());
    // std::vector<char>
    //     string(&byte_array_with_offset[offset + sizeof(std::size_t)],
    //            &byte_array_with_offset[offset + sizeof(std::size_t) + str_length + 1]);
    // offset += sizeof(std::size_t) + str_length + 1;
    // return {string.data()};
    std::size_t str_length = 0;
    std::memcpy(&str_length, &byte_array_with_offset[offset], sizeof(std::size_t));
    offset += sizeof(std::size_t);

    // Extract the string data, including the null terminator
    std::string result(&byte_array_with_offset[offset], str_length);
    offset += str_length + 1; // +1 for the null terminator

    return result;
}

template <typename T> void CoSimInterface::insert(const T& value, std::vector<char>& result) const
{
    if constexpr (kHasSerializeMethod<T>)
    {
        auto result2 = value.toByteVector();
        const auto* bytes_len = result2.data(); // reinterpret_cast<const char*>(result2.data());
        std::copy(bytes_len, bytes_len + result2.size(), std::back_inserter(result));
    }
    else
    {
#if __cplusplus >= 202002L
        auto bytes = std::as_bytes(std::span{&value, 1}); // std::as_bytes requires C++20
#else
        std::vector<std::byte> bytes(sizeof(T));
        std::memcpy(bytes.data(), &value, sizeof(T));
#endif
        // std::copy(bytes.begin(), bytes.end(), std::back_inserter(result));
        std::transform(bytes.begin(), bytes.end(), std::back_inserter(result),
                       [](std::byte byte) { return static_cast<char>(byte); });
        // const auto* bytes_len = reinterpret_cast<const char*>(&value);
        // std::copy(bytes_len, bytes_len + sizeof(T), std::back_inserter(result));
    }
}

template <>
inline void CoSimInterface::insert<std::string>(const std::string& value,
                                                std::vector<char>& result) const
{
    const std::size_t length = value.size();
    insert(length, result);

    result.insert(result.end(), value.begin(), value.end());
    result.push_back('\0'); // Append the null terminator if needed
    // const auto* bytes = value.c_str(); // c_str is '\0' terminated, so +1
    // std::copy(bytes, bytes + length + 1, std::back_inserter(result));
}
} // namespace CoSimSocket
