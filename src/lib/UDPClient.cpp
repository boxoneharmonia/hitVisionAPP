#include "UDPClient.hpp"
#include "file.hpp"
using namespace std;
#define CHUNK_SIZE 1024

UDPClient::UDPClient()
{
    createSocket();
}

UDPClient::~UDPClient()
{
    if (sockfd >= 0)
    {
        close(sockfd);
    }
}

void UDPClient::createSocket()
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Failed to create UDP socket");
        exit(EXIT_FAILURE);
    }
}

void UDPClient::bindAddress(const string &ip, uint16_t port)
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

void UDPClient::connectAddress(const string &ip, uint16_t port)
{
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0)
    {
        perror("Invalid address or Address not supported");
        exit(EXIT_FAILURE);
    }
}

void UDPClient::sendFile(const string &ip, uint16_t port, const string &filePath)
{
    ifstream file(filePath, ios::binary);
    if (!file.is_open())
    {
        cout << "Failed to open file: " << filePath << endl;
        return;
    }
    const size_t bufferSize = CHUNK_SIZE;
    char sendbuffer[bufferSize];

    // connectAddress(ip, port);

    while (file.read(sendbuffer, bufferSize) || file.gcount() > 0)
    {
        size_t bytesToSend = file.gcount();
        ssize_t sentBytes = sendto(sockfd, sendbuffer, bytesToSend, 0,
                                   (struct sockaddr *)&serverAddr, sizeof(serverAddr));
        if (sentBytes < 0)
        {
            perror("sendto failed");
            break;
        }
    }
    cout << "File " << filePath << " sent successfully via UDP!" << endl;
}

void UDPThread()
{
    UDPClient client;
    string targetIP = "192.168.0.170";
    uint16_t targetPort = 6555;

    string imagePath;
    int imageIndex = -1;
    int imageIndexNew = 0;

    client.connectAddress(targetIP, targetPort);

    while (1)
    {
        if (UDPSocketRunning)
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