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

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <type_traits>
#include <unistd.h>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

template <typename T>
T AspherixCoSimSocket::readSocket(size_t size) const
{
    static constexpr bool needs_size = !std::is_trivially_copyable<T>::value;

    size_t recv_size = 0;
    int cur_size = 0;

    std::vector<char> buf;
    buf.reserve( size );
    buf.resize( size );

    const auto socket_file_descriptor = isServer() ? insockfd_: sockfd_;

    while (recv_size < size)
    {
        cur_size = ::read( socket_file_descriptor, buf.data() + recv_size, size - recv_size);

        if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for reading data " << std::to_string(errno) << std::endl;
            else
                error_one(std::string("\n\nERROR: AspherixCoSimSocket::read_socket: Failed getting data. ")+std::to_string(cur_size));
        }
        else if (cur_size == 0)
            error_one(std::string("\n\nERROR: AspherixCoSimSocket::read_socket: Disconnected. ")+std::to_string(cur_size));

        recv_size += cur_size;
    }

    if constexpr (needs_size)
        return buf;
    else
        return *reinterpret_cast<T*>(buf.data());
}

template<typename T>
int AspherixCoSimSocket::writeSocket(const T &object) const
{
    static constexpr bool needs_size = !std::is_trivially_copyable<T>::value;

    size_t size;
    if constexpr (needs_size)
        size = object.size();
    else
        size = sizeof(T);

    const char *buf = nullptr;
    if constexpr (needs_size)
        buf = reinterpret_cast<const char*>(object.data());
    else //if (std::is_enum<T>::value)
        buf = reinterpret_cast<const char*>(&object);

    auto send_size = 0;
    auto cur_size = 0;

    const auto socket_file_descriptor = isServer() ? insockfd_: sockfd_;

    while (send_size < size)
    {

    //    if constexpr (needs_size)
    //        cur_size = ::write(socket_file_descriptor, static_cast<const char*>(object.data()) + send_size, size - send_size);
    //    else if (std::is_enum<T>::value)
            cur_size = ::write(socket_file_descriptor, buf + send_size, size - send_size);
    //    else
    //        cur_size = ::write(socket_file_descriptor, static_cast<const char*>(object) + send_size, size - send_size);
            //cur_size = ::write(socket_file_descriptor, static_cast<const char*>(static_cast<typename std::underlying_type<T>::type>(object)) + send_size, size - send_size);

        if (cur_size > 0)
            send_size += cur_size;
        else if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for sending data " << std::to_string(errno) << std::endl;
            else
                error_one("\n\nERROR: AspherixCoSimSocket::write_socket: Failed sending data.\n");
        }
        else if (cur_size == 0)
            error_one(std::string("\n\nERROR: AspherixCoSimSocket::write_socket: Disconnected. ")+std::to_string(cur_size));
    }
    if (size != send_size)
        return 1;
        // throw error
    return 0;
}

#endif // ASPHERIX_COSIM_SOCKET_I_H
#endif // _WIN32
