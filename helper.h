#pragma once
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include <vector>
#include <set>
#include <regex>
#include <netinet/tcp.h>
#include <iomanip>
#include <cmath>

using namespace std;

const int LISTEN_QUEUE_SIZE = 5;
const int MAX_ID_SIZE = 50;
const int MAX_TOPIC_SIZE = 51;
const int MAX_STRING_SIZE = 1501;

// UDP Message
struct UDPMessage
{
    uint8_t data_type;
    string topic;
    uint8_t *data;
    int size;
};

// Message for subscribe/unsubscribe
typedef struct SubscribeMessage
{
    uint8_t command;
    char topic[MAX_TOPIC_SIZE];
} SubscribeMessage;

// TCP Header
typedef struct TCP_Header
{
    uint32_t ip;
    int port;
    int length;
    uint8_t data_type;
    char topic[MAX_TOPIC_SIZE];
} TCP_Header;

// TCP Package
typedef struct TCP_Package
{
    TCP_Header hdr;
    char data[1];
} TCP_Package;

// Class that contains Client Info
struct ClientInfo
{
    int sockfd;
    bool is_connected;
    string client_id;
    set<string> topics;
};

// Function to send all data
ssize_t send_all(int sockfd, const void *buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        int bytes_sent = send(sockfd, (const char *)buf + total, len - total, 0);
        if (bytes_sent == -1)
        {
            cerr << "Error sending data" << endl;
            return -1;
        }
        total += bytes_sent;
    }
    return total;
}

// Function to receive all data
ssize_t receive_all(int sockfd, void *buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        int bytes_received = recv(sockfd, (char *)buf + total, len - total, 0);
        if (bytes_received == -1)
        {
            cerr << "Error receiving data" << endl;
            return -1;
        }
        else if (bytes_received == 0)
        {
            return 0;
        }
        total += bytes_received;
    }
    return total;
}
