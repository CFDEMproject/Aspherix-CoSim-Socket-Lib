/*---------------------------------------------------------------------------*\
    CFDEMcoupling - Open Source CFD-DEM coupling

    CFDEMcoupling is part of the CFDEMproject
    www.cfdem.com
                                Christoph Goniva, christoph.goniva@cfdem.com
                                Copyright 2012-     DCS Computing GmbH, Linz
-------------------------------------------------------------------------------
License
    This file is part of CFDEMcoupling.

    CFDEMcoupling is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 3 of the License, or (at your
    option) any later version.

    CFDEMcoupling is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with CFDEMcoupling; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Description
    This code provides a protocol for CoSimulation data transfer.
    Note: this code is not part of OpenFOAM(R) (see DISCLAIMER).

Class
    socket

SourceFiles
    socket.cpp

\*---------------------------------------------------------------------------*/

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

// this is not available on Windows
#ifndef _WIN32

#ifndef Socket_H
#define Socket_H
enum class SocketCodes
{
    welcome_server,
    welcome_client,
    close_connection,
    start_exchange,
    bounding_box_update,
    read_a_number,
    read_a_word,
    invalid,
    request_quit
};


/*---------------------------------------------------------------------------*\
                           Class Socket Declaration
\*---------------------------------------------------------------------------*/

class Socket
{
private:
  // private data
    int sockfd_;
    int insockfd_;
    bool server_;

    int nbytesScalar_;
    int nbytesVector_;

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
public:
    // Constructors

        //- Construct from components
        Socket(bool mode, const size_t port_offset);

    // Destructor
        ~Socket();


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
};

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif
#endif
// ************************************************************************* //
