/* Global variables. */

/* The parameters of the call are kept here. */
message *m_ptr;		/* the input message itself */
message msg;		/* the output message used for is_reply */

pthread_mutex_t wis_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wis_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t www_cond = PTHREAD_COND_INITIALIZER;

static char wis_buffer[WISBUFSIZE+1]; /* static so zero filled */
static char wis_html[WISBUFSIZE+1]; /* static so zero filled */

int dump_type;
