#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    
    #if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
        using ssize_t = SSIZE_T;
        #define _SSIZE_T_DEFINED
    #endif
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

// Fallback for compilers (like MinGW/GCC on Windows) that don't define ssize_t
#ifndef _SSIZE_T_DEFINED
    #if defined(_WIN32) || defined(_WIN64)
        #include <basetsd.h>
        typedef SSIZE_T ssize_t;
        #define _SSIZE_T_DEFINED
    #endif
#endif

#include <vector>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
using namespace std;

vector<uint8_t> readExactBytes(int socket_fd, size_t numBytes) {
    vector<uint8_t> buffer(numBytes);
    size_t totalRead = 0;
    
    while(numBytes > totalRead)
    {
        size_t remRead = numBytes - totalRead;
        uint8_t* targetPointer = buffer.data() + totalRead;

        ssize_t bytesRead = ::recv (
            socket_fd,
            reinterpret_cast<char*>(targetPointer),
            remRead,
            0
        );

        if(bytesRead > 0)
            totalRead += static_cast<size_t>(bytesRead);

        else if(bytesRead == 0)
            throw runtime_error("Connection has been closed by peer prematurely.");

        else
        {
            #if defined(_WIN32) || defined(_WIN64)
                if (WSAGetLastError() == WSAEINTR)
                    continue;
                    
                throw runtime_error("Socket read error occurred: " + to_string(WSAGetLastError()));
            #else
                if (errno == EINTR)
                    continue;

                throw runtime_error("Socket read error occurred: " + to_string(errno));
            #endif
        } 
    }

    return buffer;
}

void sendAll(int socket_fd, const vector<uint8_t>& data)
{
    size_t totalSent = 0;

    while(totalSent < data.size())
    {
        size_t remSend = data.size() - totalSent;
        const uint8_t *targetPointer = data.data() + totalSent;

        ssize_t sent = ::send(
            socket_fd,
            reinterpret_cast<const char*>(targetPointer),
            remSend,
            0
        );

        if(sent > 0)
            totalSent += static_cast<size_t>(sent);

        else if(sent == 0)
            throw runtime_error("Connection has been closed by peer prematurely.");

        else
        {
            #if defined(_WIN32) || defined(_WIN64)    
                if(WSAGetLastError() == WSAEINTR)
                    continue;

                throw runtime_error("Socket write error occured." + to_string(WSAGetLastError())); 
            #else
                if(errno == EINTR)
                    continue;

                throw runtime_error("Socket write error occured." + to_string(errno)); 
            #endif
        }
    }
}