/*---------------------------------------------------------------------------*\
    Aspherix-CoSimulation-Socket-Library

    (C) 2019 DCS Computing GmbH, Linz, Austria

    This software is released under the GNU GPL v3.

|*---------------------------------------------------------------------------*|

Description
    This code provides a protocol for CoSimulation data transfer.
    Note: this code is not part of OpenFOAM(R) (see DISCLAIMER).

Class
    AspherixCoSimSocket

SourceFiles
    aspherix_cosim_socket.cpp
\*---------------------------------------------------------------------------*/

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

// this is not available on Windows
#ifndef _WIN32

#ifndef ASPHERIX_COSIM_SOCKET_I_H
#define ASPHERIX_COSIM_SOCKET_I_H

// #include <cstdio>
// #include <cstring>
// #include <iostream>
// #include <string>
// #include <type_traits>
// #include <unistd.h>
// #include <vector>

// #include <arpa/inet.h>
// #include <netinet/in.h>
// #include <sys/socket.h>

// using AspherixCoSimSocket = CoSimSocket::AspherixCoSimSocket;

template <typename T> void AspherixCoSimSocket::read_socket(T* const value)
{
    read_socket(static_cast<void* const>(value), sizeof(T));
}

template <typename T> void AspherixCoSimSocket::write_socket(const T* const value)
{
    write_socket(static_cast<const void* const>(value), sizeof(T));
}

template <typename T> std::vector<T> AspherixCoSimSocket::readData()
{
    std::size_t vector_size = 0;
    read_socket(&vector_size, sizeof(std::size_t));
    std::vector<T> vector;
    if (vector_size > 0)
    {
        vector.resize(vector_size);
        read_socket(vector.data(), vector_size * sizeof(T) / sizeof(char));
    }
    return vector;
}

template <typename T> void AspherixCoSimSocket::writeData(const std::vector<T>& data)
{
    const std::size_t size = data.size();
    write_socket(&size, sizeof(std::size_t));
    if (size > 0)
    {
        write_socket(data.data(), size * sizeof(T) / sizeof(char));
    }
}

template <typename T> auto AspherixCoSimSocket::readValue(std::size_t size) -> T
{
    static constexpr bool needs_size = !std::is_trivially_copyable_v<T>;

    size_t recv_size = 0;
    int cur_size = 0;

    if constexpr (needs_size)
    {
        size = readValue<std::size_t>();
        using ElementType = std::remove_pointer_t<decltype(std::declval<T>().data())>;
        size = size * sizeof(ElementType);
        error("Only trivially copyable datatypes are supported for direct socket communication");
    }

    std::vector<char> buf;
    buf.reserve(size);
    buf.resize(size);

    const auto socket_file_descriptor = isServer() ? insockfd_ : sockfd_;

    while (recv_size < size)
    {
        cur_size = ::read(socket_file_descriptor, buf.data() + recv_size, size - recv_size);

        if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for reading data " << std::to_string(errno) << std::endl;
            else
                error(std::string(
                          "\n\nERROR: AspherixCoSimSocket::read_socket: Failed getting data. ")
                      + std::to_string(cur_size));
        }
        else if (cur_size == 0)
            error(std::string("\n\nERROR: AspherixCoSimSocket::read_socket: Disconnected. ")
                  + std::to_string(cur_size));

        recv_size += cur_size;
    }

    // if constexpr (needs_size)
    //     return buf;
    // else
    return *reinterpret_cast<T*>(buf.data());
}

template <typename T> int AspherixCoSimSocket::writeValue(const T& object)
{
    static constexpr bool needs_size = !std::is_trivially_copyable_v<T>;

    const char* buf = nullptr;
    std::size_t size;

    if constexpr (needs_size)
    {
        size = object.size();
        writeValue(size);
        using ElementType = std::remove_pointer_t<decltype(object.data())>;
        size = size * sizeof(ElementType);
        buf = reinterpret_cast<const char*>(object.data());
        error("Only trivially copyable datatypes are supported for direct socket communication");
    }
    else
    {
        size = sizeof(T);
        buf = reinterpret_cast<const char*>(&object);
    }

    auto send_size = 0;
    auto cur_size = 0;

    const auto socket_file_descriptor = isServer() ? insockfd_ : sockfd_;

    while (send_size < size)
    {

        //    if constexpr (needs_size)
        //        cur_size = ::write(socket_file_descriptor, static_cast<const char*>(object.data())
        //        + send_size, size - send_size);
        //    else if (std::is_enum<T>::value)
        cur_size = ::write(socket_file_descriptor, buf + send_size, size - send_size);
        //    else
        //        cur_size = ::write(socket_file_descriptor, static_cast<const char*>(object) +
        //        send_size, size - send_size);
        // cur_size = ::write(socket_file_descriptor, static_cast<const char*>(static_cast<typename
        // std::underlying_type<T>::type>(object)) + send_size, size - send_size);

        if (cur_size > 0)
            send_size += cur_size;
        else if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for sending data " << std::to_string(errno) << std::endl;
            else
                error("\n\nERROR: AspherixCoSimSocket::write_socket: Failed sending data.\n");
        }
        else if (cur_size == 0)
            error(std::string("\n\nERROR: AspherixCoSimSocket::write_socket: Disconnected. ")
                  + std::to_string(cur_size));
    }
    if (size != send_size)
        return 1;
    // throw error
    return 0;
}

template <typename T>
int AspherixCoSimSocket::exchangeValue(T& object, const CoSimSocket::SyncDirection direction,
                                       const std::size_t size)
{
    // static constexpr bool kNeedsSize = !std::is_trivially_copyable_v<T>;
    //
    // size_t obj_size;
    // if constexpr (kNeedsSize)
    //     obj_size = object.size();
    // else
    //     obj_size = sizeof(T);
    //
    // const char* buf = nullptr;
    // if constexpr (kNeedsSize)
    //     buf = reinterpret_cast<const char*>(object.data());
    // else // if (std::is_enum<T>::value)
    //     buf = reinterpret_cast<const char*>(&object);

    // auto send_size = 0;
    // auto cur_size  = 0;

    // const auto socket_file_descriptor = isServer() ? insockfd_ : sockfd_;

    if (direction == CoSimSocket::SyncDirection::kClientToServer && isClient()
        || direction == CoSimSocket::SyncDirection::kServerToClient && isServer())
    {
        writeValue(object);
    }
    else
    {
        object = readValue<T>();
    }
    return 0;
}

// Specialization for kSend (const T&)
template <typename T> struct AspherixCoSimSocket::exchangeValueImpl<SyncDirection::kSend, T>
{
    static void execute(AspherixCoSimSocket* socket, const T& object)
    {
        socket->writeValue(object);
    }
};

// Specialization for kReceive (T by value)
template <typename T> struct AspherixCoSimSocket::exchangeValueImpl<SyncDirection::kRecv, T>
{
    static void execute(AspherixCoSimSocket* socket, T& object) { object = socket->readValue<T>(); }
};

// Public interface to exchangeValue
template <SyncDirection D, typename T>
typename std::enable_if<D == SyncDirection::kSend, void>::type AspherixCoSimSocket::exchangeValue(
    const T& object)
{
    exchangeValueImpl<D, T>::execute(this, object);
}

template <SyncDirection D, typename T>
typename std::enable_if<D == SyncDirection::kRecv, void>::type AspherixCoSimSocket::exchangeValue(
    T& object)
{
    exchangeValueImpl<D, T>::execute(this, object);
}

// template <typename T> int AspherixCoSimSocket::exchangeValue(T& object)
// {
//     object = readValue<T>();
//     return 0;
// }
//
// template <typename T> int AspherixCoSimSocket::exchangeValue(const T& object)
// {
//     return writeValue(object);
// }

// // Overloaded function template for const reference
// template <CoSimSocket::SyncDirection D, typename T>
// typename std::enable_if_t<D == CoSimSocket::SyncDirection::kSend, void>::type
// AspherixCoSimSocket::
//     exchangeValueImpl(const T& object)
// {
//     writeValue(object);
//     // if (direction == SyncDirection::toServer)
//     // {
//     //     std::cout << "Exchanging value to Server with const reference." << std::endl;
//     //     // Implement exchange logic for Server
//     // }
//     // std::is_same<typename std::decay<T>::type, AspherixCoSimSocket>::value
//     //                             && (((T::mode_ == CoSimSocket::Mode::kClient)
//     //                                  && (direction ==
//     //                                  CoSimSocket::SyncDirection::kServerToClient))
//     //                                 || ((T::mode_ == CoSimSocket::Mode::kServer)
//     //                                     && (direction ==
//     //                                     CoSimSocket::SyncDirection::kClientToServer))),
//     //                         void>::type
// }
//
// template <typename T>
// void typename std::enable_if_t<D == CoSimSocket::SyncDirection::kRecv, void>::type
// AspherixCoSimSocket::exchangeValueImpl(T& object)
// {
//     object = readValue<T>();
// }

#endif // ASPHERIX_COSIM_SOCKET_I_H
#endif // _WIN32
