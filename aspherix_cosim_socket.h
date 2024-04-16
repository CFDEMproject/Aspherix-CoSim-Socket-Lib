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

class AspherixCoSimSocket {

private:
    // private data
    int sockfd_;
    int insockfd_;
    bool server_;
    /*
        int nbytesInt_;
        int nbytesScalar_;
        int nbytesVector_;
        int nbytesVector2D_;
        int nbytesQuaternion_;
    */
    int rcvBytesPerParticle_;
    int sndBytesPerParticle_;
    /*
        std::vector<int> pushBytesPerPropList_;
        std::vector<int> pushCumOffsetPerProperty_;
        std::vector<int> pullBytesPerPropList_;
        std::vector<int> pullCumOffsetPerProperty_;
    */
    std::vector<CoSimField> push_field_list_;
    std::vector<CoSimField> pull_field_list_;

    int portRangeReserved_;

    // private member functions
    void error_one(const std::string msg) const;
    void error_all(const std::string msg) const;
    size_t readNumberFromFile(const std::string path);
    void deleteFile(const std::string path);
    void readPortFile(int proc, const std::string path, size_t& port, int& found,
                      int n_tries_max = 1);
    int tryConnect(struct sockaddr_in);
    void selectTO(int& sock);

    int waitSeconds_;
    int ntries_connect_;

    const bool verbose_;
    const bool keepPortOffsetFile_;
    std::string portFileName_;
    const size_t processNumber_;

public:
    // Constructors

    //- Construct from components
    AspherixCoSimSocket(bool mode, const size_t port_offset, std::string customPortFilePath = "",
                        const size_t portBase = 49152, int waitSeconds = 1,
                        int ntries_connect_ = 10, bool verbose = false,
                        bool keepPortOffsetFile = false);

    // Destructor
    ~AspherixCoSimSocket();

    // Member Functions
    template <typename T> T readSocket() const;

    template <typename T> int writeSocket(const T& data) const;

    void read_socket(void* const buf, const size_t size) const;
    void write_socket(const void* const buf, const size_t size) const;
    void sendPushPullProperties();
    size_t writeFieldList(const std::vector<CoSimField>& field_list);
    size_t readFieldList();
    void writeField(const CoSimField& field);
    CoSimField readField();
    void writeString(const std::string& str);
    std::string readString();

    void buildBytePattern();
    void exchangeStatus(SocketCodes statusSend, SocketCodes statusExpect);
    void exchangeDomain(bool active, double* limits);
    std::vector<uint8_t> readData() const;
    void writeData(const size_t& dataSize, const uint8_t*& data);
    void closeSocket() const;

    // Access Functions
    inline int get_rcvBytesPerParticle() { return rcvBytesPerParticle_; }
    //    inline void set_rcvBytesPerParticle(int var){rcvBytesPerParticle_=var;}

    inline int get_sndBytesPerParticle() { return sndBytesPerParticle_; }
    //    inline void set_sndBytesPerParticle(int var){sndBytesPerParticle_=var;}
    /*
        inline std::vector<int> get_pushBytesPerPropList(){return pushBytesPerPropList_;}
        inline void set_pushBytesPerPropList(std::vector<int> var){pushBytesPerPropList_=var;}

        inline std::vector<int> get_pushCumOffsetPerProperty(){return pushCumOffsetPerProperty_;}
        inline void set_pushCumOffsetPerProperty(std::vector<int>
       var){pushCumOffsetPerProperty_=var;}

        inline std::vector<int> get_pullBytesPerPropList(){return pullBytesPerPropList_;}
        inline void set_pullBytesPerPropList(std::vector<int> var){pullBytesPerPropList_=var;}

        inline std::vector<int> get_pullCumOffsetPerProperty(){return pullCumOffsetPerProperty_;}
        inline void set_pullCumOffsetPerProperty(std::vector<int>
       var){pullCumOffsetPerProperty_=var;}
    */
    inline void addField(const CoSimField& field)
    {
        field.comm == CommStyle::Push ? push_field_list_.push_back(field)
                                      : pull_field_list_.push_back(field);
    }

    inline std::vector<CoSimField> getPushFieldList() { return push_field_list_; }
    inline std::vector<CoSimField> getPullFieldList() { return pull_field_list_; }

    void printTime();
};

#ifdef __INCLUDE_PRIVATE_SOCKET__
#include "aspherix_cosim_socket_I.h"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif
#endif
// ************************************************************************* //
