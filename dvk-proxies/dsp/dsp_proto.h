
void main_pws(void *arg_port);
void wdmp_px_stats(proxy_t *px_ptr);
int timespec2str(char *buf, uint len, struct timespec *ts);
int enable_keepalive(int sock);
void sig_alrm(int sig);
int dsp_put2lcl(proxy_hdr_t *hdr_ptr, proxy_payload_t *pl_ptr);
void update_stats(proxy_t *px_ptr, proxy_hdr_t *hd_ptr, proxy_payload_t *pl_ptr, int px_conn_type);
void *pr_thread(void *arg);
void *ps_thread(void *arg); 

