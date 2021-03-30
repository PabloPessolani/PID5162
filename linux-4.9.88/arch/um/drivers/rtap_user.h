
struct request_v2 {
	uint32_t magic;
	uint32_t version;
	enum request_type type;
	struct sockaddr_un sock;
};


struct request_v3 {
	uint32_t magic;
	uint32_t version;
	enum request_type type;
	struct sockaddr_un sock;
};

#define RQ3_FORMAT "magic=%d version=%d type=%d\n"
#define RQ3_FIELDS(p) p->magic, p->version, p->type

union request {
	struct request_v2 v2;
	struct request_v3 v3;
};

