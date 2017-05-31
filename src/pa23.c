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

#include "banking.h"
#include "extra.h"

static FILE *events_log_f = NULL;
timestamp_t lamport_time = 0;

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

void push_balance(BalanceHistory *history, TransferOrder *order, timestamp_t sent_time)
{
	timestamp_t current_time = get_lamport_time();
	balance_t prev;
	prev = history->s_history_len == 0
			? 0 : history->s_history[history->s_history_len - 1].s_balance;

	timestamp_t new_len = current_time + 1;

	if (new_len > 1) 
	{
		for (timestamp_t t = history->s_history_len; t < new_len; t++)
		{
			history->s_history[t] = history->s_history[t - 1];
			history->s_history[t].s_time++;
		}
	}

	for (timestamp_t t = sent_time; t < current_time; t++) 
	{
		history->s_history[t].s_balance_pending_in = order->s_amount;
	}

	history->s_history[current_time].s_time = current_time;
	if (order->s_src == history->s_id)
	{
		history->s_history[current_time].s_balance = prev - order->s_amount;
	} 
	else
	{
		history->s_history[current_time].s_balance = prev + order->s_amount;
	}
	history->s_history_len = current_time + 1;
}

int parent_action(ProcInfo *proc_info)
{
	close_pipes(proc_info);
	
	// Get started messages
	if (receive_all(proc_info) < 0) return -1;

	// Robbery
	bank_robbery(proc_info, proc_info->proc_ct - 1);
	inc_time();
	Message stop;
	new_message(&stop, STOP);
    int rc = send_multicast(proc_info, &stop);
    if( rc != 0 ) return rc;

	// Get done messages
	if (receive_all(proc_info) < 0) return -1;

	// Get history
	AllHistory all;
	all.s_history_len = proc_info->proc_ct - 1;
	Message msg;
	new_message(&msg, 0);
	for(local_id i = 1; i < proc_info->proc_ct; i++)
	{
		int rc = receive(proc_info, i, &msg);
		update_time(msg.s_header.s_local_time);
		inc_time();
		if( rc != 0 ) return 1;
		memcpy(&all.s_history[i - 1], msg.s_payload, msg.s_header.s_payload_len);
	}

	print_history(&all);

	for(local_id pid = 1; pid < proc_info->proc_ct; pid++)
	{
		wait(NULL);
	}
	return 0;
}

int child_body(ProcInfo *proc_info) 
{
	BalanceHistory *history = proc_info->history;
	char buff[MAX_PAYLOAD_LEN];

	Message msg;
	new_message(&msg, 0);
	TransferOrder order;
	memset(&order, 0, sizeof order);

	while (1)
	{
		int rc = receive_any(proc_info, &msg);
		if (rc != 0) return 1;
		update_time(msg.s_header.s_local_time);
		inc_time();

		switch (msg.s_header.s_type)
		{
			case TRANSFER:
				memcpy(&order, msg.s_payload, msg.s_header.s_payload_len);
				inc_time();
				if (order.s_src == proc_info->local_pid) 
				{
					msg.s_header.s_local_time = get_lamport_time();
					snprintf(buff, MAX_PAYLOAD_LEN, log_transfer_out_fmt,
							 get_lamport_time(), proc_info->local_pid,
							 order.s_amount, order.s_dst);
					log_event(buff);

					push_balance(history, &order, msg.s_header.s_local_time);

					rc = send(proc_info, order.s_dst, &msg);
				}
				else
				{
					push_balance(history, &order, msg.s_header.s_local_time);
					new_message(&msg, ACK);
					snprintf(buff, MAX_PAYLOAD_LEN, log_transfer_in_fmt,
							 get_lamport_time(), proc_info->local_pid,
							 order.s_amount, order.s_src);
					log_event(buff);

					rc = send(proc_info, 0, &msg);
				}
				if (rc != 0) return 1;
			break;

			case STOP:
				order.s_src = 0;
				order.s_dst = proc_info->local_pid;
				order.s_amount = 0;
				push_balance(history, &order, get_lamport_time());
				return 0;
			break;
		}
	}

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
			 proc_info->history->s_history[get_lamport_time()].s_balance);
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
			 proc_info->history->s_history[get_lamport_time()].s_balance);
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

	// Send history
	BalanceHistory *history = proc_info->history;
	inc_time();
	TransferOrder order;
	order.s_src = 0;
	order.s_dst = proc_info->local_pid;
	order.s_amount = 0;
	push_balance(history, &order, get_lamport_time());

	new_message(&msg, BALANCE_HISTORY);
	msg.s_header.s_payload_len =
		sizeof *history - (MAX_T + 1 - history->s_history_len) * sizeof *history->s_history;

	memcpy(msg.s_payload, history, msg.s_header.s_payload_len);
	int rc = send(proc_info, 0, &msg);
	return rc;
}

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount) 
{
	inc_time();
    Message msg;
    new_message(&msg, TRANSFER);

    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;

    msg.s_header.s_payload_len = sizeof order;
    memcpy(msg.s_payload, &order, msg.s_header.s_payload_len);

    int rc = send(parent_data, src, &msg);
    if( rc != 0 ) exit(1);

    rc = receive(parent_data, dst, &msg);
    if( rc != 0 ) exit(1);
}

int main(int argc, char * const argv[])
{
	if (argc < 2) 
	{
		fprintf(stderr, "Not enough arguments\n");
        return 1;
    }
	
	int n = 0;
	switch (getopt(argc, argv, "p:")) 
	{
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
	n++;
	if (argc + 1 < n + optind) return 1;

	BalanceHistory *balances = malloc(n * sizeof(BalanceHistory));
	if (balances == NULL) return 2;
	memset(balances, 0, sizeof(BalanceHistory));

	int last_arg = optind - 1;
	for (int i = 1; i < n; i++)
	{
		balances[i] = (BalanceHistory) { i, 1, {{atoi(argv[last_arg + i]), 0, 0}}};
	}

	ProcInfo proc_info;
	memset(&proc_info, 0, sizeof(proc_info));
	proc_info.proc_ct = n;
	
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
			proc_info.history = &balances[pid];
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

	free(balances);
	return 0;
}

