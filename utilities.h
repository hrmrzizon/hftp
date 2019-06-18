#pragma once

struct in_addr;

char* cstrdup(char* str);
char* cstrndup(char* str, int len);

char* get_file_extension(char* name);

int get_paged_buffer(int req_bytes, char** buffer, int* buffer_len);

int domain_to_addrv4(const char* domain, struct in_addr* addr);
int host_to_addrv4(const char* host, struct in_addr* addr);


