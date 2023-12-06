#define TCP_BUFFER_SIZE 1024
// === ERROR CODES === //
#define OK 0x00
#define NOSUCHFILE 0x1
#define NOACCESS 0x02
#define UNDEFINED 0x03

/* Response struct - server's response to the client */
struct response {
	char status_code; /* Status code */
	char data[TCP_BUFFER_SIZE]; /* Data to be sent - requested file or error description */
};
