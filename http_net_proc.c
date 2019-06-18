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

#include "http_core.h"
#include "http_method.h"
#include "utilities.h"
#include "url.h"
#include "http_net_proc.h"

extern int (*g_param_set_func_arr[])(struct http_callback*, void*, struct url_data*, char*);

int process_method(struct sockaddr_in* in, struct url_data* data, char* local_dir, struct http_connection* conn, int method)
{
    char buffer[4096];
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
	{
		fputs("hftp.err >> fail to get socekt fd..\n", stderr);
		return 1;
	}

	if (connect(sockfd, (struct sockaddr*)in, sizeof(struct sockaddr_in)) < 0)
	{
		fputs("hftp.err >> fail to connect..\n", stderr);
		close(sockfd);
		return 1;
	}

    conn->sockfd = sockfd;

    union method_chunks c;
    memset(&c, 0, sizeof(c));

    g_param_set_func_arr[method](&conn->clb, &c, data, local_dir);

    if (http_one_req_resp(conn, buffer, 4096))
    {
        fprintf(stderr, "hftp.err >> fail to http process..\n");
        return 1;
    }

	close(sockfd);
    conn->sockfd = -1;

	return 0;
}

int http_conn_proc(char* method_str, char* remote_url_str, char* local_dir_str)
{
    struct http_connection conn; http_connection_init(&conn);

   	struct url_data dat;
	memset(&dat, 0, sizeof(struct url_data));
	url_to_data(remote_url_str, &dat);
	
	struct in_addr addr;
	if (!host_to_addrv4(dat.domain, &addr))
	{
        // socket initialization
		struct sockaddr_in in;
        memset(&in, 0, sizeof(struct sockaddr_in));

		in.sin_family = AF_INET;
		in.sin_addr = addr;
		in.sin_port = htons(dat.port);

        // method
        int method = -1;

        if (name_to_method(method_str, &method))
        {
            fprintf(stderr, "hftp.err >> fail to get method(%s)..\n", method_str);
            goto http_get_fail;
        }

        /*
         append connectino socket fd
         */

        if (process_method(&in, &dat, local_dir_str, &conn, method))
        {
            fprintf(stderr, "hftp.err >> fail to process http transmission..\n");
            goto http_get_fail;
        }
        else
            printf("hftp >> process command end!\n");               
	}
	else
    {
	    fputs("hftp.err >> fail to parse <remote_url>..\n", stderr);
        goto http_get_fail;
    }

    http_connection_destroy(&conn);
	url_data_cleanup(&dat);

	return 0;

http_get_fail:

    http_connection_destroy(&conn);
	url_data_cleanup(&dat);

    return 1;
}


