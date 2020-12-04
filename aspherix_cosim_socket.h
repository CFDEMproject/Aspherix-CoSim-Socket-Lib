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

#include <vector>
#include <string>

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
private:
    // private data
    int sockfd_;
    int insockfd_;
    bool server_;

    int nbytesInt_;
    int nbytesScalar_;
    int nbytesVector_;
    int nbytesVector2D_;
    int nbytesQuaternion_;

    int rcvBytesPerParticle_;
    int sndBytesPerParticle_;

    std::vector<int> pushBytesPerPropList_;
    std::vector<int> pushCumOffsetPerProperty_;
    std::vector<int> pullBytesPerPropList_;
    std::vector<int> pullCumOffsetPerProperty_;

    std::vector<std::string> pushNameList_;
    std::vector<std::string> pushTypeList_;
    std::vector<std::string> pullNameList_;
    std::vector<std::string> pullTypeList_;

    int portRangeReserved_;

    // private member functions
    void error_one(const std::string msg);
    void error_all(const std::string msg);
    size_t readNumberFromFile(const std::string path);
    void deleteFile(const std::string path);
    void readPortFile(int proc, const std::string path,size_t& port,int& found,int n_tries_max=1);
    int tryConnect(struct sockaddr_in);
    int selectTO(int& sock);

    int waitSeconds_;
    
    const bool verbose_;
    const bool keepPortOffsetFile_;
    std::string portFileName_;
    const size_t processNumber_;

public:
    // Constructors

    //- Construct from components
    AspherixCoSimSocket
    (
        bool mode,
        const size_t port_offset,
        std::string customPortFilePath="",
        int  waitSeconds=1,
        bool verbose=false,
        bool keepPortOffsetFile=false
    );

    // Destructor
    ~AspherixCoSimSocket();

    // Member Functions
    void read_socket(void *const buf, const size_t size);
    void write_socket(void *const buf, const size_t size);
    void sendPushPullProperties();
    void buildBytePattern();
    void exchangeStatus(SocketCodes statusSend, SocketCodes statusExpect);
    void exchangeDomain(bool active, double* limits);
    void rcvData(size_t& dataSize, char*& data);
    void sendData(size_t& dataSize, char*& data);
    void closeSocket();

    // Access Functions
    inline int get_rcvBytesPerParticle(){return rcvBytesPerParticle_;}
    inline void set_rcvBytesPerParticle(int var){rcvBytesPerParticle_=var;}

    inline int get_sndBytesPerParticle(){return sndBytesPerParticle_;}
    inline void set_sndBytesPerParticle(int var){sndBytesPerParticle_=var;}

    inline std::vector<int> get_pushBytesPerPropList(){return pushBytesPerPropList_;}
    inline void set_pushBytesPerPropList(std::vector<int> var){pushBytesPerPropList_=var;}

    inline std::vector<int> get_pushCumOffsetPerProperty(){return pushCumOffsetPerProperty_;}
    inline void set_pushCumOffsetPerProperty(std::vector<int> var){pushCumOffsetPerProperty_=var;}

    inline std::vector<int> get_pullBytesPerPropList(){return pullBytesPerPropList_;}
    inline void set_pullBytesPerPropList(std::vector<int> var){pullBytesPerPropList_=var;}

    inline std::vector<int> get_pullCumOffsetPerProperty(){return pullCumOffsetPerProperty_;}
    inline void set_pullCumOffsetPerProperty(std::vector<int> var){pullCumOffsetPerProperty_=var;}

    inline std::vector<std::string> get_pushNameList(){return pushNameList_;}
    inline void set_pushNameList(std::vector<std::string> var){pushNameList_=var;}
    inline void pushBack_pushNameList(std::string var){pushNameList_.push_back(var);}

    inline std::vector<std::string> get_pushTypeList(){return pushTypeList_;}
    inline void set_pushTypeList(std::vector<std::string> var){pushTypeList_=var;}
    inline void pushBack_pushTypeList(std::string var){pushTypeList_.push_back(var);}

    inline std::vector<std::string> get_pullNameList(){return pullNameList_;}
    inline void set_pullNameList(std::vector<std::string> var){pullNameList_=var;}
    inline void pushBack_pullNameList(std::string var){pullNameList_.push_back(var);}

    inline std::vector<std::string> get_pullTypeList(){return pullTypeList_;}
    inline void set_pullTypeList(std::vector<std::string> var){pullTypeList_=var;}
    inline void pushBack_pullTypeList(std::string var){pullTypeList_.push_back(var);}
    
    void printTime();
};

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif
#endif
// ************************************************************************* //
