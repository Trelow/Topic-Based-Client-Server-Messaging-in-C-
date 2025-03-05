#include "helper.h"

using namespace std;

// Function to parse string
string ParseString(uint8_t *data, int length)
{
    char buf[MAX_STRING_SIZE] = {0};
    memcpy(buf, data, length);
    return string(buf);
}

// Function to parse float
float parseFloat(uint8_t *data)
{
    uint32_t num;
    uint8_t exp = data[sizeof(uint32_t) + 1];
    memcpy(&num, data + 1, sizeof(uint32_t));
    num = ntohl(num);
    float res = num * pow(10, -exp);
    if (data[0] == 1)
        res = -res;
    return res;
}

// Function to parse short real
float ParseShortReal(uint8_t *data)
{
    uint16_t num;
    memcpy(&num, data, sizeof(uint16_t));
    return ntohs(num) / 100.0f;
}

// Function to parse int
int32_t ParseInt(uint8_t *data)
{
    uint32_t num;
    memcpy(&num, data + 1, sizeof(uint32_t));
    num = ntohl(num);
    if (data[0] == 1)
        num = -num;
    return num;
}

// Connect client to server
int ConectToServer(const char *server_ip, int port)
{
    /// Create a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        cerr << "Error creating socket" << endl;
        return -1;
    }

    // Set up server address
    sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);

    // Convert IPv4 to binary form, if conversion fails return -1
    if (inet_pton(AF_INET, server_ip, &srv_addr.sin_addr) <= 0)
        return -1;

    // Try to connect to server
    if (connect(sock, (sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
        return -1;

    // Enable TCP_NODELAY, if fails, close socket and return -1
    int flag = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag)) < 0)
    {
        close(sock);
        return -1;
    }
    // Return the connected socket
    return sock;
}

// Function to parse content depending on data type
void ParseContent(uint8_t *content, TCP_Header h)
{
    if (h.data_type == 0)
    {
        int32_t num = ParseInt(content);
        cout << "INT - " << num << endl;
    }
    else if (h.data_type == 1)
    {
        float num = ParseShortReal(content);
        cout << "SHORT_REAL - " << fixed << setprecision(2) << num << endl;
    }
    else if (h.data_type == 2)
    {
        float num = parseFloat(content);
        cout << "FLOAT - " << fixed << setprecision(4) << num << endl;
    }
    else if (h.data_type == 3)
    {
        string str = ParseString(content, h.length - sizeof(TCP_Header));
        cout << "STRING - " << str << endl;
    }
    else
    {
        cerr << "Invalid data type" << endl;
    }
}

// Stdin flow
int StdinFlow(int server_sock)
{
    // Read input command
    int bytes_received = 0;
    string command;
    getline(cin, command);
    // Extract first word of command
    string first_word = command.substr(0, command.find(" "));
    // If exit command, shutdown server socket and break
    if (first_word == "exit")
    {
        shutdown(server_sock, SHUT_RDWR);
        return 1;
    } // Else check if command is subscribe or unsubscribe
    else
    {
        SubscribeMessage sub_msg;
        // If there is no space after first word, print error message
        if (command.find(" ") == string::npos)
        {
            cerr << "Invalid command" << endl;
            return -1;
        }
        string topic = command.substr(first_word.size() + 1, command.size());
        strncpy(sub_msg.topic, topic.c_str(), 51);
        // If topic is empty, print error message
        if (topic.empty())
        {
            cerr << "Invalid command" << endl;
            return -1;
        }
        // If command is subscribe
        if (first_word == "subscribe")
        {
            sub_msg.command = 1;
            // Send subscribe message
            bytes_received = send_all(server_sock, &sub_msg, sizeof(SubscribeMessage));
            if (bytes_received < 0)
            {
                cerr << "Error sending subscribe message" << endl;
            }
            else
            {
                cout << "Subscribed to topic " << topic << endl;
            }
        } // If command is unsubscribe
        else if (first_word == "unsubscribe")
        {
            sub_msg.command = 0;
            // Send unsubscribe message
            bytes_received = send_all(server_sock, &sub_msg, sizeof(SubscribeMessage));
            if (bytes_received < 0)
            {
                cerr << "Error sending unsubscribe message" << endl;
            }
            cout << "Unsubscribed from topic " << topic << endl;
        } // Else print error message
        else
        {
            cerr << "Invalid command" << endl;
        }
    }
    return 0;
}

// TCP flow
int TCPFLow(int server_sock)
{
    TCP_Header h;
    // Receive header of packet
    int bytes_received = receive_all(server_sock, (uint8_t *)&h, sizeof(TCP_Header));
    // If received packet with no data, shutdown server socket and break
    if (h.length == sizeof(TCP_Header))
    {
        shutdown(server_sock, SHUT_RDWR);
        return 1;
    }
    // Allocate memory for content of packet
    int data_size = h.length - sizeof(TCP_Header);
    uint8_t *content = new uint8_t[data_size];
    // Receive content of packet
    bytes_received = receive_all(server_sock, content, data_size);
    // If all data received
    if (bytes_received == data_size)
    {
        // Print udp IP, port, topic
        struct in_addr ip_addr;
        ip_addr.s_addr = h.ip;
        cout << inet_ntoa(ip_addr) << ":" << h.port << " - " << h.topic << " - ";
        // Parse data based on data type and print it
        ParseContent(content, h);
    }
    else
    {
        cerr << "Error receiving data from server" << endl;
    }
    delete[] content;
    return 0;
}

int main(int argc, char *argv[])
{
    // Set stdout to unbuffered
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    // Connect to server
    int server_sock = ConectToServer(argv[2], atoi(argv[3]));
    // Get client id
    const char *client_id = argv[1];
    // Get server ip and port
    int bytes_received = 0;

    // Exit if connection to server failed
    if (server_sock < 0)
        return 1; // Exit if connection to server failed

    // Send client id to server
    bytes_received = send_all(server_sock, client_id, MAX_ID_SIZE);
    if (bytes_received < 0)
    {
        cerr << "Error sending client id" << endl;
        return 1;
    }

    // Set up poll for stdin and server socket
    struct pollfd fds[2];
    int nfds = 2;
    // Stdin
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    // Server socket
    fds[1].fd = server_sock;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    while (true)
    {
        // Poll for events
        int ret = poll(fds, nfds, -1);
        // If poll fails, break
        if (ret < 0)
        {
            cerr << "Error in poll" << endl;
            break;
        }
        // If stdin has input
        if ((fds[0].revents & POLLIN) == POLLIN)
        {
            int res = StdinFlow(server_sock);
            // If exit command, break
            if (res == 1)
            {
                break;
            } // If invalid command, continue
            else if (res == -1)
            {
                continue;
            }
        } // If server socket has input, it received packet from server
        else if ((fds[1].revents & POLLIN) == POLLIN)
        {
            int res = TCPFLow(server_sock);
            // If server socket received packet with no data, break
            if (res == 1)
            {
                break;
            }
        }
    }
    // Close server socket
    close(server_sock);
    return 0;
}
