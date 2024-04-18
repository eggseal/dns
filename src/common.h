#pragma once

typedef struct Header {
    short int id;
    short int flags; // QR(1) OPCODE (4) AA(1) TC(1) RD(1) RA(1) Z(3) RCODE(4)

    short int qcount;  
    short int anscount;
    short int authcount;
    short int addcount;
} Header;

typedef struct Question {
    short int type;
    short int _class;
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

typedef struct QueryRes {
    char query[298];
    char response[0xFF];
} QueryRes;

typedef struct Cache {
    unsigned int index;
    QueryRes entries[10];
} Cache;

#define PROTOCOL 0

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

unsigned char* get_class(unsigned short type) {
    if (type == 1) return "IN";
}

unsigned short resolve_class(char* type) {
    if (streq(type, "IN")) return 1;
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

unsigned char* read_name(unsigned char* reader,unsigned char* buffer,int* count, int* size) {
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
        *size += p + 1;
        for(int j = 0; j < p; j++) {
            name[i] = name[i + 1];
            i++;
        }
        name[i] = '.';
    }
    name[name_len - 1] = '\0'; //remove the last dot
    return name;
}

char* check_cache(Cache* cache, char* query) {
    for (int i = 0; i < 10; i++) {
        if (streq(query, cache->entries[i].query)) 
            return cache->entries[i].response;
    }
    printf("[CCH] %s not found\n", query);
    return "\0";
}

void add_cache(Cache* cache, char* query, char* res) {
    cache->index++;
    if (cache->index >= 10) cache->index = 0;

    printf("[CCH] Cached %s %s\n", query, res);
    strcpy(cache->entries[cache->index].query, query);
    strcpy(cache->entries[cache->index].response, res);
}