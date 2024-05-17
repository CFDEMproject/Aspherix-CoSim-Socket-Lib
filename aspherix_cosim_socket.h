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
    welcome_server,
    welcome_client,
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

class AspherixCoSimSocket
{

public:

enum class Mode
{
    kClient,
    kServer
};

private:

    bool isServer() const
    { return mode_ == Mode::kServer; };

    bool isClient() const
    { return mode_ == Mode::kClient; };

    // private data
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
    void deleteFile(const std::string path);
    void readPortFile(int proc, const std::string path,size_t& port,int& found,int n_tries_max=1);
    int tryConnect(struct sockaddr_in);
    void selectTO(int& sock);

    int waitSeconds_;
    int ntries_connect_;

    int port_;
    const bool verbose_;
    const bool keepPortOffsetFile_;
    std::string portFileName_;
    const size_t processNumber_;

public:
    // Constructors

    //- Construct from components
    AspherixCoSimSocket
    (
        Mode mode,
        const size_t port_offset,
        std::string customPortFilePath="",
        int  waitSeconds=1,
        int  ntries_connect_=10,
        bool verbose=false,
        bool keepPortOffsetFile=false
    );

    // Destructor
    ~AspherixCoSimSocket();

    // Member Functions
    void read_socket(void *const buf, const size_t size) const;
    void write_socket(const void *const buf, const size_t size) const;
    void sendProperties();
    size_t recvProperties();
    size_t writeFieldList(const std::vector<CoSimField> &field_list);
    size_t readFieldList();
    void writeField(const CoSimField &field);
    CoSimField readField();
    void writeString(const std::string &str);
    std::string readString();

    void buildBytePattern();
    void exchangeStatus(SocketCodes statusSend, SocketCodes statusExpect);
    void exchangeDomain(bool active, double* limits);

    void readData(size_t& dataSize, char*& data);
    void writeData(const size_t& dataSize, char *const &data);
    void closeSocket() const;

    // Access Functions
    inline int get_rcvBytesPerParticle(){return rcvBytesPerParticle_;}

    inline int get_sndBytesPerParticle(){return sndBytesPerParticle_;}
    inline void addField(const CoSimField &field)
    {
        std::cout << "     adding field to list: " << field.info() << "\n";
        field.isCommStylePush() ? push_field_list_.push_back(field) : pull_field_list_.push_back(field);
    }

    inline std::vector<CoSimField> getSendFieldList() { return push_field_list_; }
    inline std::vector<CoSimField> getRecvFieldList() { return pull_field_list_; }

    void printTime();
};

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif
#endif
// ************************************************************************* //
