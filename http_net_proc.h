#pragma once

struct sockaddr_in;
struct url_data;
struct http_connection;

int process_method(struct sockaddr_in* in, struct url_data* data, char* local_dir, struct http_connection* conn, int method);
int http_conn_proc(char* method_str, char* remote_url_str, char* local_dir_str);

