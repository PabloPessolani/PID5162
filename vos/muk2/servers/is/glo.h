/* Global variables. */

/* The parameters of the call are kept here. */
message *m_ptr;		/* the input message itself */
message msg;		/* the output message used for is_reply */

int wis_mutex = 1;
Rendez wis_cond;
Rendez www_cond;

static char wis_buffer[WISBUFSIZE+1]; /* static so zero filled */
static char wis_html[WISBUFSIZE+1]; /* static so zero filled */
static char wis_cmd[_POSIX_ARG_MAX];

int dump_type;
