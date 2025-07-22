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
#include <type_traits>
#ifndef _WIN32

#ifndef ASPHERIX_COSIM_SOCKET_I_H
#define ASPHERIX_COSIM_SOCKET_I_H

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

template <typename T> void AspherixCoSimSocket::readData(std::vector<T>& vector)
{
    std::size_t vector_size = 0;
    read_socket(&vector_size, sizeof(std::size_t));
    vector.resize(vector_size);
    if (vector_size > 0)
    {
        read_socket(vector.data(), vector_size * sizeof(T) / sizeof(char));
    }
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

template <typename T>
auto AspherixCoSimSocket::readValue(std::size_t size) -> typename std::enable_if<!std::is_trivially_copyable<T>::value, T>::type
{
    error("Only trivially copyable datatypes are supported for direct socket communication");
}

template <typename T>
auto AspherixCoSimSocket::readValue(std::size_t size) -> typename std::enable_if<std::is_trivially_copyable<T>::value, T>::type
{
    size_t recv_size = 0;
    int cur_size = 0;

    std::vector<char> buf;
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

    return *reinterpret_cast<T*>(buf.data());
}

template <typename T>
auto AspherixCoSimSocket::writeValue(const T& object) -> typename std::enable_if<!std::is_trivially_copyable<T>::value, int>::type
{
    error("Only trivially copyable datatypes are supported for direct socket communication");
}

template <typename T>
auto AspherixCoSimSocket::writeValue(const T& object) -> typename std::enable_if<std::is_trivially_copyable<T>::value, int>::type
{
    const std::size_t size = sizeof(T);
    const char* buf = reinterpret_cast<const char*>(&object);

    std::size_t send_size = 0;
    std::size_t cur_size = 0;

    const auto socket_file_descriptor = isServer() ? insockfd_ : sockfd_;

    while (send_size < size)
    {
        cur_size = ::write(socket_file_descriptor, buf + send_size, size - send_size);
        if (cur_size > 0)
            send_size += cur_size;
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

#endif // ASPHERIX_COSIM_SOCKET_I_H
#endif // _WIN32
