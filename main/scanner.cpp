#include <iostream>
#include <string>
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")


using namespace std;

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed: " << iResult << endl;
        return 1;
    }

    // Get local IP address
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct hostent* host = gethostbyname(hostname);
    char* local_ip = inet_ntoa(*(struct in_addr*)*host->h_addr_list);

    // Initialize ICMP library
    HANDLE hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) {
        cout << "IcmpCreateFile failed: " << GetLastError() << endl;
        return 1;
    }

    // Get network interface information
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*) malloc(ulOutBufLen);
    if (pAdapterInfo == NULL) {
        cout << "Memory allocation error" << endl;
        return 1;
    }

    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        // Allocate enough memory for the adapter info
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*) malloc(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            cout << "Memory allocation error" << endl;
            return 1;
        }

        // Get the adapter info
        if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) != ERROR_SUCCESS) {
            cout << "GetAdaptersInfo failed: " << GetLastError() << endl;
            return 1;
        }
    }

    // Loop through all network interfaces
    for (PIP_ADAPTER_INFO adapter = pAdapterInfo; adapter != NULL; adapter = adapter->Next) {
        // Ignore loopback and tunnel interfaces
        if (adapter->Type == MIB_IF_TYPE_LOOPBACK || adapter->Type == IF_TYPE_TUNNEL) {
            continue;
        }

        // Loop through all IP addresses on the interface
        for (PIP_ADDR_STRING ipaddr = &adapter->IpAddressList; ipaddr != NULL; ipaddr = ipaddr->Next) {
            // Ignore the local IP address
            if (strcmp(ipaddr->IpAddress.String, local_ip) == 0) {
                continue;
            }

            // Ping the IP address
            char send_buf[32] = "Ping test";
            DWORD reply_size = sizeof(ICMP_ECHO_REPLY) + sizeof(send_buf);
            LPVOID reply_buf = (LPVOID) malloc(reply_size);
            if (reply_buf == NULL) {
                cout << "Memory allocation error" << endl;
                return 1;
            }

            DWORD reply_count = IcmpSendEcho(hIcmpFile, inet_addr(ipaddr->IpAddress.String), send_buf, sizeof(send_buf), NULL, reply_buf, reply_size, 5000);
            if (reply_count == 0) {
                cout << "Ping failed: " << GetLastError() << endl;
                continue;
            }

            // Print the IP address, hostname, and ping time
            string hostname            = "";
        HOSTENT* host = gethostbyaddr((const char*) &((ICMP_ECHO_REPLY*) reply_buf)->Address, sizeof(((ICMP_ECHO_REPLY*) reply_buf)->Address), AF_INET);
        if (host != NULL) {
            hostname = host->h_name;
        }
        cout << "IP Address: " << ipaddr->IpAddress.String << " | Hostname: " << hostname << " | Ping: " << ((ICMP_ECHO_REPLY*) reply_buf)->RoundTripTime << " ms" << endl;
        getchar();

        // Free the reply buffer
        free(reply_buf);
    }
}

// Free the adapter info
free(pAdapterInfo);

// Clean up the ICMP library
IcmpCloseHandle(hIcmpFile);

// Clean up Winsock
WSACleanup();

return 0;
}