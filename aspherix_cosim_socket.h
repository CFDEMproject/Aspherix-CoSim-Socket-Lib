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

#include <iostream>
#include <string>
#include <vector>

#include "aspherix_cosim_field.h"

enum class SocketCodes
{
    welcome,
    close_connection,
    start_exchange,
    bounding_box_update,
    read_a_number,
    read_a_word,
    ping,
    invalid,
    request_quit
};

/*---------------------------------------------------------------------------*\
                           Class AspherixCoSimSocket Declaration
\*---------------------------------------------------------------------------*/

class AspherixCoSimSocket {

public:
    static constexpr size_t kBasePort = 49152;
    static constexpr size_t kConnectionTryLimit = 10;
    static constexpr size_t kWaitSeconds = 1;

    enum class Mode
    {
        kClient,
        kServer
    };

    bool isServer() const { return mode_ == Mode::kServer; };
    bool isClient() const { return mode_ == Mode::kClient; };

private:
    // private data
    bool mutually_closed_sockets_;
    int sockfd_;
    int insockfd_;
    Mode mode_;
    int rcvBytesPerParticle_;
    int sndBytesPerParticle_;
    std::vector<CoSimField> push_field_list_;
    std::vector<CoSimField> pull_field_list_;

    int portRangeReserved_;

    // private member functions
    void error_one(const std::string msg) const;
    void error_all(const std::string msg) const;
    size_t readNumberFromFile(const std::string path);
    void deletePortFile() const;
    void writePortFile(const std::string& port_file_path, size_t port_offset);
    void readPortFile(int proc, const std::string path, size_t& port, int& found,
                      int n_tries_max = 1);
    int tryConnect(struct sockaddr_in);
    void selectTO(int& sock);

    int waitSeconds_;
    int ntries_connect_;

    const size_t base_port_;
    int port_;
    const bool verbose_;
    const bool keepPortOffsetFile_;
    std::string portFileName_;
    const size_t processNumber_;

public:
    // Constructors

    //- Construct from components
    AspherixCoSimSocket(const Mode& mode, size_t port_offset,
                        const std::string& custom_port_file_path = "", size_t base_port = kBasePort,
                        int wait_seconds = kWaitSeconds, int ntries_connect = kConnectionTryLimit,
                        bool verbose = false, bool keep_port_offset_file = false);

    // Destructor
    ~AspherixCoSimSocket();

    // Member Functions
    template <typename T> void read_socket(T* const value);
    void read_socket(void* const buf, const size_t size) const;
    template <typename T> void write_socket(const T* const value);
    void write_socket(const void* const buf, size_t size) const;

    void syncData(std::vector<char> client_to_server, std::vector<char> server_to_client) {}

    void sendProperties();
    size_t recvProperties();
    size_t writeFieldList(const std::vector<CoSimField>& field_list);
    size_t readFieldList();
    void writeField(const CoSimField& field);
    CoSimField readField();
    void writeString(const std::string& str);
    std::string readString();

    void writeBool(bool& flag) { write_socket(&flag, sizeof(bool)); };
    void readBool(bool& flag) { read_socket(&flag, sizeof(bool)); };

    void buildBytePattern();
    SocketCodes exchangeStatus(SocketCodes statusSend = SocketCodes::ping,
                               SocketCodes statusExpect = SocketCodes::ping);
    void exchangeDomain(bool active, double* limits);

    void readData(size_t& dataSize, char*& data);
    std::vector<char> readData() const;
    void writeData(const std::vector<char>& data) const;
    void writeData(const size_t& dataSize, char* const& data);
    void closeSocket(const bool mutual = true) const;
    void mutually_closed_sockets(bool flag) { mutually_closed_sockets_ = flag; }
    bool mutually_closed_sockets() const { return mutually_closed_sockets_; }

    bool hasOpenSocket() const { return (insockfd_ > 0 || sockfd_ > 0); };

    auto getBasePort() const noexcept { return base_port_; }

    // Access Functions
    inline int get_rcvBytesPerParticle() { return rcvBytesPerParticle_; }

    inline int get_sndBytesPerParticle() { return sndBytesPerParticle_; }
    inline void addField(const CoSimField& field)
    {
        std::cout << "     adding field to list: " << field.info() << "\n";
        field.isCommStylePush() ? push_field_list_.push_back(field)
                                : pull_field_list_.push_back(field);
    }

    inline std::vector<CoSimField> getSendFieldList() { return push_field_list_; }
    inline std::vector<CoSimField> getRecvFieldList() { return pull_field_list_; }

    void printTime() const;
};

template <typename T> void AspherixCoSimSocket::read_socket(T* const value)
{
    read_socket(static_cast<void* const>(value), sizeof(T));
}

template <typename T> void AspherixCoSimSocket::write_socket(const T* const value)
{
    write_socket(static_cast<const void* const>(value), sizeof(T));
}
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif
#endif
// ************************************************************************* //
