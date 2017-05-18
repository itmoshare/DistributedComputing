#define _DEFAULT_SOURCE
#include <unistd.h>
#include <string.h>

#include "ipc.h"
#include "extra.h"

#define TIMEOUT 20000

int send(void * self, local_id dst, const Message * msg) 
{
	ProcInfo *proc_info = (ProcInfo*)self;
	Pipe p = proc_info->pipes[proc_info->local_pid][dst];
	size_t msg_size = sizeof(msg->s_header) + msg->s_header.s_payload_len;
	if (write(p.writeEnd, msg, msg_size) < 0) return -1;
	return 0;
}

int send_multicast(void * self, const Message * msg) 
{
	ProcInfo *proc_info = (ProcInfo*)self;
	for(local_id pid = 0; pid < proc_info->proc_ct; pid++)
	{
		if (pid == proc_info->local_pid)
			continue;
		if(send(self, pid, msg) < 0) return -1;
	}
	return 0;
}

int receive(void * self, local_id from, Message * msg) 
{
	ProcInfo *proc_info = (ProcInfo*)self;
	Pipe p = proc_info->pipes[from][proc_info->local_pid];
	char buff[MAX_MESSAGE_LEN];
	while (1)
	{
		int rc = read(p.readEnd, buff, sizeof buff);
		if (rc > 0)
		{
			memcpy(msg, buff, rc);
			return 0;
		}
		usleep(100000);
	}
}

int receive_any(void * self, Message * msg) 
{
	ProcInfo *proc_info = (ProcInfo*)self;
	char buff[MAX_MESSAGE_LEN];
	while (1)
	{
		for(local_id pid = 0; pid < proc_info->proc_ct; pid++)
		{
			if (pid == proc_info->local_pid) continue;

			Pipe p = proc_info->pipes[pid][proc_info->local_pid];
			if (p.readEnd == 0) return -1;
			int rc = read(p.readEnd, buff, sizeof(buff));
			if (rc > 0) 
			{
				memcpy(msg, buff, rc);
				return 0;
			}
		}
		usleep(TIMEOUT);
	}
	return 0;
}

