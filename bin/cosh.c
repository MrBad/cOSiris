#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "syscalls.h"
//
//  Simple shell intended for my hobby osdev - cOSiris
//      supports multiple commands, pipes and redirections
//		TODO:	some builtins: bg, fg and group
//				some history, env and maybe variables
//
#define MAX_ARGV 16
#define MAX_TOKEN_SZ 512

typedef struct cmd {
	int argc;					// number of arguments
	char *argv[MAX_ARGV];		// arguments
	char *outfile, *infile;		// file names used in redirects
	int fdin, fdout;			// file descriptors
	int append;					// if fdout should be opened in append mode
	int do_pipe;				// if we have a pipe after command
	pid_t pid;					// pid of this cmd
	struct cmd *next;			// link to next struct
} cmd_t;

void print_prompt();
int parse(char *buf, cmd_t **head);
int read_line(char *buf, size_t size);
void print_commands(cmd_t *head);
int exec_commands(cmd_t *head);


void clean_cmd(cmd_t *cmd) 
{
	cmd_t *c;
	int i;
	while (cmd) {
	    if (cmd->fdin > -1)
	        close(cmd->fdin);
	    if (cmd->fdout > -1)
	        close(cmd->fdout);
		if (cmd->infile)
			free(cmd->infile);
		if (cmd->outfile)
			free(cmd->outfile);
		for (i = 0; i < cmd->argc; i++)
			free(cmd->argv[i]);
		c = cmd;
		cmd = cmd->next;
		free(c);
	}
}

void sig_clean(int signum)
{
    (void) signum;
    printf("^X\n");
    print_prompt();
}

// from where we read -> stdin for now //
int fd;

int main() 
{
	char line[512];
	fd = 0;
	cmd_t *head;

	signal(SIGINT, sig_clean);
	signal(SIGTERM, sig_clean);

	while (1) {
		head = NULL;
		print_prompt();
		if (read_line(line, sizeof(line)) <= 0)
		    break;
		if (parse(line, &head) < 0) {
			clean_cmd(head);
			continue;
		}
		exec_commands(head);
		clean_cmd(head);
	}
}

void print_prompt() 
{
	write(1, "# ", 2);
}

int read_line(char *buf, size_t size) 
{
	ssize_t n = 0;
	n = read(fd, buf, size);
	buf[n] = 0;
	return (int) n;
}

#define is_space(p) (p==' '||p=='\t'||p=='\r'||p=='\n')
#define is_symbol(p) (p=='<'||p=='>'||p=='|'||p==';'||p=='"')

int get_token(char **buf, char *tok, char *end) 
{
	char *p;
	p = *buf;

	while (*p && is_space(*p))
		p++;
	if (!*p) return -1;
	if (is_symbol(*p)) {
		if (*p == '>') {
			*tok++ = *p++;
			if (*p == '>')
				*tok++ = *p++;
		} else if (*p == '"') {
			p++;
			while (*p && *p != '"') {
				if (tok == end)
					return -1;
				*tok++ = *p++;
			}
			if (tok == end)
				return -1;
			p++;
		} else {
			*tok++ = *p++;
		}
		*tok = 0;
	}
	else {
		while (*p && !is_space(*p) && !is_symbol(*p)) {
			*tok++ = *p++;
			if (tok == end)
				return -1;
		}
		*tok = 0;
	}
	*buf = p;
	return *p ? 0 : -1;
}

cmd_t *cmd_new() 
{
	cmd_t *cmd;
	cmd = calloc(1, sizeof(cmd_t));
	cmd->fdin = cmd->fdout = -1;
	return cmd;
}

int parse(char *buf, cmd_t **h) 
{
	char token[MAX_TOKEN_SZ];
	char *end = token + MAX_TOKEN_SZ;
	char *ptr = buf;
	cmd_t *cmd, *head;
	head = cmd = *h;

	while (get_token(&ptr, token, token + MAX_TOKEN_SZ) == 0) {
		if (is_symbol(token[0])) {
			if (!cmd) {
				printf("invalid command\n");
				return -1;
			}
			if (token[0] == '>') {
				if (token[1] == '>')
					cmd->append = 1;
				if (get_token(&ptr, token, end) < 0) {
					printf("redir: no such file\n");
					return -1;
				}
				cmd->outfile = strdup(token);
				continue;
			}
			else if (token[0] == '<') {
				if (get_token(&ptr, token, end) < 0) {
					printf("redir <: no such file\n");
					return -1;
				}
				cmd->infile = strdup(token);
				continue;
			}
			else if (token[0] == '|') {
				char *sptr = ptr;
				if (get_token(&ptr, token, end) < 0) {
					printf("invalid pipe\n");
					return -1;
				}
				cmd->do_pipe = 1;
				ptr = sptr;
			}

			cmd->argv[cmd->argc] = 0;
			cmd_t *c;
			cmd = cmd_new();
			for (c = head; c->next; c = c->next);
			c->next = cmd;
		}
		else {
			if (!head) {
				head = cmd = *h = cmd_new();
			}
			if (cmd->argc + 1 >= MAX_ARGV) {
				printf("too many arguments\n");
				return -1;
			}
			cmd->argv[cmd->argc++] = strdup(token);
		}
	}
	return 0;
}

void print_commands(cmd_t *head) 
{
	cmd_t *cmd;
	
	for (cmd = head; cmd; cmd = cmd->next) {
		printf("cmd: %s, argc: %d\n", cmd->argv[0], cmd->argc);
		if (cmd->do_pipe) {
			printf("\tdo pipe ");
			if (!cmd->next) {
				printf("no next command\n");
				return;
			}
			printf(" | %s\n", cmd->next->argv[0]);
		} else {
			if (cmd->outfile) {
				printf("\twrite to %s\n", cmd->outfile);
			}
		}
		if (cmd->infile) {
			printf("\tread from %s\n", cmd->infile);
		}

	}
}

int cd(char *dir) {
	int ret;
	if(dir)
		ret = chdir(dir);
	else 
		ret = chdir("/");
	if(ret < 0) {
		printf("cd: cannot change to %s\n", dir);
	}
	return ret;
}

int exec_commands(cmd_t *head) 
{
	cmd_t *cmd;
	int fd[2];
	pid_t pid, fg_pid;
	int status;

	for (cmd = head; cmd; cmd = cmd->next) {
		if(strcmp(cmd->argv[0], "cd") == 0) {
			cd(cmd->argv[1]);
			continue;
		}
		// BUG - somehow i do not close a fd[0] when it should be
		// and gets replicated on fork... grrr....
		if (cmd->do_pipe) {
			pipe(fd);
			cmd->fdout = fd[1];
		} else {
			if (cmd->outfile) {
				int flags = O_WRONLY;
				flags |= cmd->append ? O_APPEND : O_CREAT | O_TRUNC;
				cmd->fdout = open(cmd->outfile, flags, 0666);
			}
		}
		if (cmd->infile) {
			cmd->fdin = open(cmd->infile, O_RDONLY, 0666);
		}

		pid = fork();
		if (pid == 0) {
			if(cmd->do_pipe) {
				close(fd[0]);
			}
			if (cmd->fdout > 0) {
				close(1);
				if (dup(cmd->fdout) != 1) {
					printf("error in dup() stdout\n");
				}
			} else {
			    fg_pid = getpid();
			    tcsetpgrp(1, fg_pid);
			} 
			if (cmd->fdin > 0) {
				close(0);
				if (dup(cmd->fdin) != 0)
					printf("error in dup() stdin\n");
			}
			execvp(cmd->argv[0], cmd->argv);
			printf("error executing: %s\n", cmd->argv[0]);
			exit(1);
		} else {
		    if(cmd->do_pipe) {
			    cmd->next->fdin = fd[0];
			    close(fd[1]);
		    }
		    cmd->pid = pid;
		}
	}

	pid_t p;
	int err = 0;
	while((p = wait(&status)) > 0) {
		cmd_t *cm;
		// kernel is closing this //
		for(cm = head; cm; cm = cm->next) {
			if(p == cm->pid) {
/*				if(cmd->fdin != -1)
					close(cmd->fdin);
				if(cmd->fdout != -1)
					close(cmd->fdout);
*/				if(status != 0) {
					printf("%s exited with status %d\n", cm->argv[0], status);
					err++;
				}
			}
		}
	}
	fg_pid = getpid();
	tcsetpgrp(1, fg_pid);
	return err;
}
