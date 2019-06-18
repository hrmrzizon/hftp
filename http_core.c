#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <assert.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <ctype.h>

#include "http_core.h"
#include "url.h"
#include "utilities.h"
#include "hftp.h"
#include "mime_type.h"

enum http_common_header
{
    CMN_CONNECTION,
    CMN_CONTENT_LENGTH,
    CMN_CONTENT_TYPE,
    CMN_CONTENT_LANGUAGE,
    CMN_CONTENT_ENCODING,
    CMN_TRANSFER_ENCODING,
};

const char* g_common_headers[] = {
		"Connection" 		/*: close/keep-alive */ , 
		"Content-Length" 	/*: (unsigned integer) */ ,
		"Content-Type"		/*: (MIME_type)/(MIME_subtype) or (MIME_type)/'*' or '*'/'*'(;multi-part/form-data or charset)*/,
		"Content-Language"	/*: langauge description*/,
		"Content-Encoding"	/*: (br, gzip, deflate, etc..)*/,
        "Transfer-Encoding",
        NULL
	};

enum http_request_header
{
    REQ_HOST,
    REQ_USER_AGENT,
    REQ_ACCEPT,
    REQ_RANGE,
};

const char* g_request_headers[] = { 
		"Host" 				/*: (domain)*/, 
		"User-Agent" 		/*: (appname + version)*/, 
		"Accept" 			/*: (MIME_type)/(MIME_subtype) or (MIME_type)/'*' or '*'/'*'(;q=[0,1])*/,
		"Range"				/*: (Accept-Ranges)=(num)-(num)*/,
        NULL
	};

enum http_response_header
{
    RSP_ACCEPT_RANGES,
    RSP_CONTENT_RANGE,
};

const char* g_response_headers[] = {
		"Accept-Ranges" 	/*: (bytes? bits?) or none */ ,
		"Content-Range"		/*: <unit> <start>-<end>/<size>, <unit> <sta>-<end>/'*', <unit> '*'/<sz> */,
        NULL
	};

#define RESPONSE_CODE(x) (g_resp_codes[(int)x])
#define GET_CATEGORY(x) (g_resp_codes[(int)x] / 100)

const int g_resp_codes[] = {
        /*INF_CONTINUE                          =*/ 100,
        /*INF_SWITCHING_PROTOCOAL               =*/ 101,

        /*SUC_OK                                =*/ 200,
        /*SUC_CREATED                           =*/ 201,
        /*SUC_ACCEPTED                          =*/ 202,
        /*SUC_NON_AUTHORITATIVE_INFORM          =*/ 203,
        /*SUC_NO_CONTENT                        =*/ 204,
        /*SUC_RESET_CONTENT                     =*/ 205,
        /*SUC_PARTIAL_CONTENT                   =*/ 206,

        /*RDR_MULTIPLE_CHOICES                  =*/ 300,
        /*RDR_MOVED_PERMANANTLY                 =*/ 301,
        /*RDR_FOUND                             =*/ 302,
        /*RDR_SEE_OTHER                         =*/ 303,
        /*RDR_NOT_MODIFIED                      =*/ 304,
        /*RDR_TEMPORARY_REDIRECT                =*/ 307,
        /*RDR_PERMANANT_REDIRECT                =*/ 308,

        /*CER_BAD_REQUEST						=*/ 400,
    	/*CER_UNAUTHORIZED				    	=*/ 401,
	    /*CER_FORBIDDENT						=*/ 403,
	    /*CER_NOT_FOUND				    		=*/ 404,
    	/*CER_METHOD_NOT_ALLOWED				=*/ 405,
    	/*CER_NOT_ACCEPTABLE					=*/ 406,
    	/*CER_PROXY_AUTHENTICATION_REQUIRED	    =*/ 407,
    	/*CER_REQUEST_TIMEOUT					=*/ 408,
    	/*CER_CONFLICT			    			=*/ 409,
    	/*CER_GONE					    		=*/ 410,
    	/*CER_LENGTH_REQUIRED					=*/ 411,
    	/*CER_PRECONDITION_FAILED				=*/ 412,
    	/*CER_PAYLOAD_TOO_LARGE			    	=*/ 413,
    	/*CER_URI_TOO_LONG			    		=*/ 414,
    	/*CER_UNSUPPORTED_MEDIA_TYPE			=*/ 415,
    	/*CER_RANGE_NOT_SATISFIABLE 			=*/ 416,
    	/*CER_EXPECTATION_FAILED				=*/ 417,
	    /*CER_IM_A_TEAPOT						=*/ 418,
    	/*CER_UNPROCESSABLE_ENTITY	    		=*/ 422,
    	/*CER_TOO_EARLY			    			=*/ 425,
    	/*CER_UPGRADE_REQUIRED	    			=*/ 426,
	    /*CER_PRECONDITION_REQUIRED 			=*/ 428,
    	/*CER_TOO_MANY_REQUESTS				    =*/ 429,
    	/*CER_REQUEST_HEADER_FIELDS_TOO_LARGE	=*/ 431,
        /*CER_UNAVAILABLE_FOR_LEGAL_REASONS	    =*/ 451,
	    
    	/*SER_INTERNAL_SERVER_ERROR			    =*/ 500,
    	/*SER_NOT_IMPLEMENTED					=*/ 501,
	    /*SER_BAD_GATEWAY						=*/ 502,
    	/*SER_SERVICE_UNAVAILABLE				=*/ 503,
	    /*SER_GATEWAY_TIMEOUT					=*/ 504,
	    /*SER_HTTP_VERSION_NOT_SUPPORTED		=*/ 505,
    	/*SER_NETWORK_AUTHENTICATION_REQUIRED	=*/ 511,
    };

int http_parse_response_header_per_buffer( /*in*/char* buffer, int data_len,  
                                    /*out*/struct http_response_context* ctx, struct http_response* resp)
{
    const char separator[] = "\r\n"; int sep_len = 2;
    char* next = NULL,* prev = NULL,* prevprev = NULL;

    ctx->remain_data_len = ctx->remain_data_index = 0;

    // execute until not exist "\r\n" in memory
    for (next = NULL, prev = buffer, prevprev = NULL; data_len >= (prev - buffer); prevprev = prev, prev = next + sep_len)
    {
        /* get next \r\n */
        {
            //(next = memmem((void*)prev, data_len - (prev-buffer), (void*)separator, sep_len));

            char* findnext = NULL;
            int flag = 0;
            for (char* p = prev; findnext == NULL; p++)
            {
                if ((int)(p - prev) >= data_len - (int)(prev - buffer)) 
                {
                    ctx->remain_data_index = (unsigned int)(prev - buffer);
                    ctx->remain_data_len = (unsigned int)(data_len - (int)ctx->remain_data_index);
                    break;
                }
                switch(*p)
                {
                    case '\r':
                        flag = 1;
                        break;
                    case '\n':
                        if (flag == 1)
                            findnext = --p;
                        break;
                    default:
                        flag = 0;
                        break;
                }
            }
            
            if (ctx->remain_data_index > 0)
            {
                //puts(">> end of buffer, read more data..");
                break;
            }
            else if (next + 2 == findnext)
            {
                next = findnext;
                ctx->state = HR_BODY;
                break;
            }
            else
                next = findnext;
        }

        if (ctx->remain_data_len > 0)
        {
            //puts(">> end of buffer, read more data..");
            break;
        }

       // set "~'\r'\n~" -> "~'\0'\n~"
        *next = '\0';
 
        switch (ctx->state)
        {
        case HR_FIRST_LINE: // first line, "HTTP/1.1 200 OK"
            {
                char* http_str = strstr(prev, "HTTP") + 4 + 1;

                sscanf(http_str, "%d.%d %d %s", &resp->version_major, &resp->version_minor, &resp->code, resp->desc);

                ctx->state = HR_HEADER_LINE;
            }
            break;	
        case HR_HEADER_LINE: // header after first line, "Connection : keep-alive"
            {
                char* separator = ": ";
                char* sep_addr = strstr(prev, separator);
                sep_addr[0] = '\0';

                if (http_parse_response_header_line(prev, sep_addr + 2, resp))
                {
                    hftp_log_err("cannot process %s:%s header line..\n", prev, sep_addr+2);
                }

                sep_addr[0] = separator[0];
            }
            break;
        }

        *next = separator[0];
    }

    if (ctx->remain_data_len > 0)
    {
        memmove(buffer, buffer + ctx->remain_data_index, ctx->remain_data_len);
    }
    // write body 
    else if (ctx->state == HR_BODY)
    {
        ctx->remain_data_index = (prev + 2) - buffer;
        ctx->remain_data_len = data_len - (prev - buffer) - 2;
    }

    return 0;
}

int http_parse_response_header_line(/*in*/char* name, /*in*/char* data, /*out*/struct http_response* resp)
{
    if (strcasecmp(g_common_headers[CMN_CONNECTION], name) == 0)
    {
        if (strcasecmp(data, "close") == 0)
            resp->connection = 0;
        else if (strcasecmp(data, "keep-alive") == 0)
            resp->connection = 1;
        else
            return 1;
    }
    else if (strcasecmp(g_common_headers[CMN_CONTENT_TYPE], name) == 0)
    {
        char* semicolon_ptr = strchr(data, ';');

        if (semicolon_ptr)
            *semicolon_ptr = '\0';

        resp->content_type_index = index_of_mime_type(data);

        if (semicolon_ptr)
            *semicolon_ptr = ';';
    }
    else if (strcasecmp(g_common_headers[CMN_CONTENT_LENGTH], name) == 0)
    {
        sscanf(data, "%lld", &resp->content_length);
    }
    else if (strcasecmp(g_response_headers[RSP_ACCEPT_RANGES], name) == 0)
    {
        if (strcasecmp("bytes", data) == 0)
            resp->accept_range = AR_BYTES;
        else
            resp->accept_range = AR_NOT_SUPPORTED;
    }
    else if (strcasecmp(g_common_headers[CMN_TRANSFER_ENCODING], name) == 0)
    {
        if (strcasecmp("chunked", data) == 0)
            resp->transfer_encoding = TE_CHUNKED;
        else
            resp->transfer_encoding = TE_NOT_SUPPORTED;
    }

    return 0;
}

int http_response_cleanup(struct http_response* resp)
{
    assert(resp != NULL);

    return 0;
}

int http_connection_init(struct http_connection* conn)
{
    assert(conn != NULL);

    memset(conn, 0, sizeof(struct http_connection));
    //conn->header_buffer = (char*)malloc(HTTP_CONN_HEADER_BUFFER_SIZE);
    
    if (conn == NULL)
    {
        hftp_log_err("fail to allocate buffer..");
        return 1;
    }

    return 0;
}

int http_connection_clear(struct http_connection* conn)
{
    memset(conn, 0, sizeof(struct http_connection) - sizeof(void*) * HTTP_CONN_DATA_PTR_NUM);
    //memset(conn->header_buffer, 0, HTTP_CONN_HEADER_BUFFER_SIZE);

    return 0;
}

int http_connection_destroy(struct http_connection* conn)
{
    assert(conn != NULL);

    http_response_cleanup(&conn->resp);
    
    //if (conn->data_buffer)      free(conn->data_buffer);
    //if (conn->header_buffer)    free(conn->header_buffer);

    return 0;
}

int http_internal_request(struct http_connection* conn, char* buffer, int buffer_capacity)
{
    assert(conn != NULL);
    assert(conn->sockfd > 2);
    assert(buffer != NULL);
    assert(buffer_capacity > 0);
    assert(conn->clb.req_write != NULL);

    int len = 0;
    while ((len = conn->clb.req_write(conn, buffer, buffer_capacity, conn->clb.req_write_param)) > 0)
    {
        write(conn->sockfd, buffer, len);
    }

    if (len < 0)
    {
        hftp_log_err("fail to write request..\n");
        return 1;
    }

    return 0;
}

int http_request(struct http_connection* conn, char* buffer, int buffer_capacity)
{
    assert(conn != NULL);
    assert(conn->sockfd > 2);
    assert(buffer != NULL);
    assert(buffer_capacity > 0);

    if (conn->clb.before_req)
    if (conn->clb.before_req(conn, conn->clb.before_req_param))
    {
        hftp_log_err("fail to call callback function \"before request\"\n");
        goto fail_request;
    }

    if (http_internal_request(conn, buffer, buffer_capacity))
    {
        hftp_log_err("fail to call callback function \"request\"\n");
        goto fail_request;
    }

    if (conn->clb.after_req)
    if (conn->clb.after_req(conn, conn->clb.after_req_param))
    {
        hftp_log_err("fail to call callback function \"after request\"\n");
        goto fail_request;
    }

    conn->state = 1;
    return 0;

fail_request:

    conn->state = -1;
    close(conn->sockfd);
    conn->sockfd = 0;
    return 1;
} 

int http_internal_response(struct http_connection* conn, char* buffer, int buffer_capacity);
int http_response(struct http_connection* conn, char* buffer, int buffer_capacity)
{
    assert(conn != NULL);
    assert(conn->sockfd > 2);
    assert(buffer != NULL);
    assert(buffer_capacity > 0);

    if (conn->clb.before_resp)
    if (conn->clb.before_resp(conn, conn->clb.before_resp_param))
    {
        hftp_log_err("fail to call callback function \"before response\"\n");
        goto fail_response;
    }

    if (http_internal_response(conn, buffer, buffer_capacity))
    {
        hftp_log_err("fail to call callback function \"response\"\n");
        goto fail_response;
    }


    if (conn->clb.after_resp)
    if (conn->clb.after_resp(conn, conn->clb.after_resp_param))
    {
        hftp_log_err("fail to call callback function \"after response\"\n");
        goto fail_response;
    }

    conn->state = 3;
    close(conn->sockfd);
    conn->sockfd = 0;
    return 0;

fail_response:

    conn->state = -1;
    close(conn->sockfd);
    conn->sockfd = 0;
    return 1;
}

int http_one_req_resp(struct http_connection* conn, char* buffer, int buffer_capacity)
{
    assert(conn != NULL);
    assert(buffer != NULL);
    assert(buffer_capacity > 0);

    if (conn->clb.before_req)
    if (conn->clb.before_req(conn, conn->clb.before_req_param))
    {
        hftp_log_err("fail to call callback function \"before request\"\n");
        return 1;
    }

    if (http_internal_request(conn, buffer, buffer_capacity))
    {
        hftp_log_err("fail to call callback function \"request\"\n");
        return 1;
    }

    if (conn->clb.after_req)
    if (conn->clb.after_req(conn, conn->clb.after_req_param))
    {
        hftp_log_err("fail to call callback function \"after request\"\n");
        return 1;
    }

    conn->state = 1;

    conn->state = 2;

    if (conn->clb.before_resp)
    if (conn->clb.before_resp(conn, conn->clb.before_resp_param))
    {
        hftp_log_err("fail to call callback function \"before response\"\n");
        return 1;
    }

    if (http_internal_response(conn, buffer, buffer_capacity))
    {
        hftp_log_err("fail to call callback function \"response\"\n");
        return 1;
    }

    if (conn->clb.after_resp)
    if (conn->clb.after_resp(conn, conn->clb.after_resp_param))
    {
        hftp_log_err("fail to call callback function \"after response\"\n");
        return 1;
    }

    conn->state = 0;

    return 0;
}



int http_chunked_response_body(
        struct http_connection* conn, char* buffer, int read_len, 
        int* continued, long long int* body_len, 
        int (*resp_body_func)(struct http_connection* resp, char* buffer, int buffer_len, void* param), void* body_param
)
{
    struct http_response_context* ctx = &conn->ctx;
    struct http_response* resp = &conn->resp;

    if (read_len == 0)
    {
        *continued = 0;
        return 0;
    }

    assert(conn != NULL);
    assert(buffer != NULL);
    assert(read_len > 0);
    //assert(resp_body_func != NULL);

    int remain_data_index = ctx->remain_data_index;
    int remain_data_len = ctx->remain_data_len? ctx->remain_data_len: read_len;
    int entering = 1;

    ctx->remain_data_index = ctx->remain_data_len = 0;

    switch (ctx->chunk_terminate_state)
    {
        case 0:
            goto remain_size_byte;
        case 1:
            goto remain_separator_mid;
        case 2:
            goto remain_chunk;
        case 3:
            goto remain_separator_end;
    }

    while (1)
    {
remain_size_byte:
        ctx->chunk_terminate_state = 0;

        if (entering)
        {
            int prev_len = strlen(ctx->chunk_size_str);
            
            if (4 - prev_len > remain_data_len)
            {
                int i;
                for (i = prev_len; i < 4; i++)
                    ctx->chunk_size_str[i] = buffer[remain_data_index + (i - prev_len)];
                ctx->chunk_size_str[i] = '\0';

                remain_data_index += remain_data_len;
                remain_data_len = 0;
                
                *continued = 1;
                break;
            }
            else
            {
                int i ;
                for (i = prev_len; i < 4; i++)
                    ctx->chunk_size_str[i] = buffer[remain_data_index + (i - prev_len)];
 
                if (ctx->chunk_size_str[0] == '0' && 
                    ctx->chunk_size_str[1] == '\r' &&
                    ctx->chunk_size_str[2] == '\n')
                {
                    ctx->chunk_size_str[0] = '0';
                    ctx->chunk_size_str[1] = '\0';
                    ctx->chunk_size = 0;
                    *continued = 0;
                    break;
                }               
                
                ctx->chunk_size_str[i] = '\0';

                remain_data_index += 4 - prev_len;
                remain_data_len -= 4 - prev_len;
            }

            entering = 0;
        }
        else
        {
            if (buffer[remain_data_index + 0] == '0' && 
                buffer[remain_data_index + 1] == '\r' &&
                buffer[remain_data_index + 2] == '\n')
            {
                ctx->chunk_size_str[0] = '0';
                ctx->chunk_size_str[1] = '\0';
                ctx->chunk_size = 0;
                *continued = 0;
                break;
            }
            if (remain_data_len < 4)
            {
                int i;
                for (i = 0; i < remain_data_len; i++)
                    ctx->chunk_size_str[i] = buffer[remain_data_index + i];
                ctx->chunk_size_str[i] = '\0';

                remain_data_len -= i;
                remain_data_index += i;

                *continued = 1;
                break;
            }
            else
            {
                int i ;
                for (i = 0; i < 4; i++)
                    ctx->chunk_size_str[i] = buffer[remain_data_index + i];
                ctx->chunk_size_str[i] = '\0';

                remain_data_index += 4;
                remain_data_len -= 4;
            }
        }

        {
            char* ep;
            ctx->chunk_size = strtol(ctx->chunk_size_str, &ep, 16);
        }

remain_separator_mid:
        ctx->chunk_terminate_state = 1;

        if (entering)
        {
            if (ctx->chunk_state_remain_byte > remain_data_len)
            {
                ctx->chunk_state_remain_byte = ctx->chunk_state_remain_byte - remain_data_len; 
                *continued = 1;
                break;
            }

            remain_data_len -= ctx->chunk_state_remain_byte; 
            remain_data_index += ctx->chunk_state_remain_byte; 
            ctx->chunk_state_remain_byte = 0;

            entering = 0;
        }
        else
        {
            if (remain_data_len < 2)
            {
                ctx->chunk_state_remain_byte = 2 - remain_data_len;
                *continued = 1;
                break;
            }

            remain_data_len -= 2;
            remain_data_index += 2;
        }
 
remain_chunk:

        ctx->chunk_terminate_state = 2;

        if (entering)
        {
            if (remain_data_len < ctx->chunk_state_remain_byte)
            {
                ctx->chunk_state_remain_byte = ctx->chunk_state_remain_byte - remain_data_len;

                if (conn->clb.resp_body_func != NULL)
                    conn->clb.resp_body_func(conn, buffer + remain_data_index, remain_data_len, body_param);

                *body_len += remain_data_len;
                remain_data_len -= remain_data_len;
                remain_data_index += remain_data_len;
                *continued = 1;
                break;
            }

            if (conn->clb.resp_body_func != NULL)
                conn->clb.resp_body_func(conn, buffer + remain_data_index, ctx->chunk_state_remain_byte, body_param);

            *body_len += ctx->chunk_state_remain_byte;
            remain_data_len -= ctx->chunk_state_remain_byte;
            remain_data_index += ctx->chunk_state_remain_byte;

            entering = ctx->chunk_state_remain_byte = 0;
        }
        else
        {
            if (remain_data_len < ctx->chunk_size)
            {
                ctx->chunk_state_remain_byte = ctx->chunk_size - remain_data_len;

                if (conn->clb.resp_body_func != NULL)
                    conn->clb.resp_body_func(conn, buffer + remain_data_index, remain_data_len, body_param);

                *body_len += remain_data_len;
                remain_data_len -= remain_data_len;
                remain_data_index += remain_data_len;
                *continued = 1;
                break;
            }

            if (conn->clb.resp_body_func != NULL)
                conn->clb.resp_body_func(conn, buffer + remain_data_index, ctx->chunk_size, body_param);

            *body_len += ctx->chunk_size;
            remain_data_len -= ctx->chunk_size;
            remain_data_index += ctx->chunk_size;
        }
        
remain_separator_end:
        ctx->chunk_terminate_state = 3;

        if (entering)
        {
            if (ctx->chunk_state_remain_byte > remain_data_len)
            {
                ctx->chunk_state_remain_byte = ctx->chunk_state_remain_byte - remain_data_len; 
                *continued = 1;
                break;
            }

            remain_data_len -= ctx->chunk_state_remain_byte; 
            remain_data_index += ctx->chunk_state_remain_byte; 
            ctx->chunk_state_remain_byte = 0;

            entering = 0;
        }
        else
        {
            if (remain_data_len < 2)
            {
                ctx->chunk_state_remain_byte = 2 - remain_data_len;
                *continued = 1;
                break;
            }

            remain_data_len -= 2;
            remain_data_index += 2;
        }
   }

    return 0;
}

int http_internal_response(struct http_connection* conn, char* buffer, int buffer_capacity)
{
    assert(conn != NULL);
    assert(conn->sockfd > 2);
    assert(buffer != NULL);
    assert(buffer_capacity > 0);

    int header_end_flag = 0, read_len = 0;
    long long int body_len = 0;
    int forced_continue = 0, read_offset = 0;

    conn->ctx.state = 0;

    while ((read_len = read(conn->sockfd, buffer + conn->ctx.remain_data_len, buffer_capacity - conn->ctx.remain_data_len)) > 0 || forced_continue)
	{
        if (read_len < 0)
        {
            hftp_log_err("fail to read network input..\n");
            return 1;
        }

        if (conn->ctx.state < HR_BODY)
        {
            http_parse_response_header_per_buffer(buffer, read_len, &conn->ctx, &conn->resp);
        }
        
        if (HR_BODY == conn->ctx.state)
        {
            if (header_end_flag == 0)
            {
                if (conn->resp.transfer_encoding == TE_CHUNKED)
                {
                    forced_continue = 1;
                    conn->ctx.chunk_terminate_state = 0;
                }

                if (conn->clb.resp_header_func != NULL)
                    conn->clb.resp_header_func(conn, conn->clb.resp_header_param);

                header_end_flag = 1;

                if (conn->resp.transfer_encoding == TE_CHUNKED)
                {
                    http_chunked_response_body(conn, buffer, read_len, &forced_continue, &body_len, conn->clb.resp_body_func, conn->clb.resp_body_param);
                }
                else if (conn->resp.transfer_encoding == TE_NONE)
                {
                    if (conn->ctx.remain_data_len > 0)
                    {
                        body_len += conn->ctx.remain_data_len;

                        if (conn->clb.resp_body_func != NULL)
                            conn->clb.resp_body_func(conn, buffer + conn->ctx.remain_data_index, conn->ctx.remain_data_len, conn->clb.resp_body_param);

                        conn->ctx.remain_data_len = conn->ctx.remain_data_index = 0;
                    }
                }            
            }
            else
            {
                if (conn->resp.transfer_encoding == TE_CHUNKED)
                {
                    http_chunked_response_body(conn, buffer, read_len, &forced_continue, &body_len, conn->clb.resp_body_func, conn->clb.resp_body_param);
                }
                else if (conn->resp.transfer_encoding == TE_NONE)
                {
                    body_len += read_len;

                    if (conn->clb.resp_body_func != NULL)
                        conn->clb.resp_body_func(conn, buffer, read_len, conn->clb.resp_body_param);
                }
            }
            
            if (conn->resp.transfer_encoding == TE_NONE)
            if (conn->resp.content_length > 0 && conn->resp.content_length <= body_len)
            {
                if (body_len != conn->resp.content_length)
                {
                    hftp_log_err("Content-Length(%lld) != Reponse's Body Length(%lld)..\n", conn->resp.content_length, body_len);
                    return 1;
                }

                conn->ctx.state = HR_END;
                break;
            }

            if (conn->resp.transfer_encoding == TE_CHUNKED)
                if (conn->ctx.chunk_size == 0)
                {
                    conn->ctx.state = HR_END;
                    break;
                }
        }
   	}

    return 0;
}

