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

#include <time.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>

#include "hftp.h"
#include "http_core.h"
#include "http_method.h"
#include "threads.h"

#include "utilities.h"

#include "url.h"

extern int                  g_debug_log;

#define                     MIN(a,b)                                (((a)<(b))?(a):(b))
#define                     MAX(a,b)                                (((a)>(b))?(a):(b))


#define                     INPUT_BUFFER_COUNT                      4096

char                        g_input_buffer[INPUT_BUFFER_COUNT];
pthread_t                   g_input_thread;

int                         g_fixed_input;
pthread_mutex_t             g_remain_count_mutex;
int                         g_remain_count;

pthread_mutex_t             g_continue_input_mutex;
pthread_cond_t              g_continue_input_cond;
int                         g_continue_input_flag;

/*
    epoll event patcher variable declaration
    */

#define                     EPOLL_INSTANCE_MAX_SOCKFD               512


pthread_t                   g_epoll_patcher_thread;
int                         g_epoll_patcher_cancel;
int                         g_epoll_inst_desc;
struct epoll_event          g_epoll_event_buffer[EPOLL_INSTANCE_MAX_SOCKFD];

struct http_connection      g_conn_workspace[EPOLL_INSTANCE_MAX_SOCKFD];


/* 
   workspace variable declaration

   1. event queue
   2. work thead
   */


#define                     EPOLL_EVENT_QUEUE_MAX                   256

pthread_rwlock_t            g_epoll_event_queue_rwlock;
int                         g_epoll_event_queue_count;
int                         g_epoll_event_queue_start_index;
struct epoll_event          g_epoll_event_queue[EPOLL_EVENT_QUEUE_MAX];

#define                     CONN_EVENT_QUEUE_MAX                    256

pthread_rwlock_t            g_conn_event_queue_rwlock;
int                         g_conn_event_queue_count;
int                         g_conn_event_queue_start_index;
struct conn_event           g_conn_event_queue[CONN_EVENT_QUEUE_MAX];

#define                     DEFAULT_WORKER_COUNT                    8

int                         g_workspace_count;
struct thread_workspace*    g_workspace;
pthread_rwlock_t            g_thread_work_flag_rwlock;
long long int               g_thread_work_flag;

int get_idle_work_thread_index_not_safety()
{
    for (int i = 0; i < MIN(64, g_workspace_count); i++)
        if (!(g_thread_work_flag & (0x01 << i)))
            return i;

    return -1;
}

extern int (*g_param_set_func_arr[])(struct http_callback*, void*);

void signal_display(int signal)
{
    if (!g_debug_log) return;

    hftp_log_out("singal: %d\n", signal);
    hftp_log_out("conn: %d\n", g_conn_event_queue_count);
    hftp_log_out("epoll: %d\n", g_epoll_event_queue_count);

    for (int i = 0; i < g_workspace_count; i++)
    {
        hftp_log_out("workspace%d: conn_index:%d, http handler:%s\n", i, g_workspace[i].conn_index, g_workspace[i].http_process == NULL? "null": g_workspace[i].http_process == http_request? "request handler": "response_handler");
        if (g_workspace[i].conn_index >= 0)
        {
            struct http_connection* conn = g_conn_workspace + g_workspace[i].conn_index;
            hftp_log_out("conn state:%d sockfd:%d, filefd:%d\n", conn->state, conn->sockfd, conn->sockfd);
        }
    }
}

int input_init()
{
    signal(SIGABRT, signal_display);
    signal(SIGINT,  signal_display);
    signal(SIGQUIT, signal_display);

    g_continue_input_flag = 0;
    pthread_mutex_init(&g_continue_input_mutex, NULL);
    pthread_cond_init(&g_continue_input_cond, NULL);
    
    g_fixed_input = 0;

    struct epoll_event ev;
    ev.data.fd = 0;
    ev.events = EPOLLIN;
    if (epoll_ctl(g_epoll_inst_desc, EPOLL_CTL_ADD, 0, &ev))
    {
        if (errno == EPERM)
        {
            // stdin is fixed.. 
            
            pthread_mutex_init(&g_remain_count_mutex, NULL);

            g_fixed_input = 1;
            input_handling();
        }
        else
        {
            hftp_log_err("fail to add stdin to epoll instance\n");
            return 1;
        }
    }

    return 0;
}

int input_cond_blocking()
{
    while (!g_continue_input_flag)
    {
        pthread_mutex_lock(&g_continue_input_mutex);
        pthread_cond_wait(&g_continue_input_cond, &g_continue_input_mutex);
        pthread_mutex_unlock(&g_continue_input_mutex);
    }
}

int input_destroy()
{
    if (g_fixed_input)
        pthread_mutex_init(&g_remain_count_mutex, NULL);

    pthread_mutex_destroy(&g_continue_input_mutex);
    pthread_cond_destroy(&g_continue_input_cond);
    return 0;
}

int input_handling()
{
    int input_count = 0, quit_flag = 0;

    while(1)
    {
        input_count = 0;

        int in;
        hftp_log_lock();
        do
        {
            in = hftp_std_input_c_not_safety();
            g_input_buffer[input_count++] = in;
        }
        while (in != '\n' && in != EOF);
        hftp_log_unlock();

        if (in == EOF)
            hftp_log_out("\n");

        if (in == '\n')
            g_input_buffer[input_count-1] = '\0';
        else if (in == EOF)
            g_input_buffer[input_count-2] = '\0';

        if (strcmp(g_input_buffer, "quit") == 0)
        {
            quit_flag = 1;
            break;
        }

        if (input_count - 1 <= 0)
            continue;
        else
        {
            interprete_multiple_commands(g_input_buffer);

            if (in == EOF || in == '\n' || in == '\0') 
                break;
        }
    }

    if (quit_flag)
    {
        g_continue_input_flag = 1;
        pthread_cond_signal(&g_continue_input_cond);
    }

    return 0;
}

/*
   1. parse string
   2. get host
   3. connect
   4. set connection buffer
 */
// semicolon replaced as semicolon
int interprete_single_command(char* str)
{
    struct conn_event cev;
    cev.sockfd = 0;
    cev.conn_index = -1;

    char domain[DOMAIN_MAX_LEN], remote_dir[REMOTE_DIR_MAX_LEN], local_dir[LOCAL_DIR_MAX_LEN];

    unsigned short port;
    int method;
    int fail = 0;

    int token_cnt = 0;
    char separators[] = " "; 
    char* token;

    token = strtok(str, separators);
    while (token)
    {
        switch (token_cnt)
        {
            case 0:
                if (name_to_method(token, &method))
                    fail = 1;   
                break;
            case 1:
                if (url_to_words(token, NULL, NULL, NULL, domain, &port, remote_dir, NULL, NULL, NULL))
                    fail = 1;
                break;
            case 2:
                strcpy(local_dir, token);
                break;
            default:
                break;
        }

        token_cnt++;

        token = strtok(NULL, separators);
    }

    if (token_cnt < 2 || token_cnt > 3 || fail)
    {
        hftp_log_err("usage : <method> <remote dir> [local dir], input : %s\n", str);
        return 1;
    }

    struct in_addr addr;
	if (host_to_addrv4(domain, &addr))
    {
        hftp_log_err("fail to convert address from remote domain(%s)\n", domain);
        return 1;
    }
   
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        hftp_log_err("fail to allocate socket\n");
        return 1;
    }

    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_addr = addr;
    in.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*)&in, sizeof(struct sockaddr_in)) < 0)
    {
        hftp_log_err("fail to connect..\n");
        close(sockfd);
        return 1;
    }

    for (int i = 0; i < EPOLL_INSTANCE_MAX_SOCKFD; i++)
    {
        if (g_conn_workspace[i].sockfd != 0) continue;

        int flag = 0;
        for (int j = 0; j < g_workspace_count; j++)
            if (i == g_workspace[j].conn_index)
            {
                flag = 1;
                break;
            }

        if (flag) continue;
        if (conn_queue_have_conn_index(i)) continue;

        {
            g_conn_workspace[i].state = 0;
            g_conn_workspace[i].method = method;
            g_conn_workspace[i].sockfd = sockfd;
 
            strcpy(g_conn_workspace[i].domain,      domain);
            strcpy(g_conn_workspace[i].remote_dir,  remote_dir);
            strcpy(g_conn_workspace[i].local_dir,   local_dir);

            cev.sockfd = sockfd;
            cev.conn_index = i;
            break;
        }
    }

    if (cev.conn_index < 0)
    {
        hftp_log_err("fail to find connection..\n");
        return 1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    if (epoll_ctl(g_epoll_inst_desc, EPOLL_CTL_ADD, sockfd, &ev))
    {
        hftp_log_err("fail to add netsocket to epoll instance\n");
        return 1;
    }
   
    pthread_mutex_lock(&g_remain_count_mutex);
    g_remain_count++;
    pthread_mutex_unlock(&g_remain_count_mutex);

    pthread_rwlock_wrlock(&g_thread_work_flag_rwlock);
    int idle_thread_index = get_idle_work_thread_index_not_safety();

    if (idle_thread_index >= 0)
    {
        g_workspace[idle_thread_index].http_process = http_request;
        g_workspace[idle_thread_index].conn_index = cev.conn_index;

        g_param_set_func_arr[method](
                &g_conn_workspace[cev.conn_index].clb, 
                &g_workspace[idle_thread_index].chunks
                );

        g_thread_work_flag = g_thread_work_flag | (0x01 << idle_thread_index);
        pthread_rwlock_unlock(&g_thread_work_flag_rwlock);
        pthread_cond_signal(&g_workspace[idle_thread_index].cond);
    }
    else
    {
        pthread_rwlock_unlock(&g_thread_work_flag_rwlock);
 
        if (enqueue_conn_event(&cev))
            hftp_log_err("fail to enqueue to connect event\n");  
    }
   
    return 0;
}

int is_empty_string(const char* str)
{
    int empty = 1;
    for (int i = 0; str[i]; i++)
        if (str[i] != ' ' && str[i] != '\n' && str[i] != '\r')
            empty = 0;
    return empty;
}

int interprete_multiple_commands(char* str_multi_commands)
{
    char* cursor = str_multi_commands,* semi,* lineend;

    do
    {
        semi = strchr(cursor, ';');
        lineend = strchr(cursor, '\n');

        if (semi && !lineend)
        {
            *semi = '\0';
        }
        else if (!semi && lineend)
        {
            *lineend = '\0';
        }
        else if (semi && lineend)
        {
            if (semi < lineend)
            {
                *semi = '\0';
                lineend = NULL;
            }
            else
            {
                semi = NULL;
                *lineend = '\0';
            }
        }

        if (!is_empty_string(cursor))
            interprete_single_command(cursor);

        if (semi)
            cursor = semi + 1;
        else if (lineend)
            cursor = lineend + 1;
        else
            cursor = NULL;
    }
    while (cursor);
    return 0;
}

/*
     epoll event patcher definition declaration
 */
int epoll_patcher_init()
{
    // single epoll-instance
    g_epoll_inst_desc = epoll_create(EPOLL_INSTANCE_MAX_SOCKFD);

    if (g_epoll_inst_desc < 0)
    {
        hftp_log_err("fail to create epoll instance\n");
        return 1;
    }

    if (input_init())
        return 1;

    if (pthread_create(&g_epoll_patcher_thread, NULL, epoll_patcher, NULL))
    {
        hftp_log_err("fail to create epoll patcher pthread..\n\n");
        return 1;
    }

    return 0;
}

int epoll_patcher_join()
{
    g_epoll_patcher_cancel = 1;
    pthread_join(g_epoll_patcher_thread, NULL);
    return 0;
}

int epoll_patcher_destroy()
{
    input_destroy();
    // epoll_dstroy not exit
    return 0;
}

void* epoll_patcher(void* param)
{
    struct epoll_event events[EPOLL_INSTANCE_MAX_SOCKFD];
    int event_cnt = 0;

    while(1)
    {
        if (g_epoll_patcher_cancel) break;

        event_cnt = epoll_wait(g_epoll_inst_desc, events, EPOLL_INSTANCE_MAX_SOCKFD, 5000);

        if (event_cnt == 0)
            continue;

        if (event_cnt < 0)
        {
            hftp_log_err("fail to epoll_wait..\n");
            return NULL;
        }

        int same_thread_socket;
        for (int ei = 0; ei < event_cnt; ei++)
        {
            if (events[ei].data.fd == 0)
            {
                input_handling();
                continue;
            }

            same_thread_socket = 0;
            for (int th = 0; th < DEFAULT_WORKER_COUNT; th++)
            {
                if (g_workspace[th].conn_index >= 0)
                if (events[ei].data.fd == g_conn_workspace[g_workspace[th].conn_index].sockfd)
                {
                    same_thread_socket = 1;
                    break;
                }
            }

            if (epoll_queue_have_sockfd(events[ei].data.fd))
                same_thread_socket = 1;

            if (same_thread_socket)
                continue;
       
            int idle_thread_index = -1;
            pthread_rwlock_rdlock(&g_thread_work_flag_rwlock);
            for (int i = 0; i < MIN(64, g_workspace_count); i++)
                if (!(g_thread_work_flag & (0x01 << i)))
                {
                    idle_thread_index = i;
                    break;
                }
            pthread_rwlock_unlock(&g_thread_work_flag_rwlock);

            if (idle_thread_index >= 0)
            {
                g_workspace[idle_thread_index].http_process = http_response;

                for (int i = 0; i < EPOLL_INSTANCE_MAX_SOCKFD; i++)
                {
                    if (events[ei].data.fd == g_conn_workspace[i].sockfd)
                    {
                        g_workspace[idle_thread_index].conn_index = i;
                        break;
                    }
                }
             
                pthread_rwlock_wrlock(&g_thread_work_flag_rwlock);
                g_thread_work_flag = g_thread_work_flag | (0x01 << idle_thread_index);
                pthread_rwlock_unlock(&g_thread_work_flag_rwlock);   

                pthread_cond_signal(&g_workspace[idle_thread_index].cond);
            } 
            else
            {
                if (enqueue_epoll_event(events + ei))
                {
                    fprintf(stdout, "fail to enqueue epoll_event, but continued..\n");
                }
            }
        }
    }

    return NULL;
}

int conn_queue_have_conn_index(int conn_index)
{
    int cnt = g_conn_event_queue_count;
    int start = g_conn_event_queue_start_index;

    for (int i = 0; i < g_conn_event_queue_count; i++)
        if (g_conn_event_queue[(start + i) % CONN_EVENT_QUEUE_MAX].conn_index == conn_index)
            return 1;

    return 0;
}

int enqueue_conn_event_raw(int sockfd, char* domain, char* remote_dir, char* local_dir)
{
    assert(sockfd > 2);
    assert(domain != NULL);
    assert(remote_dir != NULL);
    assert(local_dir != NULL);

    pthread_rwlock_wrlock(&g_conn_event_queue_rwlock);

    int index = (g_conn_event_queue_start_index + g_conn_event_queue_count) % CONN_EVENT_QUEUE_MAX;

    g_conn_event_queue[index].sockfd = sockfd;
    strcpy(g_conn_event_queue[index].domain, domain);
    strcpy(g_conn_event_queue[index].remote_dir, remote_dir);
    strcpy(g_conn_event_queue[index].local_dir, local_dir);
    g_conn_event_queue_count++;

    pthread_rwlock_unlock(&g_conn_event_queue_rwlock);

    return 0;
}

int enqueue_conn_event(struct conn_event* in_ev)
{
    assert(in_ev != NULL);

    pthread_rwlock_wrlock(&g_conn_event_queue_rwlock);
    g_conn_event_queue[(g_conn_event_queue_start_index + g_conn_event_queue_count) % CONN_EVENT_QUEUE_MAX] = *in_ev;
    g_conn_event_queue_count++;
    pthread_rwlock_unlock(&g_conn_event_queue_rwlock);

    return 0;
}

int enqueue_conn_event_not_safety(struct conn_event* in_ev)
{
    assert(in_ev != NULL);

    g_conn_event_queue[(g_conn_event_queue_start_index + g_conn_event_queue_count) % CONN_EVENT_QUEUE_MAX] = *in_ev;
    g_conn_event_queue_count++;

    return 0;
}

int dequeue_conn_event(struct conn_event* out_ev)
{
    assert(out_ev != NULL);

    pthread_rwlock_wrlock(&g_conn_event_queue_rwlock);
    *out_ev = g_conn_event_queue[g_conn_event_queue_start_index % CONN_EVENT_QUEUE_MAX];
    g_conn_event_queue_count--;
    g_conn_event_queue_start_index = (g_conn_event_queue_start_index + 1) % CONN_EVENT_QUEUE_MAX;
    pthread_rwlock_unlock(&g_conn_event_queue_rwlock);

    return 0;
}

int dequeue_conn_event_not_safety(struct conn_event* out_ev)
{
    assert(out_ev != NULL);

    *out_ev = g_conn_event_queue[g_conn_event_queue_start_index % CONN_EVENT_QUEUE_MAX];
    g_conn_event_queue_count--;
    g_conn_event_queue_start_index = (g_conn_event_queue_start_index + 1) % CONN_EVENT_QUEUE_MAX;

    return 0;
}

int epoll_queue_have_sockfd(int sockfd)
{
    int cnt = g_epoll_event_queue_count;
    int start = g_epoll_event_queue_start_index;

    for (int i = 0; i < cnt; i++)
        if (g_epoll_event_queue[(start + i) % EPOLL_EVENT_QUEUE_MAX].data.fd == sockfd)
            return 1;

    return 0;
}

int enqueue_epoll_event(struct epoll_event* in_ev)
{
    assert(in_ev != NULL);

    pthread_rwlock_wrlock(&g_epoll_event_queue_rwlock);
    g_epoll_event_queue[(g_epoll_event_queue_start_index + g_epoll_event_queue_count) % EPOLL_EVENT_QUEUE_MAX] = *in_ev;
    g_epoll_event_queue_count++;
    pthread_rwlock_unlock(&g_epoll_event_queue_rwlock);

    return 0;
}

int enqueue_epoll_event_not_safety(struct epoll_event* in_ev)
{
    assert(in_ev != NULL);

    g_epoll_event_queue[(g_epoll_event_queue_start_index + g_epoll_event_queue_count) % EPOLL_EVENT_QUEUE_MAX] = *in_ev;
    g_epoll_event_queue_count++;

    return 0;
}

int dequeue_epoll_event(struct epoll_event* out_ev)
{
    assert(out_ev != NULL);

    pthread_rwlock_wrlock(&g_epoll_event_queue_rwlock);
    *out_ev = g_epoll_event_queue[g_epoll_event_queue_start_index % EPOLL_EVENT_QUEUE_MAX];
    g_epoll_event_queue_count--;
    g_epoll_event_queue_start_index = (g_epoll_event_queue_start_index + 1) % EPOLL_EVENT_QUEUE_MAX;
    pthread_rwlock_unlock(&g_epoll_event_queue_rwlock);

    return 0;
}

int dequeue_epoll_event_not_safety(struct epoll_event* out_ev)
{
    assert(out_ev != NULL);

    *out_ev = g_epoll_event_queue[g_epoll_event_queue_start_index % EPOLL_EVENT_QUEUE_MAX];
    g_epoll_event_queue_count--;
    g_epoll_event_queue_start_index = (g_epoll_event_queue_start_index + 1) % EPOLL_EVENT_QUEUE_MAX;

    return 0;
}

int workspace_init(int size)
{
    g_workspace_count = size;

    g_workspace = (struct thread_workspace*)malloc(sizeof(struct thread_workspace)*g_workspace_count);
    if (g_workspace == NULL)
    {
        hftp_log_err("fail to allocate workspace\n");
        return 1;
    }
    memset(g_workspace, 0, sizeof(struct thread_workspace) * g_workspace_count); 

    pthread_rwlock_init(&g_thread_work_flag_rwlock, NULL);

    for (int i = 0; i < g_workspace_count; i++)
    {
        g_workspace[i].number = i;
        pthread_cond_init(&g_workspace[i].cond, NULL);
        pthread_mutex_init(&g_workspace[i].mutex, NULL);

        if (pthread_create(&g_workspace[i].tid, NULL, epoll_worker, &g_workspace[i].number))
        {
            hftp_log_err("fail to create thread..");
            return 1;
        }
    }

    return 0;
}

int workspace_join()
{
    for (int i = 0; i < g_workspace_count; i++)
        pthread_cancel(g_workspace[i].tid);

    struct timespec wait, err;
    wait.tv_nsec = 10000000; // 10ms

    // pthread_setcanceltype(~, PTHREAD_CANCEL_DEFERRED);
    while (g_thread_work_flag)
        nanosleep(&wait, &err);

    return 0;
}

int workspace_destroy()
{
    for (int i = 0; i < g_workspace_count; i++)
    {
        pthread_cond_destroy(&g_workspace[i].cond);
        pthread_mutex_destroy(&g_workspace[i].mutex);
    }  

    pthread_rwlock_destroy(&g_thread_work_flag_rwlock); 

    free(g_workspace); 

    return 0;
}

int is_idle_thread(int thread_index)
{
    int flag = 0;
    pthread_rwlock_rdlock(&g_thread_work_flag_rwlock);
    flag = g_thread_work_flag & (0x01 << thread_index);
    pthread_rwlock_unlock(&g_thread_work_flag_rwlock);

    return !flag;
}

void* epoll_worker(void* param)
{
    struct epoll_event eev;
    struct conn_event cev;
    int thread_index = *(int*)param, 
        auto_dequeue = 0;
    pthread_cond_t* cond = &g_workspace[thread_index].cond;
    pthread_mutex_t* mutex = &g_workspace[thread_index].mutex;
    struct thread_workspace* workspace = g_workspace + thread_index;
    workspace->conn_index = -1;

    while (1)
    {
        if (!auto_dequeue)
        while (is_idle_thread(thread_index))
        {
            pthread_mutex_lock(mutex);
            pthread_cond_wait(cond, mutex);
            pthread_mutex_unlock(mutex);
        }

        int (*http_process)(struct http_connection*, char*, int) = NULL;

        switch(auto_dequeue)
        {
            case 0:
                http_process = workspace->http_process;
                // already reserved
                break;
            case 1:
                http_process = http_request;
                workspace->conn_index = cev.conn_index;
                break;
            case 2:
                http_process = http_response;
                for (int i = 0; i < EPOLL_INSTANCE_MAX_SOCKFD; i++)
                {
                    if (g_conn_workspace[i].sockfd == eev.data.fd)
                    {
                        workspace->conn_index = i;
                        break;
                    }
                }
                break;
            default:
                hftp_log_err("dequeue flag corrupted..(%d)\n", auto_dequeue);
                break;
        }

        if (http_process)
            http_process(g_conn_workspace + workspace->conn_index, workspace->buffer, HTTP_CONN_DATA_BUFFER_SIZE);
     
        if (g_fixed_input) 
        if (g_conn_workspace[workspace->conn_index].state == -1 || g_conn_workspace[workspace->conn_index].sockfd  == 0)
        {

            pthread_mutex_lock(&g_remain_count_mutex);
            g_remain_count--;
            pthread_mutex_unlock(&g_remain_count_mutex);
            
            if (g_remain_count)
                g_continue_input_flag = 1;
            pthread_cond_signal(&g_continue_input_cond);
        }

        memset(&workspace->chunks, 0, sizeof(union method_chunks));
        auto_dequeue = 0;
        workspace->conn_index = -1;
        
        // conn event dequeue.
        if (!auto_dequeue)
        {
            pthread_rwlock_wrlock(&g_conn_event_queue_rwlock);
            if (g_conn_event_queue_count)
            {
                if (!dequeue_conn_event_not_safety(&cev))
                {
                    assert(cev.sockfd > 2);

                    struct http_connection* conn = g_conn_workspace + cev.conn_index;
                    workspace->conn_index = cev.conn_index;

                    conn->sockfd = cev.sockfd;
                    conn->state = 0;
                    g_param_set_func_arr[conn->method](&conn->clb, &workspace->chunks);

                    auto_dequeue = 1;
                }
            }
            pthread_rwlock_unlock(&g_conn_event_queue_rwlock);
        }

        // epoll event dequeue
        if (!auto_dequeue)
        {
            pthread_rwlock_wrlock(&g_epoll_event_queue_rwlock);
            if (g_epoll_event_queue_count)
            {
                if (!dequeue_epoll_event_not_safety(&eev))
                {
                    struct http_connection* conn = NULL;

                    for (int i = 0; i < EPOLL_INSTANCE_MAX_SOCKFD; i++)
                    {
                        if (eev.data.fd == g_conn_workspace[i].sockfd)
                        {
                            workspace->conn_index = i;
                            conn = g_conn_workspace + i;
                            break;
                        }
                    }
                    
                    if (conn != NULL)
                    {
                        conn->sockfd = eev.data.fd;
                        conn->state = 2;
                        auto_dequeue = 2;
                    }
                    else
                    {

                    }
                }
            }
            pthread_rwlock_unlock(&g_epoll_event_queue_rwlock);
        }

        if (!auto_dequeue)
        {
            pthread_rwlock_wrlock(&g_thread_work_flag_rwlock);
            g_thread_work_flag = g_thread_work_flag & ~(0x01 << thread_index);
            pthread_rwlock_unlock(&g_thread_work_flag_rwlock);       
        }
    }

    return NULL;
}

// thread alloc

int threads_init()
{
    if (epoll_patcher_init())
        return 1;

    // workspaces
    if (workspace_init(DEFAULT_WORKER_COUNT))
        return 1;
    
    return 0;
}

int threads_join()
{
    epoll_patcher_join();
    workspace_join();
    return 0;
}

int threads_destroy()
{
    epoll_patcher_destroy();
    workspace_destroy();
   
    return 0;
}


