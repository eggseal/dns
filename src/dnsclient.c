// Headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

// Structures
typedef struct Header {
    unsigned short id;
    short int flags; // RD(1) TC(1) AA(1) OP_CODE(4) QR(1) R_CODE(4) CD(1) AD(1) Z(1) RA(1)

    unsigned short qcount;  
    unsigned short anscount;
    unsigned short authcount;
    unsigned short addcount;
} Header;

typedef struct Question {
    unsigned short type;
    unsigned short class;
} Question;

#pragma pack(push, 1)
typedef struct ResData {
    unsigned short type;
    unsigned short class;
    unsigned int ttl;
    unsigned short len;
} ResData;
#pragma pack(pop)

typedef struct ResRecord {
    unsigned char* name;
    ResData* resource;
    unsigned char* rdata;
} ResRecord;

typedef struct Query {
    unsigned char* name;
    Question* ques;
} Query;

// Constants
#define MAX_CMD 298
#define MAX_DOMAIN 256
#define PORT 53
#define PROTOCOL 0
#define BUFFER_SIZE 0xFFFF

// Variables
char* log_path;
char cmd[MAX_CMD]; 
char *ip, *type, *domain;
unsigned char logging = 1;

// Subroutines
#define streq(var, str) (strcmp(var, str) == 0)

#define empty(str) (strlen(str) == 0)

unsigned short resolve_type(char* type) {
    if (streq(type, "SOA")) return 6;
    if (streq(type, "A")) return 1;
    if (streq(type, "NS")) return 2;
    if (streq(type, "MX")) return 15;
    if (streq(type, "CNAME")) return 5;
}

unsigned char* get_type(unsigned short type) {
    if (type == 6) return "SOA";
    if (type == 1) return "A";
    if (type == 2) return "NS";
    if (type == 15) return "MX";
    if (type == 5) return "CNAME";
}

void scanclient(char* cmd, char** ip, char** type, char** domain) {
    char* token;
    char* prev;
    *ip = NULL, *type = NULL, *domain = NULL;

    token = strtok(cmd, " ");
    while (token != NULL) {
        prev = token;
        token = strtok(NULL, " ");

        if (streq(prev, "SERVER")) *ip = strdup(token);
        else if (streq(prev, "TYPE")) *type = strdup(token);
        else if (streq(prev, "DOMAIN")) *domain = strdup(token);
    }
    (*ip)[strcspn(*ip, "\n")] = '\0';
    (*type)[strcspn(*type, "\n")] = '\0';
    (*domain)[strcspn(*domain, "\n")] = '\0'; 
}

void format_dns_name(char* query, char* domain) {
    int lock = 0;
    strcat(domain, ".");
    for (int i = 0; i < strlen(domain); i++) {
        if (domain[i] != '.') continue;

        *query++ = i - lock; // Write the length
        for (; lock < i; lock++) 
            *query++ = domain[lock]; // Write the domain until before the next '.'
        lock++;
    }
    *query++ = '\0';
}

unsigned char* read_name(unsigned char* reader,unsigned char* buffer,int* count) {
    unsigned char *name;
    unsigned int p = 0,jumped = 0;
 
    *count = 1;
    name = (unsigned char*)malloc(0x100);
    name[0]='\0';
 
    while(*reader != 0) {
        if(*reader >= 0xC0) {
            unsigned int offset = (*reader) * 0x100 + *(reader + 1) - 0xC000;
            reader = buffer + offset - 1;
            jumped = 1;
        } else {
            name[p++] = *reader;
        }
 
        reader++;
        if(jumped == 0) (*count)++;
    }
 
    name[p] = '\0';
    if(jumped == 1) (*count)++;
 
    int name_len = strlen(name);
    for(int i = 0; i < name_len; i++) {
        p = name[i];
        for(int j = 0; j < p; j++) {
            name[i] = name[i + 1];
            i++;
        }
        name[i] = '.';
    }
    name[name_len - 1] = '\0'; //remove the last dot
    return name;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("[WRN] A logging path was not entered. Logging is disabled\n");
        logging = 0;
    } else {
        log_path = argv[1];
    }
    FILE* file;

    while (1) {
        // Get the variables from user input
        printf(">> ");
        fgets(cmd, sizeof cmd, stdin);
        scanclient(cmd, &ip, &type, &domain);

        if (empty(ip) || empty(type) || empty(domain)) {
            printf("[ERR] Invalid command\n");
            continue;
        }

        printf("[REQ] Requesting to '%s' for hostname '%s'. Req type %s\n", ip, domain, type);
        unsigned char buffer[BUFFER_SIZE];
        unsigned int size = 0;

        int sockFD = socket(AF_INET, SOCK_DGRAM, PROTOCOL);
        if (sockFD < 0) { 
            printf("[ERR] Failed to create the socket\n"); 
            continue;
        }

        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(PORT);
        server.sin_addr.s_addr = inet_addr(ip);

        printf("[REQ] Resolving request... ");
        Header *header = (Header*) &buffer;
        header->id = htons(getpid());
        header->flags = 0b1000000000000000;
        header->qcount = htons(1); // 1 question
        header->anscount = 0;
        header->authcount = 0;
        header->addcount = 0;
        size += sizeof(Header);

        unsigned char* name = (unsigned char*) &buffer[size];
        format_dns_name(name, domain);
        size += strlen((const char*) name) + 1;

        Question* info = (Question*) &buffer[size];
        info->type = htons(resolve_type(type));
        info->class = htons(1);
        size += sizeof(Question);
        printf("Done.\n");

        printf("[REQ] Sending packet... ");
        int sent_size = sendto(sockFD, (char*) buffer, size, 0, (struct sockaddr*) &server, sizeof(server));
        if (sent_size < 0) printf("Failed.\n");
        else printf("Done (%d bytes).\n", sent_size);

        int len_p = sizeof(server);
        printf("\n[RES] Receiving response... ");
        int received = recvfrom(sockFD, (char*) buffer, BUFFER_SIZE, 0, (struct sockaddr*) &server, (socklen_t*) &len_p);
        if (received < 0) printf("Failed.\n");
        else printf("Done (%d bytes).\n", received);

        header = (Header*) buffer;
        unsigned char* reader = &buffer[size];
        printf("[RES] Query: %d, Answer %d, Authority: %d, Additional: %d\n", ntohs(header->qcount), ntohs(header->anscount), ntohs(header->authcount), ntohs(header->addcount));

        ResRecord answer[20], authority[20], additional[20];
        unsigned int ans_meta = 0, aut_meta = 0, add_meta = 0;

        int stop = 0;
        for (int i = 0; i < ntohs(header->anscount); i++) {
            answer[i].name = read_name(reader, buffer, &stop);
            strcat(answer[i].name, ".");
            reader += stop;

            if ((int) strlen(answer[i].name) > ans_meta) 
                ans_meta = strlen(answer[i].name);

            answer[i].resource = (ResData*) reader;
            reader += sizeof (ResData);

            if (ntohs(answer[i].resource->type) == 1) {
                answer[i].rdata = (unsigned char*) malloc(ntohs(answer[i].resource->len));
                for (int j = 0; j < ntohs(answer[i].resource->len); j++) 
                    answer[i].rdata[j] = reader[j];

                answer[i].rdata[ntohs(answer[i].resource->len)] = '\0';
                reader += ntohs(answer[i].resource->len);
            } else {
                answer[i].rdata = read_name(reader, buffer, &stop);
                reader += stop;
            }
        }

        for (int i = 0; i < ntohs(header->authcount); i++) {
            authority[i].name = read_name(reader, buffer, &stop);
            reader += stop;

            if ((int) strlen(authority[i].name) > aut_meta) 
                aut_meta = strlen(authority[i].name);
    
            authority[i].resource = (ResData*) reader;
            reader += sizeof(ResData);
    
            authority[i].rdata=read_name(reader, buffer, &stop);
            reader += stop;
        }

        for (int i = 0; i < ntohs(header->addcount); i++) {
            additional[i].name = read_name(reader, buffer, &stop);
            reader += stop;

            if ((int) strlen(additional[i].name) > add_meta) 
                add_meta = strlen(additional[i].name);
    
            additional[i].resource = (ResData*) reader;
            reader += sizeof(ResData);
    
            if (ntohs(additional[i].resource->type) == 1) {
                additional[i].rdata = (unsigned char*) malloc(ntohs(additional[i].resource->len));
                for(int j = 0; j < ntohs(additional[i].resource->len); j++)
                additional[i].rdata[j] = reader[j];
    
                additional[i].rdata[ntohs(additional[i].resource->len)]='\0';
                reader+=ntohs(additional[i].resource->len);
            } else {
                additional[i].rdata=read_name(reader, buffer, &stop);
                reader+=stop;
            }
        }

        if (ntohs(header->anscount) > 0) printf("\n[ANS] %-*s   %-5s   %-20s\n", ans_meta, "Name", "Type", "Response");
        for (int i = 0; i < ntohs(header->anscount); i++) {
            printf("[   ] %-*s", ans_meta, answer[i].name);
            printf("   %-5s", get_type(ntohs(answer[i].resource->type)));

            char* res;
            if (ntohs(answer[i].resource->type) == resolve_type("A")) {
                long* ip = (long*) answer[i].rdata;
                server.sin_addr.s_addr = *ip;
                res = inet_ntoa(server.sin_addr);
            } else if (ntohs(answer[i].resource->type) == resolve_type("CNAME")) {
                res = answer[i].rdata;
            }
            printf("   %s", res);
            printf("\n");

            if (logging == 0) continue;
            time_t current = time(NULL);
            struct tm* local = localtime(&current);

            file = fopen(log_path, "a");

            char the_time[20];
            strftime(the_time, sizeof(the_time), "%Y-%m-%d %H:%M:%02S", local);

            fprintf(file, "%s %s %s %s %s\n", the_time, ip, get_type(ntohs(answer[i].resource->type)), answer[i].name, res);
            fclose(file);
        }

        if (ntohs(header->authcount) > 0) printf("\n[AUT] %-*s   %-5s   %-20s\n", aut_meta, "Name", "Type", "Response");
        for (int i = 0; i < ntohs(header->authcount); i++) {
            printf("[   ] %-*s", ans_meta, authority[i].name);
            printf("   %-5s", get_type(ntohs(authority[i].resource->type)));

            if (ntohs(authority[i].resource->type) == resolve_type("NS")) {
                printf("   %s", authority[i].rdata);
            }
            printf("\n");
        }

        if (ntohs(header->addcount) > 0) printf("\n[ADD] %-*s   %-5s   %-20s\n", add_meta, "Name", "Type", "Response");
        for (int i = 0; i < ntohs(header->addcount); i++) {
            printf("[   ] %-*s", ans_meta, additional[i].name);
            printf("   %-5s", get_type(ntohs(additional[i].resource->type)));

            if (ntohs(additional[i].resource->type) == resolve_type("A")) {
                long* ip = (long*) additional[i].rdata;
                server.sin_addr.s_addr = *ip;
                printf("   %s", inet_ntoa(server.sin_addr));
            }
            printf("\n");
        }
    }
    return 0;
}