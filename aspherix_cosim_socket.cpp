/*---------------------------------------------------------------------------*\
    Aspherix-CoSimulation-Socket-Library

    (C) 2019 DCS Computing GmbH, Linz, Austria

    This software is released under the GNU GPL v3.
\*---------------------------------------------------------------------------*/

// this is not available on Windows
#ifndef _WIN32

#define __INCLUDE_PRIVATE_SOCKET__
#include "aspherix_cosim_socket.h"

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h> // needed for connect with timeout
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <mpi.h>
#include <numeric>
#include <vector>

static constexpr int kBasePort = 49152;
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

// Construct from components
AspherixCoSimSocket::AspherixCoSimSocket(Mode mode, const size_t processNumber,
                                         std::string customPortFilePath, int waitSeconds,
                                         int ntries_connect, bool verbose,
                                         bool keepPortOffsetFile) :
    sockfd_(0),
    insockfd_(0),
    mode_(mode),
    rcvBytesPerParticle_(0),
    sndBytesPerParticle_(0),
    push_field_list_(),
    pull_field_list_(),
    portRangeReserved_(1),
    waitSeconds_(1),
    ntries_connect_(10),
    port_(-1),
    verbose_(verbose),
    keepPortOffsetFile_(keepPortOffsetFile),
    portFileName_(""),
    processNumber_(processNumber)
{
    // create socket with DEM process
    if (processNumber == 0)
    {
        if (isServer())
        {
            printTime();
            std::cout << "Create socket with CFD process ..." << std::endl;
        }
        else
        {
            printTime();
            std::cout << "Create socket with DEM process ..." << std::endl;
        }
    }
    waitSeconds_ = waitSeconds;
    ntries_connect_ = ntries_connect;
    //==================================================
    // CHECK IF PORT FILE EXISTS AND READ IF IT DOES
    size_t portOffset(0);
    int foundPortFile(0);

    // determine the file path for the portOffset file
    // Problem: here we assume CFD and DEM live in their own directories and
    // both directories have the same mother directory
    // TODO: find a better solution (e.g. absolute file path and unique filename?)
    size_t size = 0;
    char* path = NULL;
    path = getcwd(path, size);
    std::string cwd = path;
    std::string portFilePath(cwd + "/" + customPortFilePath + "/portOffset_"
                             + std::to_string(processNumber) + ".txt");
    free(path);

    if (isServer())
    {
        // check if portfile exists and read it
        readPortFile(processNumber, portFilePath, portOffset, foundPortFile);

        if (foundPortFile == 0 && processNumber == 0)
        {
            std::cout << "\nDEM could not find portOffset file.\n"
                      << "   Auto-detecting available ports...\n"
                      << "*  Find details in the documentation (look for 'Setup a case using "
                         "socket communication').\n"
                      << std::endl;
        }
    }
    //==================================================

    port_ = kBasePort + processNumber + portOffset;

    // Creating socket file descriptor
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0)
        error_one("\n\nERROR: Socket creation failed");
    const std::string portBase_str = std::to_string(portBase);

    if (foundPortFile == 1)
    {
        if (portFileName_.empty())
            portFileName_ = portFilePath;
        printTime();
        std::cout << "Server: will forcefully attach to port " << std::to_string(port_) << "!"
                  << std::endl;
        int opt = 1;
        // Forcefully attaching socket to the port
        if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
            error_one("Failed setsockopt");
    }

    // connection will close immediately after closing your program;
    // and next restart will be able to bind again.
    linger lin;
    lin.l_onoff = 0;
    lin.l_linger = 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_LINGER, (const char*)&lin, sizeof(int));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    bool success = false;
    if (isServer())
    {
        int n_tries(0);
        int n_tries_max(
            std::min(100, 16000 / portRangeReserved_)); // go max from port 49152 to ~65535
        if (foundPortFile == 1)
            n_tries_max = 0;

        while (!success)
        {
            port_ = kBasePort + processNumber + portOffset;
            address.sin_port = htons(port_);
            success = false;
            n_tries++;

            if (verbose_)
            {
                printTime();
                std::cout << "Server: process number " << processNumber
                          << " trying to bind/listen with PORT(" + std::to_string(kBasePort)
                                 + "+ portOffset + procNr )="
                          << std::to_string(port_) << std::endl;
            }
            else if (processNumber == 0)
            {
                printTime();
                std::cout << "Server: trying to bind/listen" << std::endl;
            }

            // try attaching socket to the port
            if (bind(sockfd_, (struct sockaddr*)&address, sizeof(address)) < 0)
            {
                if (verbose_)
                {
                    printTime();
                    std::cout << "  process number " << processNumber << " Bind to "
                              << std::to_string(port_) << " failed." << std::endl;
                }

                if (n_tries > n_tries_max)
                {
                    printTime();
                    std::cout << "Server:  " << processNumber << " Bind to "
                              << std::to_string(port_)
                              << " failed (probably the port is not (yet?) available?)"
                              << std::endl;
                    break; // tried enough
                }
                portOffset += portRangeReserved_; // increase port by nProcs
            }
            else
            {
                if (verbose_)
                {
                    printTime();
                    std::cout << "  process number " << processNumber
                              << " Bind was successful with portOffset = " << portOffset
                              << std::endl;
                }
                else if (processNumber == 0)
                {
                    printTime();
                    std::cout << "Server: bind successful" << std::endl;
                }
                success = true;
            }
        }
        if (!success)
        {
            printTime();
            error_one("Server: Bind failed after all tries.");
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if (processNumber == 0)
        {
            printTime();
            std::cout << "Server: All processes bound successfully\n" << std::endl;
        }

        // if bind was successful, continue with listen
        if (listen(sockfd_, 5) < 0)
        {
            printTime();
            std::cout << "  process number " << processNumber << " Listen to "
                      << std::to_string(port_) << " failed." << std::endl;
        }
        else if (verbose_) // if listen was successful, communicate port to client
        {
            printTime();
            std::cout << "  process number " << processNumber << " Bind+Listen to "
                      << std::to_string(port_) << " successful" << std::endl;
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if (processNumber == 0)
        {
            printTime();
            std::cout << "Server: All processes listen successfully\n" << std::endl;
        }
    }

    // communicate suitable port with client via file
    if (isServer()) // server reads port from file if exists or writes suitable port to file
    {
        // only for auto port detection
        if (foundPortFile == 0)
        // if(success==1)
        {
            if (verbose_)
            {
                printTime();
                std::cout << "Server: write portOffset=" << portOffset << " to a file... "
                          << std::endl;
            }
            std::ofstream myfile2;
            myfile2.open(portFilePath);
            if (myfile2.is_open())
            {
                myfile2 << std::to_string(portOffset) << "\n";
                myfile2.close();
                portFileName_ = portFilePath;
            }
            else
            {
                printTime();
                error_one("Server: Unable to open file");
            }
        } // else error_one("ERROR");
    }
    else // client/server reads suitable port from file
    {
        // check if portfile exists and read it
        readPortFile(processNumber, portFilePath, portOffset, foundPortFile, ntries_connect_);

        if (!foundPortFile)
        {
            // check on local dir if portfile exists and read it

            std::string portFilePathOld(portFilePath);

            portFilePath = cwd + "/portOffset_" + std::to_string(processNumber) + ".txt";

            if (processNumber == 0)
            {
                printTime();
                std::cout << "Could not find portOffset files at: " << portFilePathOld
                          << "\nTrying alternative: " << portFilePath << "\n"
                          << std::endl;
            }

            readPortFile(processNumber, portFilePath, portOffset, foundPortFile, ntries_connect_);

            if (!foundPortFile)
            {
                if (processNumber == 0)
                {
                    printTime();
                    std::cout << "ERROR: CFD could not find portOffset file.\n"
                              << "   Probably there was no user defined portOffset "
                              << "   file and the DEM was not able to find suitable ports.\n"
                              << "*  Find details in the documentation "
                              << "   (look for 'Setup a case using socket communication').\n"
                              << std::endl;
                }
                error_one("FatalError: portOffset file not found.");
            }
        }
        port_ = kBasePort + processNumber + portOffset;
    }

    // server accept socket / client connect to socket
    if (isServer())
    {
        sleep(3);

        // test the socket with t/o before accept
        selectTO(sockfd_);

        // try accept
        if (verbose_)
        {
            printTime();
            std::cout << "Server: process number " << processNumber << " Try Accept..."
                      << std::endl;
        }
        socklen_t addrlen = sizeof(address);
        insockfd_ = accept(sockfd_, (struct sockaddr*)&address,
                           &addrlen); // waits for client to connect!!!
        // send(insockfd_, "1", 1, 0);
        if (insockfd_ < 0)
        {
            printTime();
            error_one("Server: Accept failed");
        }
        else if (verbose_)
        {
            printTime();
            std::cout << "Server: process number " << processNumber << " Accept successful."
                      << std::endl;
        }
        else if (processNumber == 0)
        {
            printTime();
            std::cout << "Server: Accept successful." << std::endl;
        }

        // connection will close immediately after closing your program;
        // and next restart will be able to bind again.
        setsockopt(insockfd_, SOL_SOCKET, SO_LINGER, (const char*)&lin, sizeof(int));
    }
    else // client implementation
    {
        if (verbose_)
        {
            printTime();
            std::cout << "Client: process number " << processNumber
                      << " trying to connect with PORT(" + std::to_string(kBasePort)
                             + " + portOffset + procNr)="
                      << std::to_string(port_) << std::endl;
        }
        else if (processNumber == 0)
        {
            printTime();
            std::cout << "Client: trying to connect with PORTS(" + std::to_string(kBasePort)
                             + " + portOffset + procNr)"
                      << std::endl;
        }

        address.sin_port = htons(port_);

        // trying connecton first
        // int result = tryConnect(address); // does not work?

        // test the socket with t/o before accept
        // selectTO(sockfd_);

        int ntries = 0;
        int ntry_max = ntries_connect_;
        while (connect(sockfd_, (struct sockaddr*)&address, sizeof(address)) < 0)
        {
            sleep(waitSeconds_);
            ntries++;
            if (ntries > ntry_max)
            {
                printTime();
                std::cout << "Client: " << processNumber << " Connecting to socket port "
                          << std::to_string(port_) << " failed. " << std::endl;
                std::cout
                    << "\nERROR: CFD could not connect to port.\n"
                    << "Probably the DEM run could not bind/connect to the port.\n"
                    << "*  Please make sure DEM was started as a separate run. Find details in the "
                       "documentation (look for 'Setup a case using socket communication').\n"
                    << "** Please make sure the DEM input script has a fix couple/cfd.\n"
                    << "*** If DEM was started separately & a fix couple/cfd is used, probably the "
                       "port it tried to use is not available. "
                    << "Please check the DEM logfile and try a different port (specified in "
                       "portOffset.txt).\n"
                    << std::endl;
                error_one("Connection Failed"); // std::cerr << "Connection Failed" << std::endl;
                                                // std::exit(1);
            }
            else if (verbose_)
            {
                printTime();
                std::cout << "Client: " << processNumber << " Connection attempt " << ntries << "/"
                          << ntry_max << ", waiting for timeOut=" << waitSeconds_ << "s"
                          << std::endl;
            }
            else if (processNumber == 0)
            {
                printTime();
                std::cout << "Client: Connection attempt " << ntries << "/" << ntry_max
                          << ", waiting for timeOut=" << waitSeconds_ << "s" << std::endl;
            }
        }
        // char buf[1];
        // recv(sockfd_, buf, 1, MSG_WAITFORONE);
        if (verbose_)
        {
            printTime();
            std::cout << "Client: process number " << processNumber << " Connection established."
                      << std::endl;
        }

        MPI_Barrier(MPI_COMM_WORLD);
        if (processNumber == 0)
        {
            printTime();
            std::cout << "Client: All processes connected successfully\n" << std::endl;
        }
    }

    // test connection
    // std::cout << "Server: process number " << processNumber << " testing connection
    // (read/write)..." << std::endl;
    SocketCodes test_connection_out = SocketCodes::welcome_client;
    if (isServer())
        test_connection_out = SocketCodes::welcome_server;
    SocketCodes test_connection_in = SocketCodes::invalid;
    writeSocket(test_connection_out);
    //    write_socket(&test_connection_out, sizeof(SocketCodes));
    //    read_socket(&test_connection_in, sizeof(SocketCodes));
    test_connection_in = readSocket<SocketCodes>();

    if (isServer())
    {
        if (test_connection_in != SocketCodes::welcome_client)
            error_one("Server: Connection test failed, wrong hello received from client");

        if (verbose_)
        {
            printTime();
            std::cout << "Server: process number " << processNumber
                      << " Socket connection established & tested on port " << std::to_string(port_)
                      << std::endl;
        }
        else if (processNumber == 0)
        {
            printTime();
            std::cout << "Server: Socket connection established & tested" << std::endl;
        }
    }
    else
    {
        if (test_connection_in != SocketCodes::welcome_server)
            error_one("Client: Connection test failed, wrong hello received from server");

        if (verbose_)
        {
            printTime();
            std::cout << "Client: process number " << processNumber_
                      << " Socket connection established & tested on port " << std::to_string(port_)
                      << std::endl;
        }
        else if (processNumber_ == 0)
        {
            printTime();
            std::cout << "Client: Socket connection established & tested" << std::endl;
        }
    }
}

// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //
AspherixCoSimSocket::~AspherixCoSimSocket()
{
    try
    {
        writeSocket(SocketCodes::close_connection);
        SocketCodes msg_received = readSocket<SocketCodes>();
        assert(msg_received == SocketCodes::close_connection);
        closeSocket();

        const std::string src_type = isServer() ? "Server" : "Client";

        if (verbose_)
        {
            printTime();
            std::cout << src_type + ": process number " << processNumber_
                      << " Socket connection closed on port " << std::to_string(port_) << std::endl;
        }
        else if (processNumber_ == 0)
        {
            printTime();
            std::cout << src_type + ": Socket connection closed" << std::endl;
        }
        /*
                SocketCodes msg = SocketCodes::close_connection;
                write_socket(&msg, sizeof(SocketCodes));
                read_socket(&msg, sizeof(SocketCodes));
                closeSocket();*/
    }
    catch (const std::runtime_error& re)
    {
        // NOTE: no need to manually closeConnection here, since error_ function already did so
        std::string other = "DEM";
        if (isServer())
            other = "CFD";
        if (processNumber_ == 0)
            std::cout << "Could not request closure of socket connection. " << other
                      << " side has already shut down. Closing regardless." << std::endl;
    }
}

// * * * * * * * * * * * * * * * private Member Functions  * * * * * * * * * * * * * //
void AspherixCoSimSocket::error_one(const std::string msg) const
{
    // sleep(10); // sleep so client has a chance to shut down first
    closeSocket();
    throw std::runtime_error(msg);
}

void AspherixCoSimSocket::error_all(const std::string msg) const
{
    closeSocket();
    throw std::runtime_error(msg);
}

size_t AspherixCoSimSocket::readNumberFromFile(const std::string path)
{
    size_t number(0);
    std::string line;
    std::ifstream myfile(path);
    int ntries = 0;
    sleep(1); // what if there is a file lying around and client reads before server has written?
              // //wait? // time stamp? // delete before and wait?
    while (!myfile.is_open())
    {
        sleep(1);
        ntries++;
        if (ntries > 10)
        {
            printTime();
            error_one(
                "AspherixCoSimSocket: Opening File Failed"); // std::cerr << "Opening File Failed"
                                                             // << std::endl; std::exit(1);
        }
        else if (verbose_)
        {
            printTime();
            std::cout << "Opening file attempt, path=" << path << " ntries=" << ntries << "/10"
                      << std::endl;
        }
        else if (processNumber_ == 0)
        {
            printTime();
            std::cout << "Opening file attempt, paths starting with " << path
                      << " ntries=" << ntries << "/10" << std::endl;
        }
    }
    while (std::getline(myfile, line))
        number = std::stoi(line);
    myfile.close();

    return number;
}

void AspherixCoSimSocket::deleteFile(const std::string path)
{
    if (remove(path.c_str()) != 0)
        std::cout << "Server: file" + path << " does not exist - nothing to do." << std::endl;
    else
        puts(("Server: File " + path + " successfully deleted.").c_str());
}

void AspherixCoSimSocket::readPortFile(int /*proc*/, const std::string path, size_t& port,
                                       int& found, int n_tries_max)
{
    if (verbose_)
    {
        printTime();
        std::cout << "   trying to read file " << path << "..." << std::endl;
    }
    else if (processNumber_ == 0)
    {
        printTime();
        std::cout << "   trying to read files starting with " << path << "..." << std::endl;
    }
    int success = 0;
    int n_tries = 0;
    while (success == 0)
    {
        n_tries++;
        if (std::ifstream(path)) // if file exists
        {
            found = 1;
            port = readNumberFromFile(path);
            if (verbose_)
            {
                printTime();
                std::cout << "   portOffset of this simulation run is read from file: portOffset="
                          << port << std::endl;
            }

            // sanity check of port
            if (port < 0)
                error_one("ERROR: AspherixCoSimSocket: please choose the port > 0");

            success = 1;
        }
        if (success == 0)
        {
            if (n_tries >= n_tries_max)
                break; // tried enough
            if (verbose_)
            {
                printTime();
                std::cout << "   process " << processNumber_
                          << " portOffset of this simulation could not be read attempt " << n_tries
                          << "/" << n_tries_max << ", waiting for timeOut=" << waitSeconds_ << "s"
                          << std::endl;
            }
            else if (processNumber_ == 0)
            {
                printTime();
                std::cout << "   portOffset of this simulation could not be read attempt "
                          << n_tries << "/" << n_tries_max
                          << ", waiting for timeOut=" << waitSeconds_ << "s" << std::endl;
            }
            sleep(waitSeconds_);
        }
    }
}

int AspherixCoSimSocket::tryConnect(struct sockaddr_in address)
{
    //=====================
    // test connect in non-blocking mode
    // connect with timeout (currently connected)
    // PROBLEM: program hangs if connect fails - so we want to "test" connect with a timeout
    // THIS CODE SNIPPET COMPILES BUT DOES NOT WORK AS DESIRED
    int res;
    long arg;
    // fd_set myset;
    // struct timeval tv;
    // int valopt;
    // socklen_t lon;

    // Set non-blocking
    if ((arg = fcntl(sockfd_, F_GETFL, NULL)) < 0)
    {
        fprintf(stderr, "AspherixCoSimSocket: Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
        exit(0);
    }
    arg |= O_NONBLOCK;
    if (fcntl(sockfd_, F_SETFL, arg) < 0)
    {
        fprintf(stderr, "AspherixCoSimSocket: Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
        exit(0);
    }
    // Trying to connect with timeout
    res = connect(sockfd_, (struct sockaddr*)&address, sizeof(address));
    if (res < 0)
    {
        if (errno == EINPROGRESS)
        {
            fprintf(stderr, "AspherixCoSimSocket: EINPROGRESS in connect()\n");

            /*// further tesing with timeout
            do
            {
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                FD_ZERO(&myset);
                FD_SET(sockfd_, &myset);
                res = select(sockfd_+1, NULL, &myset, NULL, &tv);
                if (res < 0 && errno != EINTR)
                {
                    fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
                    exit(0);
                }
                else if (res > 0)
                {
                    // Socket selected for write
                    lon = sizeof(int);
                    if (getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0)
                    {
                        fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno));
                        exit(0);
                    }
                    // Check the value returned...
                    if (valopt)
                    {
                        fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt,
            strerror(valopt)); exit(0);
                    }
                    break;
                }
                else
                {
                    fprintf(stderr, "Timeout in select() - Cancelling!\n");
                    exit(0);
                }
            } while (1);*/
        }
        else
        {
            fprintf(stderr, "AspherixCoSimSocket: Error connecting %d - %s\n", errno,
                    strerror(errno));
            exit(0);
        }
    }

    // Set to blocking mode again...
    if ((arg = fcntl(sockfd_, F_GETFL, NULL)) < 0)
    {
        fprintf(stderr, "AspherixCoSimSocket: Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
        exit(0);
    }
    arg &= (~O_NONBLOCK);
    if (fcntl(sockfd_, F_SETFL, arg) < 0)
    {
        fprintf(stderr, "AspherixCoSimSocket: Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
        exit(0);
    }

    return res;
}

void AspherixCoSimSocket::selectTO(int& sockfd)
{
    // use select to test the connection with a timeout
    fd_set sock;
    struct timeval tv;
    tv.tv_sec = 100;
    tv.tv_usec = 0;

    FD_ZERO(&sock);
    FD_SET(sockfd, &sock);

    int retval = select(sockfd + 1, &sock, NULL, NULL, &tv);

    // only if all processes successfully select we want to proceed
    int all_retval;
    MPI_Allreduce(&retval, &all_retval, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    retval = all_retval;

    if (retval <= 0)
    {
        printTime();
        error_one("Error: AspherixCoSimSocket::select: Server select() pre-connection socket test "
                  "failed.");
    }
    else if (verbose_)
    {
        printTime();
        std::cout << "AspherixCoSimSocket::select: process " << processNumber_
                  << " Server select() connection test successful." << std::endl;
    }
}

// * * * * * * * * * * * * * * * public Member Functions  * * * * * * * * * * * * * //
void AspherixCoSimSocket::write_socket(const void* const buf, const size_t size) const
{
    size_t send_size = 0;
    int cur_size(0);
    while (send_size < size)
    {
        if (isServer())
            cur_size = ::write(insockfd_, static_cast<const char*>(buf) + send_size,
                               size - send_size);
        else
            cur_size = ::write(sockfd_, static_cast<const char*>(buf) + send_size,
                               size - send_size);
        if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for sending data " << std::to_string(errno) << std::endl;
            else
                error_one("\n\nERROR: AspherixCoSimSocket::write_socket: Failed sending data.\n");
        }
        else if (cur_size == 0)
            error_one(std::string("\n\nERROR: AspherixCoSimSocket::write_socket: Disconnected. ")
                      + std::to_string(cur_size));

        send_size += cur_size;
    }
}

void AspherixCoSimSocket::read_socket(void* const buf, const size_t size) const
{
    size_t recv_size = 0;
    int cur_size(0);
    while (recv_size < size)
    {
        if (isServer())
            cur_size = ::read(insockfd_, static_cast<char*>(buf) + recv_size, size - recv_size);
        else
            cur_size = ::read(sockfd_, static_cast<char*>(buf) + recv_size, size - recv_size);
        if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for reading data " << std::to_string(errno) << std::endl;
            else
                error_one(std::string(
                              "\n\nERROR: AspherixCoSimSocket::read_socket: Failed getting data. ")
                          + std::to_string(cur_size));
        }
        else if (cur_size == 0)
            error_one(std::string("\n\nERROR: AspherixCoSimSocket::read_socket: Disconnected. ")
                      + std::to_string(cur_size));

        recv_size += cur_size;
    }
}

void AspherixCoSimSocket::sendProperties()
{
    // send number of push (from DEM to CFD) properties
    const auto nprops_push = writeFieldList(push_field_list_);
    std::cout << "    send number of push (from DEM to CFD) properties - done." << std::endl;

    // send number of push (from DEM to CFD) properties
    const auto nprops_pull = writeFieldList(pull_field_list_);
    std::cout << "    send number of pull (from DEM to CFD) properties - done." << std::endl;

    // send push (from DEM to CFD) names and types
    std::cout << "    send push (from DEM to CFD) names and types ..." << std::endl;
}

size_t AspherixCoSimSocket::writeFieldList(const std::vector<CoSimField>& field_list)
{
    const size_t nprops = field_list.size();
    write_socket(&nprops, sizeof(size_t));

    for (size_t i = 0; i < nprops; i++)
    {
        writeField(field_list[i]);
    }
    return nprops;
}

size_t AspherixCoSimSocket::recvProperties()
{
    // send number of push (from DEM to CFD) properties
    const auto nprops_push = readFieldList();
    std::cout << "    " << nprops_push << " push properties received" << std::endl;

    // send number of push (from DEM to CFD) properties
    const auto nprops_pull = readFieldList();
    std::cout << "    " << nprops_pull << " push properties received" << std::endl;

    return nprops_push + nprops_pull;
}

size_t AspherixCoSimSocket::readFieldList()
{
    size_t nprops = 0;
    read_socket(&nprops, sizeof(size_t));

    for (size_t i = 0; i < nprops; i++)
    {
        const auto field = readField();
        addField(field);
    }

    return nprops;
}

void AspherixCoSimSocket::buildBytePattern()
{
    const size_t nprops_push = push_field_list_.size();
    for (size_t i = 0; i < nprops_push; i++)
    {
        push_field_list_[i].setOffset(rcvBytesPerParticle_);
        rcvBytesPerParticle_ += push_field_list_[i].dataTypeSize();
    }

    const size_t nprops_pull = pull_field_list_.size();
    for (size_t i = 0; i < nprops_pull; i++)
    {
        push_field_list_[i].setOffset(sndBytesPerParticle_);
        sndBytesPerParticle_ += pull_field_list_[i].dataTypeSize();
    }
}

void AspherixCoSimSocket::exchangeStatus(SocketCodes statusSend, SocketCodes statusExpect)
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
        error_one(std::string(
            "\n\nERROR: AspherixCoSimSocket::exchangeStatus: Expected different status flag.\n"));
}

void AspherixCoSimSocket::exchangeDomain(bool active, double* limits)
{
    double bounds[6];
    for (int j = 0; j < 6; j++)
        bounds[j] = limits[j];

    SocketCodes msg;
    if (active)
    {
        msg = SocketCodes::bounding_box_update;
        write_socket(&msg, sizeof(SocketCodes));
        write_socket(&bounds, 6 * sizeof(double));
        // std::cout << "sending bounds done.\n";
    }
    else
    {
        msg = SocketCodes::invalid;
        write_socket(&msg, sizeof(SocketCodes));
        std::cout << "not using bounds.\n";
    }
}

std::vector<char> AspherixCoSimSocket::readData() const
{
    size_t vector_size;
    read_socket(&vector_size, sizeof(size_t));
    std::vector<char> byte_vector;
    byte_vector.reserve(vector_size);
    read_socket(byte_vector.data(), vector_size);
    return byte_vector;
}

void AspherixCoSimSocket::writeField(const CoSimField& field)
{
    const auto byte_vector = field.toByteVector();
    const auto vector_size = byte_vector.size();

    writeSocket(vector_size);
    writeSocket(byte_vector);
}

CoSimField AspherixCoSimSocket::readField()
{
    const size_t vector_size = readSocket<size_t>();
    std::vector<char> byte_vector;
    byte_vector = readSocket<std::vector<char>>(vector_size);
    return CoSimField(byte_vector);
}

void AspherixCoSimSocket::writeString(const std::string& str)
{
    const size_t length = str.size() + 1;
    write_socket(&length, sizeof(size_t));
    write_socket(str.c_str(), length);
}

std::string AspherixCoSimSocket::readString()
{
    size_t length;
    read_socket(&length, sizeof(size_t));
    char* byte_array = new char[length];
    read_socket(byte_array, length);
    const std::string result = byte_array;
    delete[] byte_array;
    return result;
}

void AspherixCoSimSocket::writeData(const size_t& dataSize, const char*& data)
{
    write_socket(&dataSize, sizeof(size_t));
    write_socket(data, dataSize);
}

void AspherixCoSimSocket::closeSocket() const
{
    if (insockfd_ > 0)
        ::close(insockfd_);
    if (sockfd_ > 0)
        ::close(sockfd_);

    if (isServer() && !keepPortOffsetFile_)
    {
        int success = remove(portFileName_.c_str());
        if (success != 0)
            std::cout << "Warning: portFile could not be deleted.\n";
    }
}

void AspherixCoSimSocket::printTime()
{
    std::time_t curT;
    struct std::tm* locTime;
    std::time(&curT);
    locTime = std::localtime(&curT);
    if (locTime)
        printf("[%02d:%02d:%02d] ", locTime->tm_hour, locTime->tm_min, locTime->tm_sec);
    else
        fprintf(stderr, "Local time not available");
}
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif
// ************************************************************************* //
