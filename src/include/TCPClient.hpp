#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <iostream>
#include <thread>
#include <fstream>
#include <atomic>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>

inline std::atomic_bool TCPSocketRunning = false;

class TCPClient
{
public:
    TCPClient();
    ~TCPClient();
    void createSocket();
    void bindAddress(const std::string &ip, uint16_t port);
    void connectServer(const std::string &ip, uint16_t port);
    void sendFile(const std::string &ip, uint16_t port, const std::string &filePath);
private:
    int sockfd;                     
    struct sockaddr_in serverAddr;
};

void TCPThread();

#endif