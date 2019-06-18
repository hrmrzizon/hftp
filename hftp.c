#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#include "hftp.h"

int g_debug_log = 0;
int g_prefix_log_state = 0;

int prefix_prelog(FILE* stream)
{
    if (!g_prefix_log_state)
    {
        fprintf(stream, "hftp >> ");
    //    g_prefix_log_state = 1;
    }

    return 0;
}

int prefix_postlog(FILE* stream)
{
    if (g_prefix_log_state == 2)
        fprintf(stream, "hftp >> ");
    else if(g_prefix_log_state == 1)
        g_prefix_log_state = 2;
    

    return 0;
}

pthread_mutex_t         g_std_inouterr_mutex;

int hftp_log_lock()
{
    return pthread_mutex_lock(&g_std_inouterr_mutex);
}

int hftp_log_unlock()
{
    return pthread_mutex_unlock(&g_std_inouterr_mutex);
}
int hftp_log_init()
{
    pthread_mutex_init(&g_std_inouterr_mutex, NULL);
    return 0;
}

int hftp_log_destroy()
{
    pthread_mutex_destroy(&g_std_inouterr_mutex);
    return 0;
}

int hftp_log_num(int mode)
{
    switch (mode)
    {
        case 1:
            fputs("\rhftp >> ", stdout);
            break;
        case 2:
            fputs("\rhftp.log >> ", stdout);
            break;
        case 3:
            fputs("\rhftp.err >> ", stderr);
            break;
    }
    return 0;
}

int hftp_log_out(const char* fmt, ...)
{
    pthread_mutex_lock(&g_std_inouterr_mutex);

    prefix_prelog(stdout);
//    fputc('\r', stdout);

    int cnt;
    va_list args;

    va_start(args, fmt);
    cnt = vfprintf(stdout, fmt, args);
    va_end(args);
/*
    pthread_rwlock_rdlock(&g_input_mode_rwlock);
    hftp_log_num(g_input_mode);
    pthread_rwlock_unlock(&g_input_mode_rwlock);
*/
    prefix_postlog(stdout);
    pthread_mutex_unlock(&g_std_inouterr_mutex);    

    return cnt;
}

int hftp_log_err(const char* fmt, ...)
{
    pthread_mutex_lock(&g_std_inouterr_mutex);

    prefix_prelog(stdout);

    int cnt;
    va_list args;
    
    va_start(args, fmt);
    cnt = vfprintf(stdout, fmt, args);
    va_end(args);
/*
    pthread_rwlock_rdlock(&g_input_mode_rwlock);
    hftp_log_num(g_input_mode);
    pthread_rwlock_unlock(&g_input_mode_rwlock);
*/
    prefix_postlog(stdout);

    pthread_mutex_unlock(&g_std_inouterr_mutex);

    return cnt;
}

int hftp_std_input_c()
{
    int c;
    pthread_mutex_lock(&g_std_inouterr_mutex);
    c = fgetc(stdin);
    pthread_mutex_unlock(&g_std_inouterr_mutex);
    return c;
}    

int hftp_std_input_c_not_safety()
{
    return fgetc(stdin);
}    
int hftp_log_out_with_mode(int mode, const char* fmt, ...)
{
    pthread_mutex_lock(&g_std_inouterr_mutex);

    prefix_prelog(stdout);

    int cnt;
    va_list args;

    va_start(args, fmt);
    cnt = vfprintf(stdout, fmt, args);
    va_end(args);

    prefix_postlog(stdout);

    pthread_mutex_unlock(&g_std_inouterr_mutex);    

    return cnt;
}

