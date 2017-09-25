#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>

#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include<semaphore.h>
#include<time.h>

void* thread_handler(void*); //thread function for each seperate client
void* worker_child(void*);//worker Thread function

typedef struct node node;//queuenode
typedef struct queue queue;//queue

int handle_count; sem_t m; sem_t w_sem; sem_t w_mut;
//---Structure for each website in the queue--//
struct queue_element
{
	int handle; char *name; int avg_time; int min_time; int max_time; char* status;
};
//---Structure for each Node in the queue
struct node
{
    struct queue_element* element; node *next;
};
//---queue structure
struct queue 
{
    node *head; node *tail;
};
//--FIFO- pops the head of the queue---//
struct queue_element* pop(queue *q)
{
	if(q==NULL)
	{
		return NULL;
	}
    if (q->head == NULL)
    {
    	return NULL;
	}
    struct queue_element *queue_node = q->head->element;
    node *tmp = q->head;
    q->head = q->head->next;
    free(tmp);
    return queue_node;
}
//---pushes the new element to the end of the queue
int push(queue *q, struct queue_element *element)
{
    node *new = (node*)malloc(sizeof(node));
    if (new == NULL)
        return -1;
    new->element = element;
    new->next = NULL;
    if (q->tail != NULL)
        q->tail->next = new;

    q->tail = new;
    if (q->head == NULL) /* first value */
        q->head = new;
    return 0;
}
queue* q = NULL; queue* q2 = NULL; queue *h_status[100];
int main()
{
	
	sem_init(&m,0,1); //--**for handle_count**--//
	
	int socket_desc, new_socket, c,*new_sock;
	
	struct sockaddr_in server, client;
	
	char *message, client_message[2000];
	
	socket_desc = socket(AF_INET,SOCK_STREAM,0);
	
	//---Creates a Socket--//
	if(socket_desc == -1)
	{
		puts("Could not create a socket");
		return 1;
	}
	
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(8888);
	//---Binds a server to the required port 8888 here.
	if(bind(socket_desc,(struct sockaddr*)&server,sizeof(server))<0)
	{
		perror("bind failed"); 
		close(socket_desc);
		return 1;
	}
	puts("bind done");
	
	listen(socket_desc,3);
	puts("waiting for incoming connections....");
	c = sizeof(struct sockaddr_in);
	while((new_socket = accept(socket_desc,(struct sockaddr*)&client,(socklen_t*)&c)))
	{
		puts("Connection accepted");
		
		pthread_t child;
		new_sock = malloc(1);
		*new_sock = new_socket;
		
		if(pthread_create(&child,NULL,thread_handler,(void*)new_sock)<0)
		{
			perror("could not create thread");
			return 1;
		}
	}
	close(socket_desc);
	return 0;
}

//---Connects the required website to the TCP port 80.
int connectosock(char* host,const int port)
{
	struct hostent* h_info;
	struct sockaddr_in s_in;
	int s;
	
	memset(&s_in,0,sizeof(s_in));
	s_in.sin_family = AF_INET;
	
	if((s_in.sin_port = htons(port))==0)
	{
		puts("Can't get port num");
		exit(1);
	}
	
	if(h_info = gethostbyname(host))
	{
		memcpy(&s_in.sin_addr,h_info->h_addr,h_info->h_length);
	}
	else if((s_in.sin_addr.s_addr = inet_addr(host))==INADDR_NONE)
	{
		puts("host entry not found");
		exit(1);
	}
	
	s = socket(AF_INET,SOCK_STREAM,0);
	if(s<0)
	{
		puts("socket not created");
		exit(1);
	}
	
	if(connect(s,(struct sockaddr*)&s_in,sizeof(s_in))<0)
	{
		puts("connection-failed");
		exit(1);
	}
	return s;
}

//----Thread function for Worker Threads.
void *worker_child(void* arg)
{
	queue* q_ele = (queue*)arg; clock_t start,end; int max =0,min ,avg;
	float s = 0; int sd; int time; char name[200]; struct queue_element* qe;
	while(q_ele==NULL)
	{
		sem_wait(&w_sem);
		puts("q-NULL");	
	}
	sem_wait(&w_mut);
	qe = pop(q_ele);
	sem_post(&w_mut);
	while(qe==NULL)
	{
		sem_wait(&w_sem);
		//puts("qe-NULL");
	}
	sem_wait(&w_mut);
	strcpy(name,qe->name);
	int i;
	for(i=0;i<10;i++)
	{
		qe->status = "IN_PROGRESS";
		start = clock();
		sd = connectosock(name,80);
		close(sd);
		end = clock();
		time = ((int)(end-start));
		if(i==0)
		{
			min = time;
		}
		s = s + time;
		if(time>=max)
		{
			max = time;
		}
		if(time<=min)
		{
			min = time;
		}
	}
	qe->max_time = max;
	qe->min_time = min;
	qe->avg_time = (int)(s/10);
	qe->status = "COMPLETE";
	sem_post(&w_mut);
	sem_wait(&w_sem);
}
//---Thread function for seperate client.
void* thread_handler(void* socket_desc)
{
	int sock = *((int*)socket_desc);
	int read_size;
	char message[500],client_message[2000];
	char *handle_array[100];
	while((read_size = recv(sock,client_message,2000,0))>0)
	{
		char *s = client_message;
		char *k;
		k = strtok(s," ,");
		int i = 0;
		char *args[20];
		while(k!=NULL&&i<20)
		{
			args[i] = strdup(k);
			printf("args[%d]:%s\n",i,args[i]);
			i++;
			k = strtok(NULL," ,");
		}
		args[i] = NULL;
		//Parsing pingSites.
		if(strcmp(args[0],"pingSites") == 0)
		{
			sem_wait(&m);
			handle_count++;
			memset(message,0,sizeof(message));
			sprintf(message,"%d",handle_count);
			if(send(sock,message,strlen(message),0)<0)
			{
				puts("send failed");
			}
			q = (queue*) malloc(sizeof(queue));
			q2 = (queue*) malloc(sizeof(queue));
			pthread_t worker[5];
			for(i=1;args[i]!=NULL;i++)
			{
				struct queue_element* webaddress = (struct queue_element*)malloc(sizeof(struct queue_element));
				webaddress->name = args[i];
				webaddress->handle = handle_count;
				webaddress->avg_time = 0;
				webaddress->max_time = 0;
				webaddress->min_time = 0;
				webaddress->status = "IN_QUEUE";
				push(q,webaddress);
				push(q2,webaddress);
			}
			sem_init(&w_sem,0,0);
			sem_init(&w_mut,0,1);
			for(i=0;i<5;i++)
			{
				
				pthread_create(&worker[i],NULL,worker_child,(void*)q);
			}
			for(i=1;args[i]!=NULL;i++)
			{
				sem_post(&w_sem);
			}
			h_status[handle_count-1] = q2;
			sem_post(&m);
		}
		//----Pparsing showHandles.
		else if(strcmp(args[0],"showHandles")==0)
		{
			 char message1[100];
			 char result[200];
				memset(message1,0,sizeof(message1));
				memset(result,0,sizeof(result));
			//sprintf(message,"%s","This command returns the handles of different requests made by all clients of the server");
			for(i = 0;i<handle_count;i++)
			{
			//fprintf(stdout,"%d",i);
			//puts(handle_array[i]);
			sprintf(message1,"%d\n",i+1);
			strcat(result,message1);
			}
			if(handle_count==0)
			{
				sprintf(result,"%s\n","No Handles spawned yet!!");
			}
			result[strlen(result)-1] = '\0';
			if(send(sock,result,strlen(result),0)<0)
			{
				puts("send failed");
			}
		}
		//---Parsing showHandleStatus
		else if(strcmp(args[0],"showHandleStatus") == 0)
		{
			if(args[1]==NULL)
			{
				memset(message,0,sizeof(message));
				char message2[300];
				//sprintf(message,"%s","returns the results of all the handles");
				char result2[500];
				char result3[4000];
				memset(result3,0,sizeof(result3));
				node* inode;
				for(i=0;i<handle_count;i++)
				{
					memset(result2,0,sizeof(result2));
					inode = h_status[i]->head;
					if(inode == NULL)
					{
						sprintf(result2,"%d %s\n",i+1,"No sites are given as the argument");
					}
					while(inode!=NULL)
					{
						memset(message2,0,sizeof(message2));
						sprintf(message2,"%d %s %d %d %d %s \n",inode->element->handle,
						inode->element->name,inode->element->avg_time,inode->element->min_time,
						inode->element->max_time,inode->element->status);
						strcat(result2,message2);
						inode = inode->next;
					}
					strcat(result3,result2);
				}
				memset(result2,0,sizeof(result2));
				if(handle_count==0)
				{
					sprintf(result3,"%s\n","No Handles spawned yet!!");
				}
				result3[strlen(result3)-1] ='\0';
				if(send(sock,result3,strlen(result3),0)<=0)
				{
					puts("send failed");
				}
				//memset(result3,0,sizeof(result3));
				continue;
			}
			memset(message,0,sizeof(message));
			//fprintf(stdout,"%d\n",atoi(args[1]));
			for(i=0;i<handle_count;i++)
			{
				//fprintf(stdout,"%d\n",i);
				if((i+1)==atoi(args[1]))
				{
					node* inode;
					inode = h_status[i]->head;
					if(inode == NULL)
					{
						sprintf(message,"%d %s\n",i+1,"No sites are given as the argument");
					}
					while(inode!=NULL)
					{
						sprintf(message,"%d %s %d %d %d %s \n",inode->element->handle,
						inode->element->name,inode->element->avg_time,inode->element->min_time,
						inode->element->max_time,inode->element->status);
						inode = inode->next;
					}
				}
			}
			if(handle_count==0)
			{
				sprintf(message,"%s\n","No Handles spawned yet!!");
			}
			message[strlen(message)-1]='\0';
			//sprintf(message,"%s","this command sends a specific handle to the server and status of the client is returned by server and print its status");
			if(send(sock,message,strlen(message),0)<=0)
			{
					puts("send failed");
			}
		}
		//if wrong Command is given;
		else
		{
			memset(message,0,sizeof(message));
			strcpy(message,"Please enter the correct command, use help to find available commands");
			if(send(sock,message,strlen(message),0)<0)
			{
				puts("send failed");
			}
		}
		
		memset(client_message,0,sizeof(client_message));
	}
	if(read_size == 0)
	{
		puts("client disconnected");
		fflush(stdout);
	}
	else if(read_size == -1)
	{
		perror("recv failed");
	}
	free(socket_desc);
	return 0;
}
	
	
