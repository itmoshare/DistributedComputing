#define _GNU_SOURCE
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "common.h"
#include "ipc.h"
#include "pa2345.h"

#include "extra.h"

#include "queue.h"

static FILE *events_log_f = NULL;
timestamp_t lamport_time = 0;
queue_t *g_queue = NULL;

timestamp_t get_lamport_time()
{
	return lamport_time;
}

void update_time(timestamp_t new_time)
{
	lamport_time = new_time > lamport_time 
	? new_time 
	: lamport_time;
}

void inc_time()
{
	lamport_time++;
}

void new_message(Message *msg, MessageType type)
{
	memset(msg, 0, sizeof(Message));
	msg->s_header.s_magic = MESSAGE_MAGIC;
	msg->s_header.s_type = type;
	msg->s_header.s_local_time = get_lamport_time();
}

int log_event(char *msg)
{
	fprintf(events_log_f, "%s", msg);
	return 0;
}

int request_cs(const void * self) 
{
	ProcInfo *proc_info = (ProcInfo*)self;
	Message msg;
	inc_time();
	new_message(&msg, CS_REQUEST);
	if (g_queue == NULL)
	{
		q_init(&g_queue);
	}

	enq(g_queue, proc_info->local_pid, get_lamport_time());
	int rc = send_multicast(proc_info, &msg);
	if (rc != 0) return rc;

	int replies_ct = proc_info->proc_ct - 2;
	while (replies_ct > 0 || g_queue->head->pid != proc_info->local_pid)
	{
		rc = receive_any(proc_info, &msg);
		if (rc != 0) return rc;
		update_time(msg.s_header.s_local_time);
		inc_time();
		switch(msg.s_header.s_type)
		{
			case CS_REQUEST:
				enq(g_queue, proc_info->prev, msg.s_header.s_local_time);
				inc_time();
				new_message(&msg, CS_REPLY);
				int rc = send(proc_info, proc_info->prev, &msg);
				if (rc != 0) return rc;
				break;
			case CS_REPLY:
				replies_ct--;
				break;
			case CS_RELEASE:
				deq(g_queue);
				break;
		}
	}

	return 0;
}

int release_cs(const void * self) 
{
	ProcInfo *proc_info = (ProcInfo*)self;

	deq(g_queue);
	Message msg;
	inc_time();
	new_message(&msg, CS_RELEASE);

	int rc = send_multicast(proc_info, &msg);
	if (rc != 0) return rc;

	return 0;
}

// Close unused pipes
void close_pipes(ProcInfo *proc_info)
{
	for (int i = 0; i < proc_info->proc_ct; i++)
	{
		for (int j = 0; j < proc_info->proc_ct; j++)
		{
			if (i == j) continue;
			if (i != proc_info->local_pid)
			{
				close(proc_info->pipes[i][j].writeEnd);
			}
			if (j != proc_info->local_pid)
			{
				close(proc_info->pipes[i][j].readEnd);
			}
		}
	}
}

int receive_all(ProcInfo *proc_info)
{
	Message msg;
	new_message(&msg, 0);
	for(local_id i = 1; i < proc_info->proc_ct; i++)
	{
		if (proc_info->local_pid == i)
			continue;
		if (receive(proc_info, i, &msg) < 0) return -1;
		update_time(msg.s_header.s_local_time);
		inc_time();
	}
	return 0;
}

int parent_action(ProcInfo *proc_info)
{
	close_pipes(proc_info);
	
	// Get started messages
	if (receive_all(proc_info) < 0) return -1;

	// Get done messages
	if (receive_all(proc_info) < 0) return -1;

	for(local_id pid = 1; pid < proc_info->proc_ct; pid++)
	{
		wait(NULL);
	}
	return 0;
}

int child_body(ProcInfo *proc_info) 
{
	if (proc_info->mutexl)
		request_cs(proc_info);

	char buff[1024];

	int n = proc_info->proc_ct * 5;
	for (int i = 1; i <= n; i++) 
	{
		snprintf(buff, sizeof(buff), log_loop_operation_fmt, proc_info->proc_ct, i, n);
		print(buff);
	}

	if (proc_info->mutexl)
		release_cs(proc_info);
	return 0;
}

int child_action(ProcInfo *proc_info, int sys_pid, int parentPid)
{
	close_pipes(proc_info);
	char log_buff[MAX_PAYLOAD_LEN];

	inc_time();	
	Message msg;
	new_message(&msg, STARTED);
	snprintf(msg.s_payload, MAX_PAYLOAD_LEN, log_started_fmt, get_lamport_time(),
			 proc_info->local_pid, sys_pid, parentPid, 
			 0);
	msg.s_header.s_payload_len = strlen(msg.s_payload);

	if (log_event(msg.s_payload) < 0) return -1;
	
	if (send_multicast(proc_info, &msg) < 0) return -1;
	
	if (receive_all(proc_info) < 0) return -1;

	snprintf(log_buff, MAX_PAYLOAD_LEN, log_received_all_started_fmt, get_lamport_time(),
			 proc_info->local_pid);

	if (log_event(log_buff) < 0) return -1;

	child_body(proc_info);


	snprintf(msg.s_payload, MAX_PAYLOAD_LEN,
			 log_done_fmt, get_lamport_time(), 
			 proc_info->local_pid,
			 0);
	inc_time();
	msg.s_header.s_local_time = get_lamport_time();
	msg.s_header.s_payload_len = strlen(msg.s_payload);
	msg.s_header.s_type = DONE;

	if (log_event(msg.s_payload) < 0) return -1;

	if (send_multicast(proc_info, &msg) < 0) return -1;

	if (receive_all(proc_info) < 0) return -1;

	snprintf(log_buff, MAX_PAYLOAD_LEN, log_received_all_done_fmt, get_lamport_time(),
			 proc_info->local_pid);
	if (log_event(log_buff) < 0) return -1;

	return 0;
}

int main(int argc, char * const argv[])
{
	if (argc < 2) 
	{
		fprintf(stderr, "Not enough arguments\n");
        return 1;
    }
	
	int mutexl = 0;

	struct option mutex_opt[] = 
	{
		{"mutexl", no_argument, &mutexl, 1},
		{0, 0, 0, 0}
	};

	int n = 0;
	int opt = 0;
	while ((opt = getopt_long(argc, argv, "p:", mutex_opt, NULL)) != -1) 
	{
		switch(opt) 
		{
			case 0: break;
			case 'p':
				n = atoi(optarg);
				if (!n || n <= 0 || n > 10) 
				{
					fprintf(stderr, "p parameter must be int from 1 to 10\n");
					return 1;
				}
				break;
			case -1:
				return 1;
		}
	}
	n++;

	ProcInfo proc_info;
	memset(&proc_info, 0, sizeof(proc_info));
	proc_info.proc_ct = n;
	proc_info.mutexl = mutexl;
	
	proc_info.pipes = malloc(n * sizeof(proc_info.pipes));
	for (int i = 0; i < n; i++)
		proc_info.pipes[i] = malloc(n * sizeof(Pipe));
	if (proc_info.pipes == NULL) return 1;
	
	// Open logs
	FILE * pipes_log_f = fopen(pipes_log, "a");
	if (pipes_log_f == NULL) return 1;
	setbuf(pipes_log_f, NULL);
	
	events_log_f = fopen(events_log, "a");
	if (events_log_f == NULL) return 1;
	setbuf(events_log_f, NULL);
	
	// Open pipes
	for (int i = 0; i < n; i++) 
	{
		for (int j = 0; j < n; j++) 
		{
			if (i == j) continue;
			
			int rc = pipe2((int*)&proc_info.pipes[i][j], O_NONBLOCK);
			
			if (rc < 0) return 1;
			fprintf(pipes_log_f, "open pipe(%d, %d)\n", i, j);
		}
	}
	
	// Fork
	pid_t parentPid = getpid();
	pid_t cur_sys_pid = 0;
	
	for (local_id pid = 1; pid < n; pid++)
	{
		int sys_pid = fork();
		if (sys_pid < 0) return 1;
		if (sys_pid == 0)
		{
			cur_sys_pid = getpid();
			proc_info.local_pid = pid;
			break;
		}
	}
	
	// Process action
	if (!proc_info.local_pid)
	{
		parent_action(&proc_info);
	} 
	else 
	{
		child_action(&proc_info, cur_sys_pid, parentPid);
	}
	
	// Free
	for (int i = 0; i < n; i++)
		free(proc_info.pipes[i]);
	free(proc_info.pipes);
	
	fclose(pipes_log_f);
	fclose(events_log_f);

	return 0;
}

