#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hftp.h"
#include "utilities.h"

char* cstrdup(char* str)
{
    assert(str != NULL);

    char* p = (char*)malloc(strlen(str) + 1);
    strcpy(p, str);

    return p;
}

char* cstrndup(char* str, int len)
{
    assert(str != NULL);

    char* p = (char*)malloc(len + 1);
    strncpy(p, str, len);

    return p;
}

char* get_file_extension(char* name)
{
    char* dot = strrchr(name, '.'),
        * slash = strrchr(name, '/');

    if (dot == NULL || slash > dot) return NULL;
    else 
    {
        return dot + 1;
    }
}

int get_paged_buffer(int req_bytes, char** buffer, int* buffer_len)
{
    assert(buffer != NULL);
    assert(buffer_len != NULL);

    int alloc_size = req_bytes, page_size = sysconf(_SC_PAGESIZE) ;
    alloc_size = page_size > req_bytes? page_size: (req_bytes / page_size) * page_size;

    char* buf = (char*)malloc(alloc_size);

    if (!buf)
    {
        fprintf(stderr, "hftp.gettest >> fail to allocate buffer..");
        return -1;
    }

    *buffer = buf;
    *buffer_len = alloc_size;

    return 0;
}
int host_to_addrv4(const char* host, struct in_addr* addr)
{
	assert(host != NULL);
	assert(addr != NULL);
	
	struct in_addr tempaddr;

	if (!inet_pton(AF_INET, host, &tempaddr))
	{
		if (domain_to_addrv4(host, &tempaddr))
			return 1;
	}

	*addr = tempaddr;
	return 0;
}

int domain_to_addrv4(const char* domain, struct in_addr* addr)
{
	assert(domain != NULL);
	assert(addr != NULL);

	struct hostent* hostent = gethostbyname(domain);

	if (hostent == NULL)
	{
		hftp_log_err("hftp.err >> \'%s\' domain cannot get host entries\n", domain);
		return 1;
	}

	if (hostent->h_length <= 0) 
	{
		hftp_log_err("hftp.rtt >> \'%s\' domain donot have addr..\n", domain);
		return 1;
	}
	if (hostent->h_addrtype != AF_INET) 
	{
		hftp_log_err("hftp.err >> \'%s\' domain has only ipv6..\n", domain);
		return 1;
	}

	addr->s_addr = ((struct in_addr*)hostent->h_addr_list[0])->s_addr;

	return 0;
}
