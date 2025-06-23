#include "TCPClient.hpp"
#include "file.hpp"
#include "global_state.hpp"
using namespace std;
#define CHUNK_SIZE 1024

TCPClient::TCPClient()
{
    createSocket();
}

TCPClient::~TCPClient()
{
    if (sockfd >= 0)
    {
        close(sockfd);
    }
}

void TCPClient::createSocket()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Failed to create TCP socket");
        exit(EXIT_FAILURE);
    }
}

void TCPClient::bindAddress(const string &ip, uint16_t port)
{
    struct sockaddr_in localAddr{};
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(port);
    localAddr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (bind(sockfd, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
}

void TCPClient::connectServer(const string &ip, uint16_t port)
{
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0)
    {
        perror("Invalid address");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("TCP connect failed");
        exit(EXIT_FAILURE);
    }
}

void TCPClient::sendFile(const string &ip, uint16_t port, const string &filePath)
{
    ifstream file(filePath, ios::binary);
    if (!file.is_open())
    {
        cout << "Failed to open file: " << filePath << endl;
        return;
    }

    // connectServer(ip, port);

    const size_t bufferSize = CHUNK_SIZE;
    char sendbuffer[bufferSize];

    while (file.read(sendbuffer, bufferSize) || file.gcount() > 0)
    {
        size_t bytesToSend = file.gcount();
        ssize_t sentBytes = send(sockfd, sendbuffer, bytesToSend, 0);
        if (sentBytes < 0)
        {
            perror("send failed");
            break;
        }
    }

    cout << "File " << filePath << " sent successfully via TCP!" << endl;
}

void TCPThread()
{
    TCPClient client;
    string targetIP = "192.168.0.170";
    uint16_t targetPort = 6555;

    string imagePath;
    int imageIndex = -1;
    int imageIndexNew = 0;

    while (programRunning)
    {
        if (TCPSocketRunning)
        {
            readIndexFile(imageIndexNew);
            imageIndexNew--;

            if (imageIndexNew > imageIndex)
            {
                imageIndex = imageIndexNew;
                imagePath = folderPath + "/" + to_string(imageIndexNew) + ".bmp";
                client.sendFile(targetIP, targetPort, imagePath);
            }
            else
            {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
        else
        {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
}