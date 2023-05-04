#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>

#define BUFFER_SIZE 1024
#define TIMEOUT 10 // Timeout in seconds for getnameinfo()

int main() {
    int sockfd;
    struct sockaddr_in send_addr, recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    char buffer[BUFFER_SIZE];

    // Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set the socket to non-blocking mode to enable timeouts
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Allow broadcast on the socket
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Ask the user whether to scan the connected Wi-Fi network or enter an IP address
    std::cout << "Choose an option:" << std::endl;
    std::cout << "1. Scan connected Wi-Fi network" << std::endl;
    std::cout << "2. Enter IP address" << std::endl << ">>> ";
    int option;
    std::cin >> option;
    std::cin.ignore();

    if (option == 1) {
        // Get the IP address of the connected Wi-Fi network
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) == -1) {
            perror("getifaddrs");
            exit(EXIT_FAILURE);
        }

        char host[NI_MAXHOST];
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) {
                continue;
            }
            int family = ifa->ifa_addr->sa_family;
            if (family == AF_INET && (ifa->ifa_flags & IFF_RUNNING)) {
                getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                std::cout << "Scanning network " << host << "..." << std::endl;

                // Construct the broadcast address from the network address
                std::string broadcast_addr = std::string(host);
                broadcast_addr = broadcast_addr.substr(0, broadcast_addr.find_last_of(".") + 1) + "255";

                memset(&send_addr, 0, sizeof(send_addr));
                send_addr.sin_family = AF_INET;
                send_addr.sin_addr.s_addr = inet_addr(broadcast_addr.c_str());
                send_addr.sin_port = htons(9000);

                // Send a broadcast message to all devices on the network
                if (sendto(sockfd, "Hello", 5, 0, (struct sockaddr *)&send_addr, sizeof(send_addr)) < 0) {
                    perror("sendto");
                    exit(EXIT_FAILURE);
                }
            }
        }

        freeifaddrs(ifaddr);
    } else if (option == 2) {
        // Ask the user to enter an IP address
        std::string ip_addr;
        std::cout << "Enter IP adress: ";
            std::cin >> ip_addr;

    memset(&send_addr, 0, sizeof(send_addr));
    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = inet_addr(ip_addr.c_str());
    send_addr.sin_port = htons(9000);

    // Send a message to the specified IP address
    if (sendto(sockfd, "Hello", 5, 0, (struct sockaddr *)&send_addr, sizeof(send_addr)) < 0) {
        perror("sendto");
        exit(EXIT_FAILURE);
    }
} else {
    std::cout << "Invalid option" << std::endl;
    exit(EXIT_FAILURE);
}

// Receive responses from devices on the network
int num_responses = 0;
while (num_responses < 10) {
    // Wait for a response for up to 1 second
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (select(sockfd + 1, &read_fds, NULL, NULL, &timeout) == -1) {
        perror("select");
        exit(EXIT_FAILURE);
    }

    if (FD_ISSET(sockfd, &read_fds)) {
        memset(buffer, 0, BUFFER_SIZE);
        if (recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len) < 0) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        // Print the IP address and hostname of the device that responded
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(recv_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

        struct hostent *host_info;
        int err = 0;
        fd_set except_fds;
        FD_ZERO(&except_fds);
        FD_SET(sockfd, &except_fds);
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;
        if (getnameinfo((struct sockaddr *)&recv_addr, sizeof(recv_addr), buffer, sizeof(buffer), NULL, 0, NI_NAMEREQD) == 0) {
            std::cout << ip_str << " (" << buffer << ")" << std::endl;
        } else if (errno == EAI_AGAIN) {
            std::cout << ip_str << " (timed out)" << std::endl;
        }
        num_responses++;
    }
}

close(sockfd);
return 0;
}
