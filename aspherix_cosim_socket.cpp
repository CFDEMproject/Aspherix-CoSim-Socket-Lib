/*---------------------------------------------------------------------------*\
    Aspherix-CoSimulation-Socket-Library

    (C) 2019 DCS Computing GmbH, Linz, Austria

    This software is released under the GNU GPL v3.
\*---------------------------------------------------------------------------*/

// this is not available on Windows
#include <string>
#ifndef _WIN32

#include "aspherix_cosim_socket.h"

#include <arpa/inet.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h> // needed for connect with timeout
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <tuple>
#include <unistd.h>

namespace CoSimSocket
{

class AspherixCoSimSocket::Impl {
public:
    Impl(AspherixCoSimSocket& owner) :
        owner_(owner)
    {
    }

    void writeData(const std::span<char> data)
    {
        const std::size_t size = data.size();
        owner_.write_socket(&size, sizeof(std::size_t));
        if (size > 0)
        {
            owner_.write_socket(data.data(), size);
        }
    }

    void writeData(const std::span<double> data)
    {
        const std::size_t size = data.size();
        owner_.write_socket(&size, sizeof(std::size_t));
        if (size > 0)
        {
            owner_.write_socket(data.data(), size * sizeof(double) / sizeof(char));
        }
    }

    void writeData(const std::span<int> data)
    {
        const std::size_t size = data.size();
        owner_.write_socket(&size, sizeof(std::size_t));
        if (size > 0)
        {
            owner_.write_socket(data.data(), size * sizeof(double) / sizeof(int));
        }
    }

    void writeData(const std::span<std::size_t> data)
    {
        const std::size_t size = data.size();
        owner_.write_socket(&size, sizeof(std::size_t));
        if (size > 0)
        {
            owner_.write_socket(data.data(), size * sizeof(double) / sizeof(std::size_t));
        }
    }

private:
    AspherixCoSimSocket& owner_;
};

AspherixCoSimSocket::AspherixCoSimSocket(const Mode& mode, std::size_t process_number,
                                         const std::string& custom_port_file_path,
                                         const std::size_t base_port, int wait_seconds,
                                         const std::size_t ntries_connect, bool verbose,
                                         bool keep_port_offset_file) :
    pimpl_(std::make_unique<Impl>(*this)),
    sockfd_(0),
    insockfd_(0),
    mode_(mode),
    portRangeReserved_(1),
    wait_seconds_(wait_seconds),
    ntries_connect_(ntries_connect),
    base_port_(base_port),
    port_(-1),
    verbose_(verbose),
    keepPortOffsetFile_(keep_port_offset_file),
    process_number_(process_number),
    status_(SocketStatus::kInactive)
{
    if (process_number == 0)
    {
        if (isServer())
        {
            printTime();
            std::cout << "Create socket on server for client process ..." << '\n';
        }
        else
        {
            printTime();
            std::cout << "Create socket on client for server process ..." << '\n';
        }
    }
    wait_seconds_ = wait_seconds;
    ntries_connect_ = ntries_connect;
    //==================================================
    // CHECK IF PORT FILE EXISTS AND READ IF IT DOES
    std::size_t port_offset = 0;
    bool found_port_file = false;

    // determine the file path for the port_offset file
    // Problem: here we assume CFD and DEM live in their own directories and
    // both directories have the same mother directory
    // TODO: find a better solution (e.g. absolute file path and unique filename?)
    std::size_t size = 0;
    std::string cwd = std::filesystem::current_path().string();
    std::string port_file_path = cwd + "/" + custom_port_file_path + "/port_offset_"
                                 + std::to_string(process_number) + ".txt";

    if (isServer())
    {
        if (keepPortOffsetFile_)
        {
            std::tie(port_offset, found_port_file) = readPortFile(port_file_path);

            if (found_port_file)
            {
                if (portFileName_.empty())
                {
                    portFileName_ = port_file_path;
                }
                printTime();
                std::cout << "Server: will forcefully attach to port " << std::to_string(port_)
                          << "!" << '\n';
                int opt = 1;
                // Forcefully attaching socket to the port
                if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0)
                {
                    error("Failed setsockopt");
                }
            }
            else if (!found_port_file && process_number == 0)
            {
                std::cout << "\nDEM could not find port_offset file.\n"
                          << "   Auto-detecting available ports...\n"
                          << "*  Find details in the documentation (look for 'Setup a case using "
                             "socket communication').\n"
                          << '\n';
            }
        }
        else if (std::filesystem::exists(port_file_path))
        {
            std::filesystem::remove(port_file_path);
        }
    }
    //==================================================

    port_ = base_port_ + process_number + port_offset;

    // Creating socket file descriptor
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0)
        error("\n\nERROR: Socket creation failed");

    mutually_closed_sockets_ = false;

    // connection will close immediately after closing your program;
    // and next restart will be able to bind again.
    linger lin{};
    lin.l_onoff = 1;
    lin.l_linger = 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin));

    struct ::sockaddr_in address{};
    memset(&address, 0, sizeof(::sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    bool success = false;
    if (isServer())
    {
        int n_tries(0);
        int number_of_attempts =
            std::min(100, 16000 / portRangeReserved_); // go max from port 49152 to ~65535
        if (found_port_file)
        {
            number_of_attempts = 0;
        }

        while (!success)
        {
            port_ = base_port_ + process_number + port_offset;
            address.sin_port = htons(port_);
            success = false;
            n_tries++;

            if (verbose_)
            {
                printTime();
                std::cout << "Server: process number " << process_number
                          << " trying to bind/listen with port " << std::to_string(port_) << " ("
                          << std::to_string(base_port_) << "+ port_offset + procNr)\n";
            }
            else if (process_number == 0)
            {
                printTime();
                std::cout << "Server: trying to bind/listen" << '\n';
            }

            // try attaching socket to the port
            if (bind(sockfd_, (struct sockaddr*)&address, sizeof(address)) < 0)
            {
                if (verbose_)
                {
                    printTime();
                    std::cout << "Server: process number " << process_number << " Bind to "
                              << std::to_string(port_) << " failed." << '\n';
                }

                if (n_tries > number_of_attempts)
                {
                    printTime();
                    std::cout << "Server:  " << process_number << " Bind to "
                              << std::to_string(port_)
                              << " failed (probably the port is not (yet?) available?)" << '\n';
                    break; // tried enough
                }
                port_offset += portRangeReserved_; // increase port by nProcs
            }
            else
            {
                if (verbose_)
                {
                    printTime();
                    std::cout << "Server: process number " << process_number
                              << " Bind was successful with port_offset = " << port_offset << '\n';
                }
                else if (process_number == 0)
                {
                    printTime();
                    // std::cout << "\033[31mred text\033[0m\n";
                    std::cout << "Server: bind successful" << '\n';
                }
                success = true;
            }
        }
        if (!success)
        {
            printTime();
            error("Server: Bind failed after all tries.");
        }

        MPI_Barrier(MPI_COMM_WORLD);

        if (process_number == 0)
        {
            printTime();
            std::cout << "Server: All processes bound successfully\n" << '\n';
        }

        // if bind was successful, continue with listen
        if (listen(sockfd_, 5) < 0)
        {
            printTime();
            std::cout << "Server: process number " << process_number << " Listen to "
                      << std::to_string(port_) << " failed." << '\n';
        }
        else if (verbose_) // if listen was successful, communicate port to client
        {
            printTime();
            std::cout << "Server: process number " << process_number << " Bind+Listen to "
                      << std::to_string(port_) << " successful" << '\n';
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if (process_number == 0)
        {
            printTime();
            std::cout << "Server: All processes listen successfully\n" << '\n';
        }
    }

    // communicate suitable port with client via file
    if (isServer()) // server reads port from file if exists or writes suitable port to file
    {
        // only for auto port detection
        if (!found_port_file)
        {
            writePortFile(port_file_path, port_offset);
        } // else error("ERROR");
    }
    else // client/server reads suitable port from file
    {
        // check if portfile exists and read it
        std::tie(port_offset, found_port_file) = readPortFile(port_file_path, ntries_connect_);

        if (!found_port_file)
        {
            // check on local dir if portfile exists and read it
            std::string port_file_path_old = port_file_path;

            port_file_path = cwd + "/port_offset_" + std::to_string(process_number) + ".txt";

            if (process_number == 0)
            {
                printTime();
                std::cout << "Could not find port_offset files at: " << port_file_path_old
                          << "\nTrying alternative: " << port_file_path << "\n"
                          << '\n';
            }

            std::tie(port_offset, found_port_file) = readPortFile(port_file_path, ntries_connect_);

            if (!found_port_file)
            {
                if (process_number == 0)
                {
                    printTime();
                    std::cout << "ERROR: CFD could not find port_offset file.\n"
                              << "   Probably there was no user defined port_offset "
                              << "   file and the DEM was not able to find suitable ports.\n"
                              << "*  Find details in the documentation "
                              << "   (look for 'Setup a case using socket communication').\n"
                              << '\n';
                }
                error("FatalError: port_offset file not found.");
            }
        }
        port_ = base_port_ + process_number + port_offset;
    }

    // server accept socket / client connect to socket
    if (isServer())
    {
        // sleep(3);
        sleep(wait_seconds_);

        // test the socket with t/o before accept
        selectTO(sockfd_);

        // try accept
        if (verbose_)
        {
            printTime();
            std::cout << "Server: process number " << process_number << " Try Accept..." << '\n';
        }
        socklen_t addrlen = sizeof(address);
        insockfd_ = accept(sockfd_, (struct sockaddr*)&address,
                           &addrlen); // waits for client to connect!!!
        // send(insockfd_, "1", 1, 0);
        if (insockfd_ < 0)
        {
            printTime();
            error("Server: Accept failed");
        }
        else if (verbose_)
        {
            printTime();
            std::cout << "Server: process number " << process_number << " Accept successful."
                      << '\n';
        }
        else if (process_number == 0)
        {
            printTime();
            std::cout << "Server: Accept successful." << '\n';
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
            std::cout << "Client: process number " << process_number
                      << " trying to connect with port " << std::to_string(port_) << " ("
                      << std::to_string(base_port_) << " + port_offset + proces_number)\n";
        }
        else if (process_number == 0)
        {
            printTime();
            std::cout << "Client: trying to connect with port " << std::to_string(port_) << " ("
                      << std::to_string(base_port_) << " + port_offset + proces_number)\n";
        }

        address.sin_port = htons(port_);

        int ntries = 0;
        int ntry_max = ntries_connect_;
        while (connect(sockfd_, (struct sockaddr*)&address, sizeof(address)) < 0)
        {
            sleep(wait_seconds_);
            ntries++;
            if (ntries > ntry_max)
            {
                printTime();
                std::cout << "Client: " << process_number << " Connecting to socket port "
                          << std::to_string(port_) << " failed. " << '\n';
                std::cout
                    << "\nERROR: CFD could not connect to port.\n"
                    << "Probably the DEM run could not bind/connect to the port.\n"
                    << "*  Please make sure DEM was started as a separate run. Find details in "
                       "the "
                       "documentation (look for 'Setup a case using socket communication').\n"
                    << "** Please make sure the DEM input script has a fix couple/cfd.\n"
                    << "*** If DEM was started separately & a fix couple/cfd is used, probably "
                       "the "
                       "port it tried to use is not available. "
                    << "Please check the DEM logfile and try a different port (specified in "
                       "port_offset.txt).\n"
                    << '\n';
                error("Connection Failed"); // std::cerr << "Connection Failed" <<
                                            // '\n'; std::exit(1);
            }
            else if (verbose_)
            {
                printTime();
                std::cout << "Client: " << process_number << " Connection attempt " << ntries << "/"
                          << ntry_max << ", waiting for timeOut=" << wait_seconds_ << "s" << '\n';
            }
            else if (process_number == 0)
            {
                printTime();
                std::cout << "Client: Connection attempt " << ntries << "/" << ntry_max
                          << ", waiting for timeOut=" << wait_seconds_ << "s" << '\n';
            }
        }
        if (verbose_)
        {
            printTime();
            std::cout << "Client: process number " << process_number << " Connection established."
                      << '\n';
        }

        MPI_Barrier(MPI_COMM_WORLD);
        if (process_number == 0)
        {
            printTime();
            std::cout << "Client: All processes connected successfully\n" << '\n';
        }
    }

    SocketCodes test_connection_in = exchangeStatus(SocketCodes::kWelcome);
    status_ = SocketStatus::kActive;

    if (isServer())
    {
        if (test_connection_in != SocketCodes::kWelcome)
            error("Server: Connection test failed, wrong hello received from client");

        if (verbose_)
        {
            printTime();
            std::cout << "Server: process number " << process_number
                      << " Socket connection established & tested on port " << std::to_string(port_)
                      << '\n';
        }
        else if (process_number == 0)
        {
            printTime();
            std::cout << "Server: Socket connection established & tested" << '\n';
        }
    }
    else
    {
        if (test_connection_in != SocketCodes::kWelcome)
            error("Client: Connection test failed, wrong hello received from server");

        if (verbose_)
        {
            printTime();
            std::cout << "Client: process number " << process_number_
                      << " Socket connection established & tested on port " << std::to_string(port_)
                      << '\n';
        }
        else if (process_number_ == 0)
        {
            printTime();
            std::cout << "Client: Socket connection established & tested" << '\n';
        }
    }
}

// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //
AspherixCoSimSocket::~AspherixCoSimSocket()
{
    if (status_ == SocketStatus::kActive)
    {
        try
        {
            closeSocket(true);
        }
        catch (const std::runtime_error& re)
        {
            // no need to manually closeConnection here, since error_ function already did so
            const std::string other = isServer() ? "CFD" : "DEM";
            if (process_number_ == 0)
            {
                std::cout << "Could not request closure of socket connection. " << other
                          << " side has already shut down. Closing regardless." << '\n';
            }
        }
    }
}

// * * * * * * * * * * * * * * * private Member Functions  * * * * * * * * * * * * * //
void AspherixCoSimSocket::error(const std::string& msg)
{
    closeSocket(false);
    throw std::runtime_error(msg);
}

std::size_t AspherixCoSimSocket::readNumberFromFile(const std::string& path,
                                                    const std::size_t max_attempts)
{
    std::size_t number = 0;
    std::string line;
    std::ifstream myfile(path);
    std::size_t ntries = 0;
    sleep(wait_seconds_);
    while (!myfile.is_open())
    {
        sleep(wait_seconds_);
        ntries++;
        if (ntries > max_attempts)
        {
            printTime();
            error("AspherixCoSimSocket: Opening File Failed"); // std::cerr << "Opening File
                                                               // Failed"
                                                               // << '\n'; std::exit(1);
        }
        else if (verbose_)
        {
            printTime();
            std::cout << "Opening file attempt, path=" << path << " ntries=" << ntries << "/10"
                      << '\n';
        }
        else if (process_number_ == 0)
        {
            printTime();
            std::cout << "Opening file attempt, paths starting with " << path
                      << " ntries=" << ntries << "/10" << '\n';
        }
    }
    while (std::getline(myfile, line))
    {
        number = std::stoi(line);
    }
    myfile.close();

    return number;
}

void AspherixCoSimSocket::deletePortFile() const
{
    std::string message;
    if (isServer())
    {
        if (keepPortOffsetFile_)
        {
            message = "Server: the portFile will be kept.";
        }
        else
        {
            if (remove(portFileName_.c_str()) != 0)
            {
                message = "Server: file '" + portFileName_ + "' does not exist - nothing to do.";
            }
            else
            {
                message = "Server: file '" + portFileName_ + "' successfully deleted.";
            }
        }
    }
    else if (!isServer())
    {
        message = "Client: only the server can remove the port file.";
    }
    if (verbose_)
    {
        printTime();
        std::cout << message << "\n";
    }
}

void AspherixCoSimSocket::writePortFile(const std::string& port_file_path, std::size_t port_offset)
{
    if (verbose_)
    {
        printTime();
        std::cout << "Server: write port_offset of " << port_offset << " to " << port_file_path
                  << '\n';
    }
    std::ofstream port_offset_file;
    port_offset_file.open(port_file_path);
    if (port_offset_file.is_open())
    {
        port_offset_file << std::to_string(port_offset) << "\n";
        port_offset_file.close();
        portFileName_ = port_file_path;
    }
    else
    {
        printTime();
        error("Server: Unable to open file");
    }
}

std::pair<std::size_t, bool> AspherixCoSimSocket::readPortFile(const std::string& path,
                                                               const std::size_t number_of_attempts)
{
    const std::string mode = isServer() ? "Server" : "Client";
    if (verbose_)
    {
        printTime();
        std::cout << mode << ": trying to read file " << path << "..." << '\n';
    }
    else if (process_number_ == 0)
    {
        printTime();
        std::cout << mode << ": trying to read files starting with " << path << "..." << '\n';
    }

    std::size_t port_offset = 0;
    bool success = false;
    std::size_t n_tries = 0;
    while (!success)
    {
        n_tries++;
        if (std::ifstream(path)) // if file exists
        {
            port_offset = readNumberFromFile(path, number_of_attempts);
            if (verbose_)
            {
                printTime();
                std::cout
                    << mode
                    << ": port_offset of this simulation run is read from file: port_offset = "
                    << port_offset << '\n';
            }

            // sanity check of port
            if (port_offset < 0)
                error("ERROR: AspherixCoSimSocket: please choose the port > 0");

            success = true;
        }
        if (!success)
        {
            if (n_tries >= number_of_attempts)
            {
                break;
            }
            if (verbose_)
            {
                printTime();
                std::cout << mode << ": process " << process_number_
                          << " port_offset of this simulation could not be read attempt " << n_tries
                          << "/" << number_of_attempts << ", waiting for " << wait_seconds_ << "s"
                          << '\n';
            }
            else if (process_number_ == 0)
            {
                printTime();
                std::cout << mode << ": port_offset of this simulation could not be read attempt "
                          << n_tries << "/" << number_of_attempts << ", waiting for "
                          << wait_seconds_ << "s" << '\n';
            }
            sleep(wait_seconds_);
        }
    }

    return {port_offset, success};
}

void AspherixCoSimSocket::selectTO(int& sockfd)
{
    // use select to test the connection with a timeout
    fd_set sock;
    struct timeval tv_struct{};
    tv_struct.tv_sec = 100;
    tv_struct.tv_usec = 0;

    FD_ZERO(&sock);
    FD_SET(sockfd, &sock);

    int retval = select(sockfd + 1, &sock, nullptr, nullptr, &tv_struct);

    // only if all processes successfully select we want to proceed
    int all_retval;
    MPI_Allreduce(&retval, &all_retval, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    retval = all_retval;

    if (retval <= 0)
    {
        printTime();
        error("Error: AspherixCoSimSocket::select: Server select() pre-connection socket test "
              "failed.");
    }
    else if (verbose_)
    {
        printTime();
        std::cout << "AspherixCoSimSocket::select: process " << process_number_
                  << " Server select() connection test successful." << '\n';
    }
}

// * * * * * * * * * * * * * * * public Member Functions  * * * * * * * * * * * * * //
void AspherixCoSimSocket::write_socket(const void* const buf, const std::size_t size)
{
    assert(buf != nullptr);

    std::size_t send_size = 0;
    int cur_size(0);
    while (send_size < size)
    {
        if (isServer())
        {
            cur_size = ::write(insockfd_, static_cast<const char*>(buf) + send_size,
                               size - send_size);
        }
        else
        {
            cur_size = ::write(sockfd_, static_cast<const char*>(buf) + send_size,
                               size - send_size);
        }
        if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::cout << "Waiting for sending data " << std::string(strerror(errno)) << '\n';
            }
            else
            {
                const std::string msg =
                    "\n\nERROR: AspherixCoSimSocket::write_socket: Failed sending data ("
                    + std::string(strerror(errno)) + ").\n";
                error(msg);
            }
        }
        else if (cur_size == 0)
        {
            error(std::string("\n\nERROR: AspherixCoSimSocket::write_socket: Disconnected. ")
                  + std::to_string(cur_size));
        }

        send_size += cur_size;
    }
}

void AspherixCoSimSocket::read_socket(void* const buf, const std::size_t size)
{
    std::size_t recv_size = 0;
    std::size_t cur_size = 0;
    while (recv_size < size)
    {
        if (isServer())
        {
            cur_size = ::read(insockfd_, static_cast<char*>(buf) + recv_size, size - recv_size);
        }
        else
        {
            cur_size = ::read(sockfd_, static_cast<char*>(buf) + recv_size, size - recv_size);
        }
        if (cur_size < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                std::cout << "Waiting for reading data " << std::to_string(errno) << '\n';
            else
            {
                error(std::string(
                          "\n\nERROR: AspherixCoSimSocket::read_socket: Failed getting data. ")
                      + std::to_string(cur_size));
            }
        }
        else if (cur_size == 0)
        {
            error(std::string("\n\nERROR: AspherixCoSimSocket::read_socket: Disconnected. ")
                  + std::to_string(cur_size));
        }

        recv_size += cur_size;
    }
}

SocketCodes AspherixCoSimSocket::exchangeStatus(SocketCodes status_send, SocketCodes status_expect)
{
    write_socket(&status_send, sizeof(SocketCodes));
    SocketCodes status_recv = SocketCodes::kInvalid;
    read_socket(&status_recv, sizeof(SocketCodes));
    if (status_recv == SocketCodes::kCloseConnection)
    {
        closeSocket(false);
        return SocketCodes::kCloseConnection;
    }
    if (status_expect != SocketCodes::kUndefined && status_recv != status_expect)
    {
        std::string msg = std::string(
                              "FatalError: the exchanged socket codes do not match. (received: ")
                          + std::to_string(static_cast<int>(status_recv))
                          + " but expect: " + std::to_string(static_cast<int>(status_expect)) + ")";
        error(msg);
    }
    return status_recv;
}

void AspherixCoSimSocket::writeData(const std::size_t dataSize, char* const& data)
{
    pimpl_->writeData(std::span<char>(data, dataSize));
}

void AspherixCoSimSocket::writeData(const std::size_t dataSize, double* const& data)
{
    pimpl_->writeData(std::span<double>(data, dataSize));
}

void AspherixCoSimSocket::writeData(const std::size_t dataSize, int* const& data)
{
    pimpl_->writeData(std::span<int>(data, dataSize));
}

void AspherixCoSimSocket::writeData(const std::size_t dataSize, std::size_t* const& data)
{
    pimpl_->writeData(std::span<std::size_t>(data, dataSize));
}

void AspherixCoSimSocket::writeString(const std::string& str)
{
    const std::size_t length = str.size() + 1;
    write_socket(&length, sizeof(std::size_t));
    write_socket(str.c_str(), length);
}

std::string AspherixCoSimSocket::readString()
{
    std::size_t length = 0;
    read_socket(&length, sizeof(std::size_t));
    std::vector<char> byte_array(length);
    read_socket(byte_array.data(), length);
    return {byte_array.data()};
}

void AspherixCoSimSocket::closeSocket(const bool mutual)
{
    if (mutual)
    {
        SocketCodes msg_send = SocketCodes::kCloseConnection;
        write_socket(&msg_send, sizeof(SocketCodes));
        const std::string src = isServer() ? "server" : "client";
        SocketCodes msg_recv = SocketCodes::kInvalid;
        read_socket(&msg_recv, sizeof(SocketCodes));
        assert(msg_recv == SocketCodes::kCloseConnection);
    }

    if (insockfd_ > 0)
        ::close(insockfd_);
    if (sockfd_ > 0)
        ::close(sockfd_);

    status_ = SocketStatus::kInactive;

    if (mutual)
    {
        const std::string src_type = isServer() ? "Server" : "Client";

        if (verbose_)
        {
            printTime();
            std::cout << src_type + ": process number " << process_number_
                      << " Socket connection closed on port " << std::to_string(port_)
                      << " (mutually)" << '\n';
        }
        else if (process_number_ == 0)
        {
            printTime();
            std::cout << src_type + ": Socket connection closed (mutually)" << '\n';
        }
    }

    deletePortFile();
}

void AspherixCoSimSocket::showBufferSizeInfo()
{
    socklen_t optlen = sizeof(int);
    int sndbuf_size = 0;
    if (getsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, &optlen) < 0)
    {
        error("getsockopt SO_SNDBUF failed");
    }
    else
    {
        std::cout << "Default send buffer size: " << std::to_string(sndbuf_size) << " bytes\n";
    }

    int rcvbuf_size = 0;
    if (getsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, &optlen) < 0)
    {
        error("getsockopt SO_RCVBUF failed");
    }
    else
    {
        std::cout << "Default receive buffer size: " << std::to_string(rcvbuf_size) << " bytes\n";
    }
}

void AspherixCoSimSocket::printTime() const
{
    std::time_t cur_t = 0;
    struct std::tm* loc_time = nullptr;
    std::time(&cur_t);
    loc_time = std::localtime(&cur_t);
    if (loc_time != nullptr)
    {
#if __cplusplus >= 202002L
        std::cout << std::format("[{:02}:{:02}:{:02}] ", loc_time->tm_hour, loc_time->tm_min,
                                 loc_time->tm_sec);
#else
        std::cout << '[' << std::setw(2) << std::setfill('0') << loc_time->tm_hour << ':'
                  << std::setw(2) << std::setfill('0') << loc_time->tm_min << ':' << std::setw(2)
                  << std::setfill('0') << loc_time->tm_sec << "] ";
#endif
    }
    else
        std::cerr << "Local time not available";
}

} // namespace CoSimSocket

#endif
