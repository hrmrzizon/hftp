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

#include "http_method.h"

#include "http_core.h"
#include "url.h"
#include "utilities.h"
#include "hftp.h"
#include "mime_type.h"

int (*g_param_set_func_arr[])(struct http_callback*, void*) = {  
        http_get_param_set,
        http_put_param_set,
        http_propfind_param_set,
        http_delete_param_set,
    };

/* PUT method implements  */
int http_put_before_request(struct http_connection* conn, void* param)
{
    assert(param != NULL);

    struct http_put_chunk* c = (struct http_put_chunk*)param;

    conn->filefd = open(conn->local_dir, O_RDONLY, S_IRWXU | S_IRWXG | S_IRWXO);

    if (conn->filefd < 0)
    {  
        hftp_log_err("cannot open directory(%s) for read..\n", conn->local_dir);   
        return 1;
    }

    return 0;
}

int http_put_request_write(struct http_connection* conn, char* buffer, int buffer_capacity, void* param)
{
    assert(buffer != NULL);
    assert(buffer_capacity > 0);
    assert(param != NULL);

    struct http_put_chunk* c = (struct http_put_chunk*)param;

    assert(conn->filefd > 2);

    if (!c->req_header_write_fin)
    {
        struct stat st;

        if (fstat(conn->filefd, &st))
            return -1;

        const char* req_fmt = 	"PUT %s HTTP/1.1\r\n"
                                "Host: %s\r\n"
                                "User-Agent: %s/%s\r\n"
                                "Accept: */*\r\n"
                                "Connection: %s\r\n"
                                "Content-Length: %d\r\n"
                                "Content-Type: %s\r\n"
    //                                "Authorization: Basic dXNlcjE6MTIzNA=="
                                "\r\n";

        int formatted_len = sprintf(
                buffer, req_fmt, 
                conn->remote_dir, conn->domain, HFTP_APP_NAME, HFTP_APP_VERSION,
                c->close? "close": "keep-alive",
                st.st_size, find_mime_type(get_file_extension(conn->local_dir))
                );

        if (g_debug_log)
            printf("hftp request header\n%s\n", buffer);

        c->req_header_write_fin = 1;
        return formatted_len;
    }
    else
    {
        return read(conn->filefd, buffer, buffer_capacity);
    }
}
int http_put_after_request(struct http_connection* conn, void* param)
{
    struct http_put_chunk* c = (struct http_put_chunk*)param;

    close(conn->filefd);

    return 0;
}
int http_put_before_resp(struct http_connection* conn, void* param)
{
    struct http_put_chunk* c = (struct http_put_chunk*)param;

    return 0;
}
int http_put_response_header(struct http_connection* conn, void* param)
{
    struct http_put_chunk* c = (struct http_put_chunk*)param;

    return 0;
}
int http_put_response_body(struct http_connection* conn, char* buffer, int buffer_len, void* param)
{
    struct http_put_chunk* c = (struct http_put_chunk*)param;

    return 0;
}

int http_put_after_resp(struct http_connection* conn, void* param)
{
    struct http_put_chunk* c = (struct http_put_chunk*)param;

    if (conn->resp.code == HTTP_RESPONSE_CODE(SUC_NO_CONTENT))
        hftp_log_out("\'put\' is executed but cannot put \'%s\'\n", conn->remote_dir);
    else if (
            conn->resp.code == HTTP_RESPONSE_CODE(SUC_CREATED) || 
            conn->resp.code == HTTP_RESPONSE_CODE(SUC_OK)
            )
        hftp_log_out("\'put\' successfully executed!(%s)\n", conn->remote_dir);
    else
        hftp_log_err("\'put\' has been failed, code:%d\n", conn->resp.code);

    return 0;
}

int http_put_param_set(struct http_callback* clb, void* chunk_pt)
{
    struct http_put_chunk* chunk = chunk_pt;

    memset(clb, 0, sizeof(clb));

    clb->before_req             = http_put_before_request;
    clb->req_write              = http_put_request_write;
    clb->after_req              = http_put_after_request;
    clb->before_resp            = http_put_before_resp;
    clb->resp_header_func       = http_put_response_header;
    clb->resp_body_func         = http_put_response_body;
    clb->after_resp             = http_put_after_resp;

    clb->before_req_param = clb->req_write_param = clb->after_req_param =
    clb->before_resp_param = clb->resp_header_param = clb->resp_body_param = clb->after_resp_param = chunk;
    return 0;
}

/* GET method implements */
int http_get_before_request(struct http_connection* conn, void* param)
{
    struct http_get_chunk* c = (struct http_get_chunk*)param;

    return 0;
}
int http_get_request_write(struct http_connection* conn, char* buffer, int buffer_capacity, void* param)
{
    assert(buffer != NULL);
    assert(buffer_capacity > 0);
    assert(param != NULL);

    struct http_get_chunk* c = (struct http_get_chunk*)param;

    if (c->req_write_once) return 0;

    const char* req_fmt = 	"GET %s HTTP/1.1\r\n"
                            "Host: %s\r\n"
                            "User-Agent: %s/%s\r\n"
                            "Connection: %s\r\n"
//                                "Authorization: Basic dXNlcjE6MTIzNA=="
                            "\r\n";

    int formatted_len = sprintf(
            buffer, req_fmt, 
            conn->remote_dir, conn->domain, HFTP_APP_NAME, HFTP_APP_VERSION, c->close? "close": "keep-alive"
            );

    c->req_write_once = 1;

    return formatted_len;
}
int http_get_after_request(struct http_connection* conn, void* param)
{
    struct http_get_chunk* c = (struct http_get_chunk*)param;

    return 0;
}
int http_get_before_resp(struct http_connection* conn, void* param)
{
    struct http_get_chunk* c = (struct http_get_chunk*)param;

    return 0;
}
int http_get_response_header(struct http_connection* conn, void* param)
{
    struct http_get_chunk* c = (struct http_get_chunk*)param;

    if (conn->resp.code == HTTP_RESPONSE_CODE(SUC_OK))
    {
        conn->filefd = open(conn->local_dir, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);

        if (conn->filefd < 0)
        {  
            hftp_log_err("cannot open directory(%s) for write..\n", conn->local_dir);   
            return 1;
        }
    }
    else
    {
        hftp_log_out("other response code : %d\n", conn->resp.code);
    }

    c->req_write_once = 0;

    return 0;
}
int http_get_response_body(struct http_connection* conn, char* buffer, int buffer_len, void* param)
{
    struct http_get_chunk* c = (struct http_get_chunk*)param;

    if (conn->resp.code == HTTP_RESPONSE_CODE(SUC_OK))
    {
        if (conn->filefd <= 2)
        {
            hftp_log_err("file descriptor is std..\n");
            return 1;
        }
            
        write(conn->filefd, buffer, buffer_len);
    }

    return 0;
}

int http_get_after_resp(struct http_connection* conn, void* param)
{
    struct http_get_chunk* c = (struct http_get_chunk*)param;

    if (conn->resp.code == HTTP_RESPONSE_CODE(SUC_OK))
    {
        close(conn->filefd);
        conn->filefd = 0;

        hftp_log_out("\'get\' successfully executed!(%s)\n", conn->local_dir);
    }
    else
        hftp_log_err("\'get\' has been failed, code:%d\n", conn->resp.code);

    return 0;
}

int http_get_param_set(struct http_callback* clb, void* chunk_pt)
{
    struct http_get_chunk* chunk = (struct http_get_chunk*)chunk_pt;

    memset(clb, 0, sizeof(clb));

    clb->before_req             = http_get_before_request;
    clb->req_write              = http_get_request_write;
    clb->after_req              = http_get_after_request;
    clb->before_resp            = http_get_before_resp;
    clb->resp_header_func       = http_get_response_header;
    clb->resp_body_func         = http_get_response_body;
    clb->after_resp             = http_get_after_resp;

    clb->before_req_param = clb->req_write_param = clb->after_req_param =
    clb->before_resp_param = clb->resp_header_param = clb->resp_body_param = clb->after_resp_param = chunk;
    return 0;
}

int http_propfind_before_request(struct http_connection* conn, void* param)
{
    struct http_propfind_chunk* c = (struct http_propfind_chunk*)param;


    return 0;
}
int http_propfind_request_write(struct http_connection* conn, char* buffer, int buffer_capacity, void* param)
{
    assert(buffer != NULL);
    assert(buffer_capacity > 0);
    assert(param != NULL);
 
    struct http_propfind_chunk* c = (struct http_propfind_chunk*)param;

    const char* req_fmt = 	"PROPFIND %s HTTP/1.1\r\n"
                            "Host: %s\r\n"
                            "User-Agent: %s/%s\r\n"
                            "Connection: %s\r\n"
                            "Depth: 1\r\n"
//                                "Authorization: Basic dXNlcjE6MTIzNA=="
                            "\r\n"
                            "<?xml "
                            "version=\"1.0\" "
                            "encoding=\"utf-8\" "
                            "?>"
                            "<propfind xmlns=\"DAV:\">"
                            "<allprop/>"
                            "</propfind>"
                            ;

    int formatted_len = sprintf(
            buffer, req_fmt, 
            conn->remote_dir, conn->domain, HFTP_APP_NAME, HFTP_APP_VERSION, c->close? "close": "keep-alive"
            );

    return 0;
}
int http_propfind_after_request(struct http_connection* conn, void* param)
{
    struct http_propfind_chunk* c = (struct http_propfind_chunk*)param;

    return 0;
}
int http_propfind_before_resp(struct http_connection* conn, void* param)
{
    struct http_propfind_chunk* c = (struct http_propfind_chunk*)param;

    return 0;
}
int http_propfind_response_header(struct http_connection* conn, void* param)
{
    struct http_propfind_chunk* c = (struct http_propfind_chunk*)param;


    return 0;
}
int http_propfind_response_body(struct http_connection* conn, char* buffer, int buffer_len, void* param)
{
    struct http_propfind_chunk* c = (struct http_propfind_chunk*)param;

    return 0;
}
int http_propfind_after_resp(struct http_connection* conn, void* param)
{
    struct http_propfind_chunk* c = (struct http_propfind_chunk*)param;

    return 0;
}
int http_propfind_param_set(struct http_callback* clb, void* chunk_ptr)
{
    struct http_propfind_chunk* chunk = (struct http_propfind_chunk*)chunk_ptr;

    memset(clb, 0, sizeof(clb));

    clb->before_req             = http_propfind_before_request;
    clb->req_write              = http_propfind_request_write;
    clb->after_req              = http_propfind_after_request;
    clb->before_resp            = http_propfind_before_resp;
    clb->resp_header_func       = http_propfind_response_header;
    clb->resp_body_func         = http_propfind_response_body;
    clb->after_resp             = http_propfind_after_resp;

    clb->before_req_param = clb->req_write_param = clb->after_req_param =
    clb->before_resp_param = clb->resp_header_param = clb->resp_body_param = clb->after_resp_param = chunk;
 
    return 0;
}

int http_delete_before_request(struct http_connection* conn, void* param)
{
    struct http_delete_chunk* c = (struct http_delete_chunk*)param;


    return 0;
}
int http_delete_request_write(struct http_connection* conn, char* buffer, int buffer_capacity, void* param)
{
    assert(buffer != NULL);
    assert(buffer_capacity > 0);
    assert(param != NULL);
 
    struct http_delete_chunk* c = (struct http_delete_chunk*)param;

    const char* req_fmt = 	"DELETE %s HTTP/1.1\r\n"
                            "Host: %s\r\n"
                            "User-Agent: %s/%s\r\n"
                            "Connection: %s\r\n"
                            "\r\n";

    int formatted_len = sprintf(
            buffer, req_fmt, 
            conn->remote_dir, conn->domain, HFTP_APP_NAME, HFTP_APP_VERSION, c->close? "close": "keep-alive"
            );

    return 0;
}
int http_delete_after_request(struct http_connection* conn, void* param)
{
    struct http_delete_chunk* c = (struct http_delete_chunk*)param;

    return 0;
}
int http_delete_before_resp(struct http_connection* conn, void* param)
{
    struct http_delete_chunk* c = (struct http_delete_chunk*)param;

    return 0;
}
int http_delete_response_header(struct http_connection* conn, void* param)
{
    struct http_delete_chunk* c = (struct http_delete_chunk*)param;

    return 0;
}
int http_delete_response_body(struct http_connection* conn, char* buffer, int buffer_len, void* param)
{
    struct http_delete_chunk* c = (struct http_delete_chunk*)param;

    return 0;
}
int http_delete_after_resp(struct http_connection* conn, void* param)
{
    struct http_delete_chunk* c = (struct http_delete_chunk*)param;

    return 0;
}
int http_delete_param_set(struct http_callback* clb, void* chunk_ptr)
{
    struct http_delete_chunk* chunk = (struct http_delete_chunk*)chunk_ptr;
    memset(chunk, 0, sizeof(struct http_delete_chunk));

    memset(clb, 0, sizeof(clb));

    clb->before_req             = http_delete_before_request;
    clb->req_write              = http_delete_request_write;
    clb->after_req              = http_delete_after_request;
    clb->before_resp            = http_delete_before_resp;
    clb->resp_header_func       = http_delete_response_header;
    clb->resp_body_func         = http_delete_response_body;
    clb->after_resp             = http_delete_after_resp;

    clb->before_req_param = clb->req_write_param = clb->after_req_param =
    clb->before_resp_param = clb->resp_header_param = clb->resp_body_param = clb->after_resp_param = chunk;
    return 0;
}

int method_to_name(int method, char* buffer)
{
    assert(buffer != NULL);

    switch (method)
    {
        case 0:
            strncpy(buffer, "get", 3);
            return 0;
        case 1:
            strncpy(buffer, "put", 3);
            return 0;
        case 2:
            strncpy(buffer, "propfind", 8);
            break;
        case 3:
            strncpy(buffer, "delete", 9);
            break;
        default:
            return 1;
    }

    return 0;
}

int name_to_method(char* name, int* method)
{
    assert(name != NULL);
    assert(method != NULL);

    if (strcasecmp(name, "get") == 0)
    {
        *method = 0;
        return 0;
    }
    else if (strcasecmp(name, "put") == 0)
    {
        *method = 1;
        return 0;
    }
    else if (strcasecmp(name, "propfind") == 0)
    {
        *method = 2;
        return 0;
    }
    else if (strcasecmp(name, "delete") == 0)
    {
        *method = 3;
        return 0;
    }
    return 1;
}

