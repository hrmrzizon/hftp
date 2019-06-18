#pragma once

extern const char*                      g_commomn_headers[];
extern const char*                      g_request_headers[];
extern const char*                      g_response_headers[];

enum response_code
{
	INF_CONTINUE						,
	INF_SWITCHING_PROTOCOLS				,

	SUC_OK								,
	SUC_CREATED							,
	SUC_ACCEPTED						,
	SUC_NON_AUTHORITATIVE_INFORM 		,
	SUC_NO_CONTENT						,
	SUC_RESET_CONTENT					,
	SUC_PARTIAL_CONTENT				    ,

	RDR_MULTIPLE_CHOICES				,
	RDR_MOVED_PERMANANTLY				,
	RDR_FOUND							,
	RDR_SEE_OTHER						,
	RDR_NOT_MODIFIED					,
	RDR_TEMPORARY_REDIRECT				,
	RDR_PERMANANT_REDIRECT				,

	CER_BAD_REQUEST						,
	CER_UNAUTHORIZED					,
	CER_FORBIDDENT						,
	CER_NOT_FOUND						,
	CER_METHOD_NOT_ALLOWED				,
	CER_NOT_ACCEPTABLE					,
	CER_PROXY_AUTHENTICATION_REQUIRED	,
	CER_REQUEST_TIMEOUT					,
	CER_CONFLICT						,
	CER_GONE							,
	CER_LENGTH_REQUIRED					,
	CER_PRECONDITION_FAILED				,
	CER_PAYLOAD_TOO_LARGE				,
	CER_URI_TOO_LONG					,
	CER_UNSUPPORTED_MEDIA_TYPE			,
	CER_RANGE_NOT_SATISFIABLE			,
	CER_EXPECTATION_FAILED				,
	CER_IM_A_TEAPOT						,
	CER_UNPROCESSABLE_ENTITY			,
	CER_TOO_EARLY						,
	CER_UPGRADE_REQUIRED				,
	CER_PRECONDITION_REQUIRED			,
	CER_TOO_MANY_REQUESTS				,
	CER_REQUEST_HEADER_FIELDS_TOO_LARGE	,
	CER_UNAVAILABLE_FOR_LEGAL_REASONS	,
	
	SER_INTERNAL_SERVER_ERROR			,
	SER_NOT_IMPLEMENTED					,
	SER_BAD_GATEWAY						,
	SER_SERVICE_UNAVAILABLE				,
	SER_GATEWAY_TIMEOUT					,
	SER_HTTP_VERSION_NOT_SUPPORTED		,
	SER_NETWORK_AUTHENTICATION_REQUIRED	,

    RESPONSE_CODE_MAX                   ,
};

extern const int                        g_resp_codes[];

#define HTTP_RESPONSE_CODE(x)           (g_resp_codes[x])

enum transfer_encoding
{
    TE_NONE                             = 0,
    TE_CHUNKED                          = 1,
    TE_NOT_SUPPORTED                    = -1,
};

enum accept_range
{
    AR_BYTES                            = 0,
    AR_NOT_SUPPORTED                    = -1,
};

#define RESPONSE_CODE_DESC_MAX 63

struct http_response//_header
{
    // header: start-line
    int                                 version_major;
    int                                 version_minor;
    int                                 charset;
    int                                 code;
    char                                desc[RESPONSE_CODE_DESC_MAX + 1];

    // header: (key): (data)
    int                                 connection; /* close=0, keep-alive=1 */
    int                                 content_type_index;
    long long int                       content_length;
    enum transfer_encoding              transfer_encoding;
    enum accept_range                   accept_range;
    /* omitted.. */    
    
};

enum http_response_parse_state
{
    HR_FIRST_LINE                       = 0,
    HR_HEADER_LINE                      = 1,
    HR_BODY                             = 2,
    HR_END                              = 3,
};

struct http_response_context
{
    /*internal*/
    enum http_response_parse_state      state;              // state for parse header
    /*out*/
    unsigned int                        remain_data_index;  // remain data buffer index. no '\r\n' or body.
    unsigned int                        remain_data_len;    // remain data buffer length. no '\r\n' or body.
    int                                 chunk_size;
    int                                 chunk_terminate_state;
    int                                 chunk_state_remain_byte;
    char                                chunk_size_str[5];
};

int http_parse_response_header_per_buffer(
        /* in   */char* buffer, 
        /* in   */int maxlen, 
        /* out  */struct http_response_context* ctx,
        /* out  */struct http_response* resp
        );
int http_parse_response_header_line(/*in*/char* name, char* data, /*out*/struct http_response* resp);
int http_response_cleanup(struct http_response* resp);

struct http_connection;

struct http_callback
{
    int (*before_req)                   (struct http_connection*, void*);
    int (*req_write)                    (struct http_connection*, char*, int, void*);
    int (*after_req)                    (struct http_connection*, void*);
    int (*before_resp)                  (struct http_connection*, void*);
    int (*resp_header_func)             (struct http_connection*, void*);
    int (*resp_body_func)               (struct http_connection*, char*, int, void*);
    int (*after_resp)                   (struct http_connection*, void*);

    void* before_req_param;
    void* req_write_param;
    void* after_req_param;
    void* before_resp_param;
    void* resp_header_param;
    void* resp_body_param;
    void* after_resp_param;
};

#define                                 HTTP_CONN_DATA_PTR_NUM          2
#define                                 HTTP_CONN_DATA_BUFFER_SIZE      8184
#define                                 HTTP_CONN_HEADER_BUFFER_SIZE    8184


#define                                 DOMAIN_MAX_LEN                  256
#define                                 REMOTE_DIR_MAX_LEN              256
#define                                 LOCAL_DIR_MAX_LEN               256

struct http_connection
{
    int                                 method, state, sockfd, filefd;

    struct http_response                resp;
    struct http_response_context        ctx;
    struct http_callback                clb;
    
    double                              progress;

    char                                domain[DOMAIN_MAX_LEN],
                                        remote_dir[REMOTE_DIR_MAX_LEN],
                                        local_dir[LOCAL_DIR_MAX_LEN];
};

int http_connection_init(struct http_connection* conn);
int http_connection_clear(struct http_connection* conn);
int http_connection_destroy(struct http_connection* conn);

int http_request(struct http_connection* conn, char* buffer, int buffer_capacity);
int http_response(struct http_connection* conn, char* buffer, int buffer_capacity);

int http_one_req_resp(struct http_connection* conn, char* buffer, int buffer_capacity);


