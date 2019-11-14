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
\*---------------------------------------------------------------------------*/

// this is not available on Windows
#ifndef _WIN32

#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h> // needed for connect with timeout

#include <vector>
#include <numeric>
#include <fstream>
#include <mpi.h>
#include "socket.h"

#define PORT 49152
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

// Construct from components
Socket::Socket
(
    bool mode,
    const size_t processNumber
)
:
    sockfd_(0),
    insockfd_(0),
    server_(mode),
    nbytesScalar_(8),
    nbytesVector_(3*nbytesScalar_),
    rcvBytesPerParticle_(0),
    sndBytesPerParticle_(0),
    pushBytesPerPropList_(0),
    pushCumOffsetPerProperty_(0),
    pullBytesPerPropList_(0),
    pullCumOffsetPerProperty_(0),
    pushNameList_(0),
    pushTypeList_(0),
    pullNameList_(0),
    pullTypeList_(0),
    portRangeReserved_(1)
{

    // create socket with DEM process
    if(processNumber==0)
    {
        if(server_) std::cout << "\nCreate socket with CFD process ..." << std::endl;
        else std::cout << "\nCreate socket with DEM process ..." << std::endl;        
    }

    //==================================================
    // CHECK IF PORT FILE EXISTS AND READ IF IT DOES
    size_t portOffset(0);
    int foundPortFile(0);

    // determine the file path for the portOffset file
    // Problem: here we assume CFD and DEM live in their own directories and
    // both directories have the same mother directory
    // TODO: find a better solution (e.g. absolute file path and unique filename?)
	size_t size;
    char *path=NULL;
	path=getcwd(path,size);
	std::string cwd=path;
    std::string portFilePath(cwd+"/../DEM/portOffset_"+std::to_string(processNumber)+".txt");

    if(server_)
    {
        // check if portfile exists and read it
        readPortFile(processNumber,portFilePath,portOffset,foundPortFile);

        if(foundPortFile==0)
        {
            if(processNumber==0)
                std::cout << "\nDEM could not find portOffset file.\n"
                          << "   As a fallback an automatic detection of an available port will be started.\n"
                          << "*  Find details in the documentation (look for 'Setup a case using socket communication').\n" << std::endl;

            // enforce portFile defined by user (no auto detect)
            //error_one("FatalError: portOffset files not found.");
        }
    }
    //==================================================

    // Creating socket file descriptor
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0)
        error_one("\n\nERROR: Socket creation failed");

    if(foundPortFile==1)
    {
        std::cout << "Server: will forcefully attach to port " << std::to_string(PORT+processNumber+portOffset) << "!" << std::endl;
        int opt = 1;
        // Forcefully attaching socket to the port
        if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
            error_one("Failed setsockopt");
    }

    //connection will close immediately after closing your program;
    //and next restart will be able to bind again.
    linger lin;
    lin.l_onoff = 0;
    lin.l_linger = 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(int));
    
    struct sockaddr_in address;
    memset(&address, 0, sizeof(sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    int success(0);
    if(server_)
    {
        int n_tries(0);
        int n_tries_max(std::min(100,16000/portRangeReserved_)); // go max from port 49152 to ~65535
        if(foundPortFile==1) n_tries_max=0;
        while(success==0)
        {
            address.sin_port = htons(PORT+processNumber+portOffset);
            success=0;
            n_tries++;

            std::cout << "Server: process number " << processNumber << " trying to bind/listen with PORT(49152+portOffset+procNr)=" 
                      << std::to_string(PORT+processNumber+portOffset) << std::endl;

            // try attaching socket to the port
            if (bind(sockfd_, (struct sockaddr *)&address, sizeof(address)) < 0)
            {
                std::cout << "  process number " << processNumber << " Bind to "
                          << std::to_string(PORT+processNumber+portOffset) << " failed." << std::endl;

                if(n_tries > n_tries_max)
                {
                    std::cout << "Server:  " << processNumber 
                              << " Bind to " << std::to_string(PORT+processNumber+portOffset) 
                              << " failed (probably the port is not (yet?) available?)" << std::endl;
                    break; // tried enough
                }
                portOffset+=portRangeReserved_;     // increase port by nProcs
                sleep(0.1);
            }
            else
            {
                std::cout << "  process number " << processNumber << " Bind was successful with portOffset = " << portOffset << std::endl;
                success=1;
            }
        }
        if(success==0)  error_one("Bind failed after all tries.");

        MPI_Barrier(MPI_COMM_WORLD);
        if(processNumber==0) std::cout << "\nServer: All processes bound successfully\n" << std::endl;

        // if bind was successful, continue with listen              
        if (listen(sockfd_, 5) < 0)
        {
            std::cout << "  process number " << processNumber << " Listen to "
                  << std::to_string(PORT+processNumber+portOffset) << " failed." << std::endl;
        }
        else // if listen was successful, communicate port to client
        {
            std::cout << "  process number " << processNumber << " Bind+Listen to "
                  << std::to_string(PORT+processNumber+portOffset) << " successful\n" << std::endl;
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if(processNumber==0) std::cout << "\nServer: All processes listen successfully\n" << std::endl;
    }

    // communicate suitable port with client via file
    if(server_) // server reads port from file if exists or writes suitable port to file
    {
        // only for auto port detection
        if(foundPortFile==0)
        //if(success==1)
        {
            std::cout << "Server: write portOffset=" << portOffset << " to a file... " << std::endl;
            std::ofstream myfile2;
            myfile2.open (portFilePath);
            if (myfile2.is_open())
            {
                myfile2 << std::to_string(portOffset) << "\n";
                myfile2.close();
            }
            else error_one("Server: Unable to open file");
        }//else error_one("ERROR");
    }
    else // client/server reads suitable port from file
    {
        // check if portfile exists and read it
        readPortFile(processNumber,portFilePath,portOffset,foundPortFile,10);

        if(!foundPortFile)
        {
            if(processNumber==0) std::cout << "\nERROR: CFD could not find portOffset file.\n"
                                           << "   Probably there was no user defined portOffset file and DEM was not able to find suitable ports.\n"
                                           << "*  Find details in the documentation (look for 'Setup a case using socket communication').\n" << std::endl;
            error_one("FatalError: portOffset file not found.");
        } 
    }

    // server accept socket / client connect to socket
    if(server_)
    {
        std::cout << "Server: process number " << processNumber << " Try Accept..." << std::endl;
        sleep(3);
        // test the socket with t/o before accept
        //int result = selectTO(sockfd_);

        socklen_t addrlen = sizeof(address);
        insockfd_ = accept(sockfd_, (struct sockaddr *)&address, &addrlen); // waits for client to connect!!!
        //send(insockfd_, "1", 1, 0);
        if (insockfd_ < 0) error_one("Accept failed");
        else std::cout << "Server: process number " << processNumber << " Accept successful." << std::endl;

        //connection will close immediately after closing your program;
        //and next restart will be able to bind again.
        setsockopt(insockfd_, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(int));
    }
    else // client implementation
    {
        std::cout << "Client: process number " << processNumber << " trying to connect with PORT(49152+portOffset+procNr)="
                  << std::to_string(PORT+processNumber+portOffset) << std::endl; 
        address.sin_port = htons(PORT+processNumber+portOffset);

        // trying connecton first
        //int result = tryConnect(address); // does not work?

        // test the socket with t/o before accept
        //int result = selectTO(sockfd_);

        int ntries = 0;
        while (connect(sockfd_, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            sleep(1);
            ntries++;
            if (ntries > 5)
            {
                std::cout << "Client: " << processNumber << " Connecting to socket port " 
                          << std::to_string(PORT+processNumber+portOffset) << " failed. " << std::endl;
                std::cout << "\nERROR: CFD could not connect to port.\n"
                          << "Probably the DEM run could not bind/connect to the port.\n"
                          << "*  Please make sure DEM was started as a separate run. Find details in the documentation (look for 'Setup a case using socket communication').\n"
                          << "** Please make sure the DEM input script has a fix couple/cfd.\n"
                          << "*** If DEM was started separately & a fix couple/cfd is used, probably the port it tried to use is not available. "
                          << "Please check the DEM logfile and try a different port (specified in DEM/portOffset.txt).\n" << std::endl;
                error_one("Connection Failed"); //std::cerr << "Connection Failed" << std::endl; std::exit(1);
            }
            else std::cout << "Client: " << processNumber << " Connection attempt " << ntries <<"/10" << std::endl;
        }
        //char buf[1];
        //recv(sockfd_, buf, 1, MSG_WAITFORONE);
        std::cout << "Client: process number " << processNumber << " Connection established." << std::endl;

        MPI_Barrier(MPI_COMM_WORLD);
        if(processNumber==0) std::cout << "\nClient: All processes connected successfully\n" << std::endl;
    }

    // test connection
    //std::cout << "Server: process number " << processNumber << " testing connection (read/write)..." << std::endl;
    SocketCodes test_connection_out = SocketCodes::welcome_client;
    if(server_) test_connection_out = SocketCodes::welcome_server;       
    SocketCodes test_connection_in = SocketCodes::invalid;
    write_socket(&test_connection_out, sizeof(SocketCodes));
    read_socket(&test_connection_in, sizeof(SocketCodes));

    if(server_)
    {
        if (test_connection_in != SocketCodes::welcome_client)
            error_one("Wrong hello received from client");

        std::cout << "Server: process number " << processNumber << " Socket connection established & tested on port "
                  << std::to_string(PORT+processNumber+portOffset) << std::endl;
    }
    else
    {
        if (test_connection_in != SocketCodes::welcome_server)
            error_one("Wrong hello received from server");

        std::cout << "Client: process number " << processNumber << " Socket connection established & tested on port "
                  << std::to_string(PORT+processNumber+portOffset) << std::endl;
    }
}

// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //
Socket::~Socket()
{
    SocketCodes msg = SocketCodes::close_connection;
    write_socket(&msg, sizeof(SocketCodes));
    read_socket(&msg, sizeof(SocketCodes));
    closeSocket();
}

// * * * * * * * * * * * * * * * private Member Functions  * * * * * * * * * * * * * //
void Socket::error_one(const std::string msg)
{
    //sleep(10); // sleep so client has a chance to shut down first
    closeSocket();
    throw std::runtime_error(msg);
}

void Socket::error_all(const std::string msg)
{
    closeSocket();
    throw std::runtime_error(msg);
}

size_t Socket::readNumberFromFile(const std::string path)
{
    size_t number;
    std::string line;
    std::ifstream myfile (path);
    int ntries = 0;
    sleep(1); // what if there is a file lying around and client reads before server has written? //wait? // time stamp? // delete before and wait?
    while (!myfile.is_open())
    {
        sleep(1);
        ntries++;
        if (ntries > 10) error_one("Opening File Failed"); //std::cerr << "Opening File Failed" << std::endl; std::exit(1);                    
        else std::cout << "Opening file attempt, path=" << path << " ntries=" << ntries <<"/10" << std::endl;
    }
    while ( std::getline (myfile,line) ) number=std::stoi(line);
    myfile.close();

    return number;
}

void Socket::deleteFile(const std::string path)
{
    if( remove( path.c_str() ) != 0 )
        std::cout << "Server: file" + path << " does not exist - nothing to do." << std::endl;
    else
        puts( ("Server: File " + path + " successfully deleted.").c_str() );
}

void Socket::readPortFile(int proc, const std::string path,size_t& port,int& found,int n_tries_max)
{
    std::cout << "        trying to read file " << path << "..." << std::endl;
    int success=0;
    int n_tries=0;
    while(success==0)
    {
        n_tries++;
        if (std::ifstream(path)) // if file exists
        {
            found=1;
            port = readNumberFromFile(path);
            std::cout << "        portOffset of this simulation run is read from file: portOffset=" << port << std::endl;

            // sanity check of port
            if(port < 0)
                error_one("ERROR: please choose the port > 0");

            success=1;
        }
        if(success==0)
        {
            if(n_tries >= n_tries_max) break; // tried enough
            std::cout << "        portOffset of this simulation could not be read attempt " << n_tries <<"/" << n_tries_max << std::endl;
            sleep(1);
        }
    }
}

int Socket::tryConnect(struct sockaddr_in address)
{
      //=====================
      // test connect in non-blocking mode
      // connect with timeout (currently connected)
      // PROBLEM: program hangs if connect fails - so we want to "test" connect with a timeout
      // THIS CODE SNIPPET COMPILES BUT DOES NOT WORK AS DESIRED
      int res; 
      long arg; 
      //fd_set myset; 
      //struct timeval tv; 
      //int valopt; 
      //socklen_t lon; 

      // Set non-blocking 
      if( (arg = fcntl(sockfd_, F_GETFL, NULL)) < 0) { 
         fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno)); 
         exit(0); 
      }
      arg |= O_NONBLOCK; 
      if( fcntl(sockfd_, F_SETFL, arg) < 0) { 
         fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno)); 
         exit(0); 
      }
      // Trying to connect with timeout 
      res = connect(sockfd_, (struct sockaddr *)&address, sizeof(address)); 
      if (res < 0) { 
         if (errno == EINPROGRESS) { 
            fprintf(stderr, "EINPROGRESS in connect()\n");

            /*// further tesing with timeout
            do { 
               tv.tv_sec = 1; 
               tv.tv_usec = 0; 
               FD_ZERO(&myset); 
               FD_SET(sockfd_, &myset); 
               res = select(sockfd_+1, NULL, &myset, NULL, &tv); 
               if (res < 0 && errno != EINTR) { 
                  fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno)); 
                  exit(0); 
               } 
               else if (res > 0) { 
                  // Socket selected for write 
                  lon = sizeof(int); 
                  if (getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) { 
                     fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno)); 
                     exit(0); 
                  } 
                  // Check the value returned... 
                  if (valopt) { 
                     fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt, strerror(valopt)); 
                     exit(0); 
                  }
                  break;                 
               } 
               else { 
                  fprintf(stderr, "Timeout in select() - Cancelling!\n"); 
                  exit(0); 
               } 
            } while (1);*/
         } 
         else { 
            fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno)); 
            exit(0); 
         } 
      }

      // Set to blocking mode again... 
      if( (arg = fcntl(sockfd_, F_GETFL, NULL)) < 0) { 
         fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno)); 
         exit(0); 
      } 
      arg &= (~O_NONBLOCK); 
      if( fcntl(sockfd_, F_SETFL, arg) < 0) { 
         fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno)); 
         exit(0); 
      }

    return res;
}

int Socket::selectTO(int& sockfd)
{
    // use select to test the connection with a timeout
    fd_set sock;
    struct timeval tv;
    tv.tv_sec = 2; 
    tv.tv_usec = 0; 

    FD_ZERO(&sock); 
    FD_SET(sockfd,&sock); 

    int retval = select(sockfd+1, &sock, NULL, NULL, &tv);

    // only if all processes successfully select we want to proceed
    int all_retval;
    MPI_Allreduce(&retval, &all_retval, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    retval = all_retval;

    if (retval <= 0) error_one("Error: select failed.");
    return retval;
}

// * * * * * * * * * * * * * * * public Member Functions  * * * * * * * * * * * * * //
void Socket::write_socket(void *const buf, const size_t size)
{
    size_t send_size = 0;
    int cur_size(0);
    while (send_size < size)
    {
        if(server_)
            cur_size = ::write(insockfd_, static_cast<char*>(buf)+send_size, size-send_size);
        else
            cur_size = ::write(sockfd_, static_cast<char*>(buf)+send_size, size-send_size);
        if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for sending data " << std::to_string(errno) << std::endl;
            else
                error_one("\n\nERROR: Socket::write_socket: Failed sending data.\n");
        }
        else if (cur_size == 0)
            error_one(std::string("\n\nERROR: Socket::write_socket: Disconnected. ")+std::to_string(cur_size));

        send_size += cur_size;
    }
}

void Socket::read_socket(void *const buf, const size_t size)
{
    size_t recv_size = 0;
    int cur_size(0);
    while (recv_size < size)
    {
        if(server_)
            cur_size = ::read(insockfd_, static_cast<char*>(buf)+recv_size, size-recv_size);
        else
            cur_size = ::read(sockfd_, static_cast<char*>(buf)+recv_size, size-recv_size);
        if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for reading data " << std::to_string(errno) << std::endl;
            else
                error_one(std::string("\n\nERROR: Socket::read_socket: Failed getting data. ")+std::to_string(cur_size));
        }
        else if (cur_size == 0)
            error_one(std::string("\n\nERROR: Socket::read_socket: Disconnected. ")+std::to_string(cur_size));

        recv_size += cur_size;
    }
}

void Socket::sendPushPullProperties()
{
    // send number of push (from DEM to CFD) properties
    //std::cout << "    send number of push (from DEM to CFD) properties ... pushNameList_.size()=" << pushNameList_.size() << std::endl;
    size_t h=pushNameList_.size();
    write_socket(&h, sizeof(size_t));
    //std::cout << "    send number of push (from DEM to CFD) properties - done." << std::endl;

    // send push (from DEM to CFD) names and types
    //std::cout << "    send push (from DEM to CFD) names and types ..." << std::endl;
    for (int i = 0; i < h; i++)
    {
        //std::cout << "      send pushNameList_[i].size()=" << pushNameList_[i].size() << std::endl;
        size_t name_len = pushNameList_[i].size();
        write_socket(&name_len, sizeof(size_t));
        //std::cout << "      send pushNameList_[i].size() - done." << std::endl;

        char *property_name = new char[name_len+1]; // strcpy needs 1 more
        strcpy(property_name,const_cast<char*>(pushNameList_[i].c_str()));
        //std::cout << "      send property_name=" << property_name << " of len=" << name_len << std::endl;
        write_socket(property_name, name_len);
        //std::cout << "      send property_name=" << property_name << " of len=" << name_len << " done." << std::endl;

        //std::cout << "      send pushTypeList_[i].size()=" << pushTypeList_[i].size() << std::endl;
        size_t type_len = pushTypeList_[i].size();
        write_socket(&type_len, sizeof(size_t));
        //std::cout << "      send pushTypeList_[i].size() - done." << std::endl;

        char *property_type = new char[type_len+1]; // strcpy needs 1 more
        strcpy(property_type,const_cast<char*>(pushTypeList_[i].c_str()));
        write_socket(property_type, type_len);

        delete[] property_name;
        delete[] property_type;
    }
    //std::cout << "    send push (from DEM to CFD) names and types - done." << std::endl;

    // send number of pull (from CFD to DEM) properties
    //std::cout << "    send number of pull (from CFD to DEM) properties ..." << std::endl;
    h=pullNameList_.size();
    write_socket(&h, sizeof(size_t));
    //std::cout << "    send number of pull (from CFD to DEM) properties - done." << std::endl;

    // send pull (from CFD to DEM) names and types
    //std::cout << "    send pull (from CFD to DEM) names and types ..." << std::endl;
    for (int i = 0; i < h; i++)
    {
        size_t name_len = pullNameList_[i].size();
        write_socket(&name_len, sizeof(size_t));

        char *property_name = new char[name_len+1]; // strcpy needs 1 more
        strcpy(property_name,const_cast<char*>(pullNameList_[i].c_str()));
        write_socket(property_name, name_len);

        size_t type_len = pullTypeList_[i].size();
        write_socket(&type_len, sizeof(size_t));

        char *property_type = new char[type_len+1]; // strcpy needs 1 more
        strcpy(property_type,const_cast<char*>(pullTypeList_[i].c_str()));
        write_socket(property_type, type_len);

        delete[] property_name;
        delete[] property_type;
    }
    //std::cout << "    send pull (from CFD to DEM) names and types - done." << std::endl;
}

void Socket::buildBytePattern()
{
    pushBytesPerPropList_=std::vector<int>(pushTypeList_.size());
    pushCumOffsetPerProperty_=std::vector<int>(pushTypeList_.size());
    for (int i = 0; i < pushTypeList_.size(); i++)
    {
        if(pushTypeList_[i]=="scalar-atom"){
            pushBytesPerPropList_[i]=nbytesScalar_;
            rcvBytesPerParticle_+=pushBytesPerPropList_[i];
            //std::cout << " for property=" << pushNameList_[i] << ", of type="<< pushTypeList_[i] <<", we add " << pushBytesPerPropList_[i] << " bytes." << std::endl;
        }else if(pushTypeList_[i]=="vector-atom"){
            pushBytesPerPropList_[i]=nbytesVector_;
            rcvBytesPerParticle_+=pushBytesPerPropList_[i];
            //std::cout << " for property=" << pushNameList_[i] << ", of type="<< pushTypeList_[i] <<", we add " << pushBytesPerPropList_[i] << " bytes." << std::endl;
        }else if(pushTypeList_[i]=="scalar-multisphere"){
            pushBytesPerPropList_[i]=nbytesScalar_;
            rcvBytesPerParticle_+=pushBytesPerPropList_[i];
            //std::cout << " for property=" << pushNameList_[i] << ", of type="<< pushTypeList_[i] <<", we add " << pushBytesPerPropList_[i] << " bytes." << std::endl;
        }else if(pushTypeList_[i]=="vector-multisphere"){
            pushBytesPerPropList_[i]=nbytesVector_;
            rcvBytesPerParticle_+=pushBytesPerPropList_[i];
            //std::cout << " for property=" << pushNameList_[i] << ", of type="<< pushTypeList_[i] <<", we add " << pushBytesPerPropList_[i] << " bytes." << std::endl;
        }else
            error_one(std::string("\n\nERROR: Socket::buildBytePattern() (push): Type not recognized: ")+pushTypeList_[i]+std::string(".\n"));

        if(i>0) pushCumOffsetPerProperty_[i] = pushCumOffsetPerProperty_[i-1] + pushBytesPerPropList_[i-1];
        else pushCumOffsetPerProperty_[i] = 0;
    }
    //std::cout << "rcvBytesPerParticle_=" << rcvBytesPerParticle_ << std::endl;

    //==============================================================
    // TODO here we have code duplication

    pullBytesPerPropList_=std::vector<int>(pullTypeList_.size());
    pullCumOffsetPerProperty_=std::vector<int>(pullTypeList_.size());
    for (int i = 0; i < pullTypeList_.size(); i++)
    {
        if(pullTypeList_[i]=="scalar-atom"){
            pullBytesPerPropList_[i]=nbytesScalar_;
            sndBytesPerParticle_+=pullBytesPerPropList_[i];
        }else if(pullTypeList_[i]=="vector-atom"){
            pullBytesPerPropList_[i]=nbytesVector_;
            sndBytesPerParticle_+=pullBytesPerPropList_[i];
        }else if(pullTypeList_[i]=="scalar-multisphere"){
            pullBytesPerPropList_[i]=nbytesScalar_;
            sndBytesPerParticle_+=pullBytesPerPropList_[i];
        }else if(pullTypeList_[i]=="vector-multisphere"){
            pullBytesPerPropList_[i]=nbytesVector_;
            sndBytesPerParticle_+=pullBytesPerPropList_[i];
        }else
            error_one(std::string("\n\nERROR: Socket::buildBytePattern() (pull): Type not recognized: ")+pullTypeList_[i]+std::string(".\n"));

        if(i>0) pullCumOffsetPerProperty_[i] = pullCumOffsetPerProperty_[i-1] + pullBytesPerPropList_[i-1];
        else pullCumOffsetPerProperty_[i] = 0;
    }
    //std::cout << "sndBytesPerParticle_=" << sndBytesPerParticle_ << std::endl;
}

void Socket::exchangeStatus
(
    SocketCodes statusSend,
    SocketCodes statusExpect
)
{
    write_socket(&statusSend, sizeof(SocketCodes));
    SocketCodes LIG_msg = SocketCodes::invalid;
    read_socket(&LIG_msg, sizeof(SocketCodes));
    if (LIG_msg == SocketCodes::close_connection)
    {
        closeSocket();
        return;
    }
    else if (LIG_msg != statusExpect)
        error_one(std::string("\n\nERROR: Socket::exchangeStatus: Expected different status flag.\n"));
}

void Socket::exchangeDomain
(
    bool active,
    double* limits
)
{
    double bounds[6];
    for(int j=0;j<6;j++) bounds[j]=limits[j];

    SocketCodes msg;
    if(active)
    {
        msg = SocketCodes::bounding_box_update;
        write_socket(&msg, sizeof(SocketCodes));
        write_socket(&bounds, 6*sizeof(double));
        //std::cout << "sending bounds done.\n";
    }
    else
    {
        msg = SocketCodes::invalid;
        write_socket(&msg, sizeof(SocketCodes));
        std::cout << "not using bounds.\n";
    }
}

void Socket::rcvData
(
    size_t& dataSize,
    char*& data
)
{
    // read dataSize
    read_socket(&dataSize, sizeof(size_t));

    // read data
    data = new char[dataSize];
    read_socket(data, dataSize);
}

void Socket::sendData
(
    size_t& dataSize,
    char*& data
)
{
    // write dataSize
    write_socket(&dataSize, sizeof(size_t));

    // write data
    write_socket(data, dataSize);
}

void Socket::closeSocket()
{
    if (insockfd_ > 0)
        ::close(insockfd_);
    if (sockfd_ > 0)
        ::close(sockfd_);
}
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif
// ************************************************************************* //
