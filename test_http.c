#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <ctype.h>

#include "http_net_proc.h"
#include "hftp.h"

int main(int argc, char** argv)
{
    if (argc < 3 || 4 < argc) 
    {
        fprintf(stderr, "usage : hftp_one_cmd <method(get,put,rls,lls)> <remote_url> [local_dir]\n");
        return 1;
    }

    return http_conn_proc(argv[1], argv[2], argc >= 3? argv[3]: NULL);
}

