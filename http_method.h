#pragma once

struct http_response;
struct http_connection;
struct http_callback;

/*
   methods

   PUT              OK
   GET              OK

   WebDAV Extension methods

   PROPFIND         NO
   PROPPATCH        NO
 */

struct http_put_chunk
{
    /*req*/
    int close;

    /*internal*/
    int req_header_write_fin;

    int sock_err_code;

    int chunked_transfer;
};

int http_put_before_request(struct http_connection* conn, void* param);
int http_put_request_write(struct http_connection* conn, char* buffer, int buffer_capacity, void* param);
int http_put_after_request(struct http_connection* conn, void* param);
int http_put_before_resp(struct http_connection* conn, void* param);
int http_put_response_header(struct http_connection* conn, void* param);
int http_put_response_body(struct http_connection* conn, char* buffer, int buffer_len, void* param);
int http_put_after_resp(struct http_connection* conn, void* param);
int http_put_param_set(struct http_callback* clb, void* chunk_pt);

struct http_get_chunk
{
    /*req*/
    int close;

    /*internal*/
    int req_write_once;

    int sock_err_code;
};

int http_get_before_request(struct http_connection* conn, void* param);
int http_get_request_write(struct http_connection* conn, char* buffer, int buffer_capacity, void* param);
int http_get_after_request(struct http_connection* conn, void* param);
int http_get_before_resp(struct http_connection* conn, void* param);
int http_get_response_header(struct http_connection* conn, void* param);
int http_get_response_body(struct http_connection* conn, char* buffer, int buffer_len, void* param);
int http_get_after_resp(struct http_connection* conn, void* param);
int http_get_param_set(struct http_callback* clb, void* chunk_pt);

struct http_propfind_chunk
{
    int                             close;
};

int http_propfind_before_request(struct http_connection* conn, void* param);
int http_propfind_request_write(struct http_connection* conn, char* buffer, int buffer_capacity, void* param);
int http_propfind_after_request(struct http_connection* conn, void* param);
int http_propfind_before_resp(struct http_connection* conn, void* param);
int http_propfind_response_header(struct http_connection* conn, void* param);
int http_propfind_response_body(struct http_connection* conn, char* buffer, int buffer_len, void* param);
int http_propfind_after_resp(struct http_connection* conn, void* param);
int http_propfind_param_set(struct http_callback* clb, void* chunk_ptr);

struct http_delete_chunk
{
    int                             close;
};

int http_delete_before_request(struct http_connection* conn, void* param);
int http_delete_request_write(struct http_connection* conn, char* buffer, int buffer_capacity, void* param);
int http_delete_after_request(struct http_connection* conn, void* param);
int http_delete_before_resp(struct http_connection* conn, void* param);
int http_delete_response_header(struct http_connection* conn, void* param);
int http_delete_response_body(struct http_connection* conn, char* buffer, int buffer_len, void* param);
int http_delete_after_resp(struct http_connection* conn, void* param);
int http_delete_param_set(struct http_callback* clb, void* chunk_ptr);

union method_chunks
{
    struct http_get_chunk           get;
    struct http_put_chunk           put;
    struct http_propfind_chunk      propfind;
    struct http_delete_chunk        del;
};

int method_to_name(int method, char* buffer);
int name_to_method(char* name, int* method);

