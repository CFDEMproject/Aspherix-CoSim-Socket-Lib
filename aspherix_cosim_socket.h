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

#ifndef ASPHERIX_COSIM_SOCKET_H
#define ASPHERIX_COSIM_SOCKET_H

#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace CoSimSocket
{

enum class SocketCodes : std::uint8_t
{
    kWelcome,
    kCloseConnection,
    kStartExchange,
    kBoundingBoxUpdate,
    kReadANumber,
    kReadString,
    kPing,
    kInvalid,
    kRequestQuit,
    kUndefined
};

enum class SocketStatus : std::uint8_t
{
    kInactive,
    kActive
};

enum class Mode : std::uint8_t
{
    kClient,
    kServer
};

enum class SyncDirection : std::uint8_t
{
    kClientToServer,
    kServerToClient,
    kUndefined,
    kSend,
    kRecv
};

constexpr std::size_t kBasePort           = 49152;
constexpr std::size_t kConnectionTryLimit = 10;
constexpr std::size_t kWaitSeconds        = 0;
constexpr std::size_t kNumberOfAttempts   = 10;

/*---------------------------------------------------------------------------*\
                           Class AspherixCoSimSocket Declaration
\*---------------------------------------------------------------------------*/

class AspherixCoSimSocket {

public:
    [[nodiscard]] bool isServer() const { return mode_ == Mode::kServer; };
    [[nodiscard]] bool isClient() const { return mode_ == Mode::kClient; };

private:
    // private data
    bool mutually_closed_sockets_;
    int sockfd_;
    int insockfd_;
    Mode mode_;

    int portRangeReserved_;

    // private member functions
    // Member Functions
    void read_socket(void* buf, std::size_t size);
    void write_socket(const void* buf, std::size_t size);

    template <typename T> void write_socket(const T* const value);
    template <typename T> void read_socket(T* const value);

    void error(const std::string& msg);
    std::size_t readNumberFromFile(const std::string& path, std::size_t max_attempts);
    void deletePortFile() const;
    void writePortFile(const std::string& port_file_path, std::size_t port_offset);
    std::pair<std::size_t, bool> readPortFile(const std::string& path,
                                              std::size_t number_of_attempts = 1);
    // int tryConnect(struct ::sockaddr_in);
    void selectTO(int& sock);

    int wait_seconds_;
    int ntries_connect_;

    std::size_t base_port_;
    std::size_t port_;
    bool verbose_;
    bool keepPortOffsetFile_;
    std::string portFileName_;
    std::size_t process_number_;
    SocketStatus status_;

public:
    // Constructors

    //- Construct from components
    AspherixCoSimSocket(const Mode& mode, std::size_t process_number,
                        const std::string& custom_port_file_path = "",
                        std::size_t base_port = kBasePort, int wait_seconds = kWaitSeconds,
                        std::size_t ntries_connect = kConnectionTryLimit, bool verbose = false,
                        bool keep_port_offset_file = false);

    AspherixCoSimSocket(const AspherixCoSimSocket&)            = default;
    AspherixCoSimSocket(AspherixCoSimSocket&&)                 = delete;
    AspherixCoSimSocket& operator=(const AspherixCoSimSocket&) = default;
    AspherixCoSimSocket& operator=(AspherixCoSimSocket&&)      = delete;

    // Destructor
    ~AspherixCoSimSocket();

    void writeString(const std::string& str);
    std::string readString();

    void writeBool(bool flag) { write_socket(&flag, sizeof(bool)); };
    // inline bool readBool();
    bool readBool()
    {
        bool flag = false;
        read_socket(&flag, sizeof(bool));
        return flag;
    };

    template <typename T> int writeValue(const T& object);
    template <typename T> auto readValue(std::size_t size = sizeof(T)) -> T;

    template <typename T>
    int exchangeValue(T& object, SyncDirection direction, std::size_t size = sizeof(T));

    // // Primary template for exchangeValue (not defined)
    // template <SyncDirection D, typename T, typename = void> struct exchangeValueImpl;

    // // Specialization for kSend (const T&)
    // template <typename T>
    // struct exchangeValueImpl<SyncDirection::kSend, T> {
    //     void execute(const T& object) {
    //         std::cout << "Exchanging value for sending: " << object << std::endl;
    //         // Implement send logic here
    //     }
    // };

    // // Specialization for kReceive (T by value)
    // template <typename T>
    // struct exchangeValueImpl<SyncDirection::kReceive, T> {
    //     void execute(T object) { // Takes T by value
    //         std::cout << "Exchanging value for receiving: " << object << std::endl;
    //         // Implement receive logic here
    //     }
    // };

    // Primary template for exchangeValue (not defined)
    template <SyncDirection D, typename T, typename = void> struct exchangeValueImpl;

    // Public interface to exchangeValue
    // template <SyncDirection D, typename T> void exchangeValue(T& object);
    // template <SyncDirection D, typename T> void exchangeValue(const T& object);
    template <SyncDirection D, typename T>
    typename std::enable_if<D == SyncDirection::kSend, void>::type exchangeValue(const T& object);

    template <SyncDirection D, typename T>
    typename std::enable_if<D == SyncDirection::kRecv, void>::type exchangeValue(T& object);
    // {
    //     exchangeValueImpl<D, T> impl;          // Create an instance
    //     impl.execute(std::forward<T>(object)); // Call the instance method
    // }

    // template <CoSimSocket::SyncDirection D, typename T>
    // typename std::enable_if_t<D == CoSimSocket::SyncDirection::kRecv, void>::type exchangeValue(
    //     T& object);
    // template <CoSimSocket::SyncDirection D, typename T>
    // typename std::enable_if_t<D == CoSimSocket::SyncDirection::kSend, void>::type exchangeValue(
    //     const T& object);
    // template <typename T> int exchangeValue(T& object);
    // template <typename T> int exchangeValue(const T& object);
    // template <CoSimSocket::SyncDirection D, typename T> void exchangeValue(const T& object);

    SocketCodes exchangeStatus(SocketCodes status_send   = SocketCodes::kPing,
                               SocketCodes status_expect = SocketCodes::kUndefined);
    // void exchangeDomain(bool active, double* limits);

    // void readData(std::size_t& dataSize, char*& data);
    std::vector<char> readData();
    void writeData(const std::vector<char>& data);
    // void writeData(const std::size_t& dataSize, char* const& data);

    void closeSocket(bool mutual = true);
    // void mutually_closed_sockets(bool flag) { mutually_closed_sockets_ = flag; }
    // bool mutually_closed_sockets() const { return mutually_closed_sockets_; }

    [[nodiscard]] bool hasOpenSocket() const { return (insockfd_ > 0 || sockfd_ > 0); };

    [[nodiscard]] auto getBasePort() const noexcept { return base_port_; }

    void printTime() const;

    void showBufferSizeInfo();
};

#include "aspherix_cosim_socket_I.h"
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // namespace CoSimSocket

#endif
#endif
// ************************************************************************* //
