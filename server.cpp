#include "helper.h"

using namespace std;

// Function to parse UDP message
UDPMessage ParseUDPMessage(const char *buffer, int len)
{
    UDPMessage msg;
    if (len > MAX_TOPIC_SIZE)
    {
        msg.topic = string(buffer, strnlen(buffer, MAX_TOPIC_SIZE - 1));
        msg.data_type = buffer[MAX_TOPIC_SIZE - 1];
        msg.data = (uint8_t *)memcpy(new uint8_t[len - MAX_TOPIC_SIZE], buffer + MAX_TOPIC_SIZE, len - MAX_TOPIC_SIZE);
        msg.size = len - MAX_TOPIC_SIZE;
    }
    return msg;
}

// Function to check if a topic matches a pattern
bool TopicMatches(const string &pattern, const string &topic)
{
    // Take the pattern and convert it to a regex pattern
    string regex_pattern = "^" + pattern + "$";
    // Replace + with [^/]+ to match any character except '/'
    regex_pattern = regex_replace(regex_pattern, regex("\\+"), "[^/]+");
    // Replace * with .*
    regex_pattern = regex_replace(regex_pattern, regex("\\*"), ".*");
    // Now use regex_match to check if the topic matches the regex pattern
    return regex_match(topic, regex(regex_pattern));
}

// Function to find topic, returns true if found
bool FindTopic(const set<string> &topics, const string &topic)
{
    for (const auto &t : topics)
    {
        if (TopicMatches(t, topic))
        {
            return true;
        }
    }
    return false;
}

// Function to remove newline character from a string
char *RemoveNewLine(char *str)
{
    int len = strlen(str);
    if (str[len - 1] == '\n')
        str[len - 1] = '\0';
    return str;
}

// Function to send UDP message to subscribers
void SendToSubscribers(const UDPMessage &msg, const vector<ClientInfo> &clients, char *ip, int port)
{
    // For each client
    for (const auto &client : clients)
    {
        // Check if the client is subscribed to the topic
        if (FindTopic(client.topics, msg.topic))
        {
            // Compute the total size for the packet
            size_t packet_size = sizeof(TCP_Header) + msg.size;
            // Allocate memory for the entire packet
            TCP_Package *p = (TCP_Package *)malloc(packet_size);
            // If memory allocation fails, print an error message and return
            if (!p)
            {
                cerr << "Memory allocation failed!" << endl;
                return;
            }

            // Initialize the packet header
            p->hdr.ip = inet_addr(ip);
            p->hdr.port = port;
            p->hdr.length = packet_size;
            p->hdr.data_type = msg.data_type;
            strncpy(p->hdr.topic, msg.topic.c_str(), sizeof(p->hdr.topic) - 1);
            p->hdr.topic[sizeof(p->hdr.topic) - 1] = '\0';

            // Initialize the data and copy it into the packet
            memset(p->data, 0, msg.size); // Initialize data to zero
            memcpy(p->data, msg.data, msg.size);

            // Send the entire packet
            ssize_t sent_bytes = send_all(client.sockfd, (char *)p, packet_size);
            if (sent_bytes < 0)
            {
                cerr << "Error sending message to client " << client.client_id << endl;
            }

            // Free allocated memory
            free(p);
        }
    }
}

// Function to bind sockets
int BindSockets(int port, int &tcp_socket, int &udp_socket, sockaddr_in &server_addr)
{
    if (bind(tcp_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Error binding TCP socket" << endl;
        close(tcp_socket);
        close(udp_socket);
        return -1;
    }
    // Bind UDP socket
    if (bind(udp_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Error binding UDP socket" << endl;
        close(tcp_socket);
        close(udp_socket);
        return -1;
    }
    return 0;
}

// Function to send empty packet to a client
void SendEmptyPacket(int sockfd)
{
    TCP_Package p;
    p.hdr.length = sizeof(TCP_Header);
    p.hdr.data_type = 0;
    p.hdr.ip = 0;
    p.hdr.port = 0;
    if (send_all(sockfd, (char *)&p, sizeof(TCP_Header)) < 0)
    {
        cerr << "Error sending empty packet" << endl;
    }
}

// Function to receive UDP message and send it to subscribers
void UDPFlow(int udp_socket, vector<ClientInfo> &clients)
{
    // Receive message UDP
    char buffer[MAX_STRING_SIZE];
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int bytes_read = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);

    // If bytes read is greater than 0
    if (bytes_read > 0)
    {
        // Parse the UDP message
        UDPMessage udpMsg = ParseUDPMessage(buffer, bytes_read);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        // Send the message to subscribers
        SendToSubscribers(udpMsg, clients, client_ip, client_port);
    }
    else
    {
        cerr << "Error receiving UDP message" << endl;
    }
}

int TCPServerFlow(int tcp_socket, vector<pollfd> &pfds, vector<ClientInfo> &clients)
{
    int bytes_read = 0;
    // Accept new connection
    sockaddr_in subscriber_addr;
    socklen_t subscriber_addr_len = sizeof(subscriber_addr);
    int new_socket = accept(tcp_socket, (sockaddr *)&subscriber_addr, &subscriber_addr_len);

    // If accept fails, print an error message and return
    if (new_socket < 0)
    {
        cerr << "Error accepting connection" << endl;
    }
    else
    {
        // Set TCP_NODELAY option
        int flag = 1;
        if (setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0)
        {
            cerr << "Error setting TCP_NODELAY on accepted socket" << endl;
            close(new_socket);
            return 1;
        }
        // Get client IP and port
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(subscriber_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(subscriber_addr.sin_port);
        // Receive client ID
        char client_id[MAX_ID_SIZE + 1];
        bytes_read = receive_all(new_socket, client_id, MAX_ID_SIZE);
        if (bytes_read < 0)
        {
            cerr << "Error receiving client ID" << endl;
            return 1;
        }
        client_id[bytes_read] = '\0';

        bool already_connected = false;
        bool restart = false;
        // Check if client already exists, or if it is connected
        for (size_t i = 0; i < clients.size(); i++)
        {
            if (clients[i].client_id == string(client_id))
            {
                // If client is already connected
                if (clients[i].is_connected)
                {
                    // Print message to console
                    cout << "Client " + string(client_id) + " already connected." << endl;
                    // Send empty packet to client, to make it close the connection
                    SendEmptyPacket(new_socket);

                    // Wait till client closes the connection and then close the socket
                    bytes_read = recv(new_socket, NULL, 0, 0);
                    if (bytes_read == 0)
                    {
                        close(new_socket);
                    }
                    else
                    {
                        cerr << "Error receiving data" << endl;
                    }
                    already_connected = true;
                }
                else
                {
                    // Else if client is not connected, restart the connection
                    restart = true;
                    clients[i].sockfd = new_socket;
                    clients[i].is_connected = true;
                }
                break;
            }
        }
        // If client is already connected, continue to next iteration
        if (already_connected)
        {
            return 1;
        }

        // If it is new client, add it to clients vector
        if (!restart)
            clients.push_back({new_socket, true, string(client_id), set<string>()});

        // Print that a new client has connecte
        cout << "New client " << client_id << " connected from " << client_ip << ":" << client_port << endl;
        // Add the new socket to the poll set
        pollfd new_pfd;
        new_pfd.fd = new_socket;
        new_pfd.events = POLLIN;
        pfds.push_back(new_pfd);
    }
    return 0;
}

void ClientSocketFlow(int i, vector<pollfd> &pfds, vector<ClientInfo> &clients)
{
    // Receive message from client
    SubscribeMessage msg;
    int bytes_read = receive_all(pfds[i].fd, (char *)&msg, sizeof(SubscribeMessage));

    // If bytes a more than 0 then message is received
    if (bytes_read > 0)
    {
        // If it is subscribe command add the topic to the client
        if (msg.command == 1)
        {
            for (size_t j = 0; j < clients.size(); j++)
            {
                if (clients[j].client_id == clients[i - 3].client_id)
                {
                    clients[j].topics.insert(msg.topic);
                }
            }
        } // If it is unsubscribe command remove the topic from the client
        else if (msg.command == 0)
        {
            for (size_t j = 0; j < clients.size(); j++)
            {
                if (clients[j].client_id == clients[i - 3].client_id)
                {
                    clients[j].topics.erase(msg.topic);
                }
            }
        } // Else print invalid command
        else
        {
            cerr << "Invalid command" << endl;
        }
    } // If bytes read is 0, client disconnected
    else if (bytes_read == 0)
    {
        // Find the client by its socket
        ClientInfo *client;
        for (size_t j = 0; j < clients.size(); j++)
        {
            if (clients[j].sockfd == pfds[i].fd)
            {
                client = &clients[j];
                break;
            }
        }
        // Print that the client has disconnected
        cout << "Client "
             << client->client_id << " disconnected." << endl;
        // Set the client as disconnected
        client->is_connected = false;
        client->sockfd = 0;
        // Close the socket
        close(pfds[i].fd);
        // Remove the closed socket from the poll set
        pfds.erase(pfds.begin() + i);
    }
    else
    {
        cerr << "Error receiving message" << endl;
    }
}

int main(int argc, char *argv[])
{
    // Disable buffering for stdout
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    // Check if the number of arguments is correct
    if (argc < 2 || argc > 2)
    {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    // Create TCP and UDP sockets
    int port = atoi(argv[1]);
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0)
    {
        cerr << "Error creating socket" << endl;
        return 1;
    }
    // Set TCP_NODELAY option
    int flag = 1;
    if (setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0)
    {
        cerr << "Error setting TCP_NODELAY on listening socket" << endl;
        close(tcp_socket);
        return 1;
    }
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0)
    {
        cerr << "Error creating socket" << endl;
        return 1;
    }

    // Set up server address
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_port = htons(port);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // Bind TCP socket
    if (BindSockets(port, tcp_socket, udp_socket, server_addr) < 0)
    {
        return 1;
    }

    // Prepare TCP socket for listening
    listen(tcp_socket, LISTEN_QUEUE_SIZE);

    // Pool
    vector<pollfd> pfds;
    for (int i = 0; i < 3; i++)
    {
        pfds.push_back(pollfd());
    }

    pfds[2].fd = STDIN_FILENO;
    pfds[2].events = POLLIN;

    pfds[1].fd = tcp_socket;
    pfds[1].events = POLLIN;

    pfds[0].fd = udp_socket;
    pfds[0].events = POLLIN;

    // Vector of connected clients
    vector<ClientInfo> clients;
    // Exit flags
    bool exit_triggered = false;
    bool all_clients_disconnected = false;
    // Main loop
    while (true)
    {
        // Wait for events
        int ret = poll(pfds.data(), pfds.size(), -1);
        if (ret < 0)
        {
            cerr << "Error in poll" << endl;
            close(tcp_socket);
            close(udp_socket);
            return 1;
        }
        // Check for events
        for (size_t i = 0; i < pfds.size(); i++)
        {
            // If server is going to shutdown, send empty package to all clients for closing connection
            if (exit_triggered && i > 2)
            {
                SendEmptyPacket(pfds[i].fd);
            }
            // If the socket has data to read
            if ((pfds[i].revents & POLLIN) == POLLIN)
            {
                // Check if the socket is the UDP socket
                if (pfds[i].fd == udp_socket && !exit_triggered)
                {
                    // Receive and send UDP message
                    UDPFlow(udp_socket, clients);
                } // Check if the socket is the TCP socket
                else if (pfds[i].fd == tcp_socket && !exit_triggered)
                {
                    int res = TCPServerFlow(tcp_socket, pfds, clients);
                    if (res == 1)
                    {
                        continue;
                    }
                } // Check if the socket is the stdin
                else if (pfds[i].fd == STDIN_FILENO && !exit_triggered)
                {
                    // Read the message from stdin
                    char *message = NULL;
                    size_t size = 0;
                    getline(&message, &size, stdin);
                    if (message == NULL)
                    {
                        cerr << "Error reading message" << endl;
                        continue;
                    }
                    // Remove newline character
                    RemoveNewLine(message);
                    // If message is "exit", set exit flag to true
                    if (strcmp(message, "exit") == 0)
                    {
                        exit_triggered = true;
                    }
                } // Else, the socket is a client socket
                else
                {
                    ClientSocketFlow(i, pfds, clients);
                }
            }
            // If exit is triggered and all clients are disconnected, set allClientsDisconnected
            if (exit_triggered && pfds.size() == 3)
            {
                all_clients_disconnected = true;
                break;
            }
        }
        // If all clients are disconnected, shutdown the server
        if (all_clients_disconnected)
        {
            break;
        }
    }
    // Shutdown and close sockets
    shutdown(tcp_socket, SHUT_RDWR);
    close(tcp_socket);
    shutdown(udp_socket, SHUT_RD);
    close(udp_socket);

    return 0;
}