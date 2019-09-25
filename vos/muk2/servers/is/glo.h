/* Global variables. */

/* The parameters of the call are kept here. */
message *m_ptr;		/* the input message itself */
message msg;		/* the output message used for is_reply */

int wis_mutex = 1;
int wis_cond = 0;
int www_cond = 0;

static char wis_buffer[WISBUFSIZE+1]; /* static so zero filled */
static char wis_html[WISBUFSIZE+1]; /* static so zero filled */

int dump_type;
