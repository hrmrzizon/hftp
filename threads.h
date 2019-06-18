#pragma once

/*
   input_handling
 */

int input_init();
int input_destroy();
int input_cond_blocking();

int input_handling();

int interprete_single_command(char* str);
int interprete_multiple_commands(char* str_multi_commands);

/*
   epoll event patcher
 */

int enqueue_epoll_event(struct epoll_event* in_ev);
int dequeue_epoll_event(struct epoll_event* out_ev);

int epoll_patcher_init();
int epoll_patcher_join();
int epoll_patcher_destroy();

void* epoll_patcher(void* param);

/*
   epoll io worker
 */

struct conn_event
{
    int                                 sockfd;
    int                                 conn_index;

    char                                domain[DOMAIN_MAX_LEN],
                                        remote_dir[REMOTE_DIR_MAX_LEN],
                                        local_dir[LOCAL_DIR_MAX_LEN];
};

int conn_queue_have_conn_index(int conn_index);

int enqueue_conn_event(struct conn_event* in_ev);
int enqueue_conn_event_raw(int sockfd, char* domain, char* remote_dir, char* local_dir);
int dequeue_conn_event(struct conn_event* out_ev);

struct http_connection;
union method_chunks;

struct thread_workspace
{
    int                                 number;
    pthread_t                           tid;
    pthread_cond_t                      cond;
    pthread_mutex_t                     mutex;
    
    int                             (*  http_process)(struct http_connection*, char*, int);
    int                                 conn_index;

    union method_chunks                 chunks;

    char                                buffer[HTTP_CONN_DATA_BUFFER_SIZE]; 
};

int epoll_queue_have_sockfd(int sockfd);
int enqueue_epoll_event(struct epoll_event* in_ev);
int dequeue_epoll_event(struct epoll_event* out_ev);

int workspace_init(int size);
int workspace_join();
int workspace_destroy();

void* epoll_worker(void* param);

/*
   thread check
 */
int threads_init();
int threads_join();
int threads_destroy();

