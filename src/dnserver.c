#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include "common.h"

int main(int argc, char** argv) {
    if (argc <= 3) return 1;
    char *ip, *port;
    FILE *zones, *log;
    short logging = 1;

    ip = argv[1];
    port = argv[2];
    zones = fopen(argv[3], "r");
    if (argc > 4) log = fopen(argv[4], "rw");
    else logging = 0;

    int sockFD = socket(AF_INET, SOCK_DGRAM, PROTOCOL);
    if (sockFD < 0) { 
        printf("[ERR] Failed to create the socket\n"); 
        return 1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(port));
    server.sin_addr.s_addr = inet_addr(ip);

    int res = bind(sockFD, (struct sockaddr*) &server, sizeof server);
    if (res < 0) { 
        printf("[ERR] Failed to bind the socket\n"); 
        return 1;
    }

    while (1) {
        unsigned char buffer[512];
        for (int i = 0; i < sizeof(buffer); i++) buffer[i] = '\0';
        
        struct sockaddr_in client;
        int client_len = sizeof(client);

        printf("[RES] Awaiting request. ");
        int received = recvfrom(sockFD, &buffer, 512, 0, (struct sockaddr*) &client, (socklen_t*) &client_len);
        if (received < 0) printf("Failed.\n");
        else printf("Done (%d bytes).\n", received);
        for (int i = 0; i < sizeof(buffer); i++) buffer[i] &= 0xFF;

        for (int i = 0; i < 32; i++) printf("%02X ", buffer[i]);
        printf("\n");

        Header* header = (Header*) &buffer;
        header->flags = htons(0b1000010000000000); //  Turn QR & AA to 1
        int size = sizeof(Header);

        int stop = 0;
        unsigned char* reader = &buffer[size];
        char* name = read_name(reader, buffer, &stop, &size);
        strcat(name, ".");
        size++; // Increase one for end of domain '\0'

        Question* question = (Question*) &buffer[size];
        size += sizeof(Question);

        // Look for domain on the path
        char line[0xFF], *ip = NULL;
        while (fgets(line, sizeof line, zones)) {
            if (line[0] == ';' || line[0] == '\n' || line[0] == '\r') continue;

            char* token = strtok(line, " \t\n");
            if (!streq(token, name)) continue; // Skip line if domain is different

            token = strtok(NULL, " \t\n");
            if (resolve_class(token) != ntohs(question->_class)) continue; // Skip if request isn't IN (never)

            token = strtok(NULL, " \t\n");
            if (resolve_type(token) != ntohs(question->type)) continue; // Skip if request isn't A

            token = strtok(NULL, " \t\n"); // Result IP
            ip = strdup(token);
            break;
        }
        printf("Found IP %s\n", ip);

        header->addcount = htons(0);
        header->authcount = htons(0);
        if (ip != NULL) {
            header->anscount = htons(1);
            printf("[RES] Building answer. ");

            char* answer = &buffer[size];
            printf("%d\n", size);
            *(answer++) = 0xc0;
            *(answer++) = 0x0c; // Compress domain name on offset 12

            *(answer++) = 0x00;
            *(answer++) = 0x01; // Type A

            *(answer++) = 0x00; 
            *(answer++) = 0x01; // Class IN

            *(answer++) = 0x00; 
            *(answer++) = 0x00;
            *(answer++) = 0x01; 
            *(answer++) = 0x26; // TTL

            *(answer++) = 0x00; 
            *(answer++) = 0x04; // Length (IPv4)

            unsigned long ip_num = (unsigned long) inet_addr(ip);
            *(answer++) += ip_num & 0xFF;
            *(answer++) = (ip_num >> 8) & 0xFF;
            *(answer++) = (ip_num >> 16) & 0xFF;
            *(answer++) = (ip_num >> 24) & 0xFF;
            size += 16;
            printf("Done. %ld\n", ip_num);
        }

        for (int i = 0; i < 48; i++) printf("%02X ", buffer[i]);
        printf("\n");

        printf("[RES] Sending packet... ");
        int sent_size = sendto(sockFD, (char*) buffer, size, 0, (struct sockaddr*) &client, client_len);
        if (sent_size < 0) printf("Failed.\n");
        else printf("Done (%d bytes).\n", sent_size);

        fseek(zones, 0, SEEK_SET);
        free(ip);
    }
}