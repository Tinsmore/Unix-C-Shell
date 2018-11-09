#include <stdio.h> 
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <sys/types.h>
#include <string.h>

#define BUFSIZE 64
#define TOKEN " \t\r\n\a"
#define FLAGS O_WRONLY | O_CREAT | O_TRUNC
#define MODE S_IRWXU | S_IXGRP | S_IROTH | S_IXOTH


char *setup_buffer(void)
{
	int bufsize = BUFSIZE;
	int position = 0;
	int c;
	char *buffer = malloc(sizeof(char) * bufsize);

  	while (1) 
    {
    	c = getchar();

    	if (c == EOF) 
		{
			printf("EOF sign!\n");
      		exit(EXIT_SUCCESS);
    	} 
		else if (c == '\n') 
		{
      		buffer[position] = '\0';
      		break;
    	} 
		else 
		{
      		buffer[position] = c;
    	}

    	position++;
  	}
  	
  	return buffer;
}


char **setup(char *buffer)
{
	int bufsize = BUFSIZE;
	int position = 0;
	char **tokens = malloc(bufsize * sizeof(char*));
	char *token;

	token = strtok(buffer, TOKEN);
	while (token != NULL) 
	{
    	tokens[position] = token;
    	position++;
    	token = strtok(NULL, TOKEN);
  	}
  	tokens[position] = NULL;
    
  	return tokens;
}


int len(char **args)
{
	int i;
	for(i = 0;i < BUFSIZE;i++)
		if(args[i] == NULL) return i; 
}


int isConcurrent(char **args)
{	
	if(args[0] == NULL) return 0;
	if(strcmp(args[len(args)-1],"&") == 0)
		return 1;
	else 
		return 0;
}


int redirect(char **args)
{
	if(args[0] == NULL) return 0;
	
	int l = len(args);
	if(l<3) return 0;
	if(strcmp(args[l-2],">") == 0)
		return 1;
	else if (strcmp(args[l-2],"<") == 0)
		return -1;
	else 
		return 0;
}


void printCommand(char **args)
{
	printf("->");
	for(int i=0;i<len(args);i++)
		printf("%s ",args[i]);
	printf("\n");
}


int isPipe(char **args)
{
	int i;
	
	for(i=0;i<len(args);i++)
	{
		if(strcmp(args[i],"|") == 0) return i;
	}
	
	return -1;
}

void mainloop(void) 
{ 
	char **args = malloc(BUFSIZE * sizeof(char*)), **args_child = malloc(BUFSIZE * sizeof(char*)), **args_in = malloc(BUFSIZE * sizeof(char));
	char *buffer, buffer_in[255], *last_buffer = malloc(BUFSIZE * sizeof(char)), *tmp = malloc(BUFSIZE * sizeof(char));
	int sign = 0, should_run = 1; 
	int concurrent = 0, redire = 0, pipe_sign = -1;
	int fd, sout = dup(fileno(stdout)); 
	FILE *fp = NULL;

	while (should_run) 
	{ 
		printf("osh>"); 
		
		// setup
		buffer = setup_buffer();
		strcpy(tmp,buffer);
		args = setup(buffer);
		
		if (args[0] == NULL) break;
		
		// history feature
		if (strcmp(args[0],"!!") == 0)
		{
			if (sign == 0)
			{
				printf("No commands in history.\n");
				continue;
			}
			else
			{
				strcpy(buffer,last_buffer);
				args = setup(buffer);
				printCommand(args);
			}
		}
		else
		{
			strcpy(last_buffer,tmp);
			sign = 1;
		}
			
		// concurrent option
		concurrent = isConcurrent(args);
		if(concurrent == 1) args[len(args)-1] = NULL;
		
		// redirect option
		redire = redirect(args);
		if(redire != 0)
		{
			if(redire == 1)
			{
				fd = open(args[len(args)-1], FLAGS, MODE);
				if(fd == -1){printf("open file error!\n");continue;}
				dup2(fd, fileno(stdout));
				args[len(args)-2] = NULL;
			}
			else
			{
				fp = fopen(args[len(args)-1], "r");
				if(fp == NULL){printf("open file error!\n");continue;}
				fgets(buffer_in, 255, (FILE*)fp);
				args_in = setup(buffer_in);
				for(int i=0;i<len(args_in);i++)
					args[len(args)-2+i] = args_in[i];
				args[len(args)+len(args_in)-2] = NULL;
			}
		}
		
		signal(SIGCHLD, SIG_IGN);
		// pipe option
		pipe_sign = isPipe(args);
		if(pipe_sign > -1)
		{
			int fd_pipe[2];
			
			for(int i=pipe_sign+1;i<len(args);i++)
				args_child[i-pipe_sign-1] = args[i];
			args[pipe_sign] = NULL;
			
			// first fork
			int id_pipe0 = fork();
			
			if(id_pipe0 < 0)
			{
				printf("fork failed\n");
        		continue;
			}
			else if(id_pipe0 == 0)
			{
				if (pipe(fd_pipe) == -1) 
				{ 
					printf("Pipe failed"); 
					continue; 
				} 
			
				//second fork
				int id_pipe = fork();
				
				if (id_pipe < 0)
				{ 
        			printf("fork failed\n");
        			continue;
				}
				else if (id_pipe == 0)
				{
					dup2(sout,fileno(stdout));
					read(fd_pipe[0], buffer_in, 64); 
					for(int i=0;i<strlen(buffer_in);i++)
						if(buffer_in[i] == '\n') buffer_in[i] = ' ';
					args_in = setup(buffer_in);

					int k = len(args_child);
					for(int i=0;i<len(args_in);i++)
						args_child[k+i] = args_in[i];
				
					if (execvp(args_child[0], args_child) == -1) perror("osh");
					
					exit(0);
				}
				else
				{	
					dup2(fd_pipe[1], fileno(stdout));
				
					if (execvp(args[0], args) == -1) perror("osh");
					
					dup2(sout,fileno(stdout));
				
					wait(NULL);
					exit(0);
				}
			}
			else
			{
				wait(NULL);
				fflush(stdout);
				continue;
			}
		}

		// execute
		int id = fork();
		if (id < 0)
		{ 
        	printf("fork failed\n");
        	should_run = 0;
		}
		else if (id == 0)
		{
			if (execvp(args[0], args) == -1) perror("osh");
			exit(0);
		}
		else
		{
			if(concurrent == 0)
				wait(NULL);
		}
		
		if(redire == 1)
		{
			dup2(sout,fileno(stdout));
			close(fd);
		}
		else if(redire == -1)
		{
			fclose(fp);
		}
			
		fflush(stdout);
	}
}


int main(void)
{
	mainloop();

	return 0;
}

