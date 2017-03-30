#include "ipc.h"

int send(void * self, local_id dst, const Message * msg) {
	return 0;
}

int send_multicast(void * self, const Message * msg) {
	return 0;
}

int receive(void * self, local_id from, Message * msg) {
	return 0;
}

int receive_any(void * self, Message * msg) {
	return 0;
}

