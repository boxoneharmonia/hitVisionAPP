#ifndef UDP_CLIENT_HPP
#define UDP_CLIENT_HPP

#include <iostream>
#include <thread>
#include <fstream>
#include <atomic>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>

inline std::atomic_bool UDPSocketRunning = false;

class UDPClient
{
public:
    UDPClient();
    ~UDPClient();
    void createSocket();
    void bindAddress(const std::string &ip, uint16_t port);
    void connectAddress(const std::string &ip, uint16_t port);
    void sendFile(const std::string &ip, uint16_t port, const std::string &filePath);
private:
    int sockfd;                     
    struct sockaddr_in serverAddr;
};

void UDPThread();

#endif