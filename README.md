# Topic-Based Client-Server Messaging in C++

This project implements a **client-server** model that manages messages across **TCP** and **UDP**.  
- **TCP Clients** can subscribe/unsubscribe to various **topics**.  
- **UDP Clients** send messages on certain topics, which the **server** then **forwards** to all TCP subscribers for those topics.  
- Wildcard support is implemented using **`<regex>`** to match topic patterns.

---

## Table of Contents
1. [Overview](#overview)  
2. [Data Structures](#data-structures)  
3. [Communication Flow](#communication-flow)  
4. [Topic Subscription & Wildcards](#topic-subscription--wildcards)  
5. [Multiplexing & Poll](#multiplexing--poll)  
6. [How to Build & Run](#how-to-build--run)  
7. [Conclusion](#conclusion)  

---

## Overview
The system is comprised of:
- **Server** (listening on both **TCP** and **UDP** sockets).  
- **TCP Clients** (subscribe to or unsubscribe from specific topics).  
- **UDP Clients** (send messages tagged with a particular topic).

Whenever the server receives a **UDP message**, it determines which **TCP clients** have subscribed to the message’s topic. It then **forwards** a **TCP packet** to each subscriber.

---

## Data Structures

### **SubscribeMessage**
A small structure that holds:
- `command` (1 = **subscribe**, 0 = **unsubscribe**)
- `topic` (the string identifying a subscription)

### **TCP_Header**
Stores essential metadata for a TCP message:
- `ip` and `port`: information about the **UDP sender** (used for display).
- `length`: total length of the incoming TCP packet.
- `data_type`: indicates how to parse the payload (e.g., int, float, string).
- `topic`: which topic the message references.

### **TCP_Package**
Combines:
- A **`TCP_Header`** struct.
- The **actual message payload** in a `data` array.

### **ClientInfo**
Used to track **TCP clients** connected to the server:
- `sockfd`: the client’s TCP socket.
- `client_id`: unique string identifier.
- `topics`: a set of subscribed topics.
- `is_connected`: boolean indicating whether the client is active.

### **UDPMessage**
Holds:
- `topic`, `data_type`, pointer to raw `data`, and a `size` value.

---

## Communication Flow

1. **Server Initialization**  
   - Opens two sockets: **TCP** (listens for client connections) and **UDP** (receives messages).
   - Binds both sockets to the specified port.

2. **TCP Clients**  
   - Connect and send **`client_id`** upon starting.
   - Can **subscribe** or **unsubscribe** from topics using **SubscribeMessage**.
   - Receive **forwarded messages** whenever the server gets a relevant UDP packet.

3. **UDP Messages**  
   - The server receives a datagram on the **UDP socket**.
   - Parses the **`UDPMessage`** (topic, data type, raw payload).
   - Looks up **TCP clients** that match the topic.
   - Sends each client a **`TCP_Package`** containing the IP/port of the original UDP sender and the parsed content.

---

## Topic Subscription & Wildcards

- **Topics** can include **wildcards** (`+` or `*`) to represent variable parts.  
- The server uses **C++ `<regex>`**:
  - Replaces `+` with `[^/]+` (single-segment wildcard).
  - Replaces `*` with `.*` (multi-segment wildcard).
  - Uses `regex_match` to see if an incoming topic matches any **subscribed pattern**.

---

## Multiplexing & Poll
The server:
- Utilizes **`poll()`** to watch multiple file descriptors simultaneously:
  - The **UDP socket** for incoming datagrams.
  - The **TCP listening socket** for new client connections.
  - All **TCP client sockets** for commands (subscribe/unsubscribe) or disconnections.
- When `poll()` indicates data on a socket, the server reacts by either:
  - **Reading** and **forwarding** a UDP message.
  - **Accepting** a new TCP connection.
  - Processing a **subscribe/unsubscribe** message.
  - Handling **client disconnection**.

---

## How to Build & Run

1. **Compile**  
   - Use a C++ compiler (e.g., `g++`) or your preferred build system (e.g., `make`, `CMake`).
   - Make sure to link the **`-lpthread`** or other required libraries if needed for networking on your platform.

2. **Start the Server**  
   ```bash
   ./server <port>
