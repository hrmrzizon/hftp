#define HFTP_APP_NAME       "hftp"
#define HFTP_APP_VERSION    "0.1a"

extern int g_debug_log;

int hftp_log_init();
int hftp_log_destroy();

int hftp_log_out(const char* fmt, ...);
int hftp_log_err(const char* fmt, ...);

int hftp_log_out_with_mode(int mode, const char* fmt, ...);

void set_input_mode(int set);

int hftp_std_input_c();
int hftp_std_input_c_not_safety();

int hftp_log_lock();
int hftp_log_unlock();

