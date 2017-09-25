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

int main()
{
	int socket_desc;
	struct sockaddr_in server;
	//--variables to get IP addresses by hostname--//
	struct hostent *he;
	struct in_addr **addr_list;
	char ip[100];
	///----Taking Input fom the User---///
	char s[800];
	char server_reply[2000];
	int c;
	///----define server IP ADDRESS HERE----///
	char input[100] = "localhost";
	
	socket_desc = socket(AF_INET,SOCK_STREAM,0);
		
	if(socket_desc == -1)
	{
		puts("Could not create a socket");
		return 1;
	}
	if((he = gethostbyname(input)) == NULL)
	{
		herror("gethostbyname");
		return 1;
	}
							
	addr_list = (struct in_addr**) he->h_addr_list;
					
	int j;
	for(j = 0; addr_list[j]!=NULL;j++)
	{
		strcpy(ip,inet_ntoa(*addr_list[j]));
	}
	printf("%s resolved to: %s \n",input,ip);
			
	server.sin_addr.s_addr = inet_addr(ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(8888);
							
	if((connect(socket_desc,(struct sockaddr *)&server,sizeof(server)))<0)
	{
		puts("connection error");
		return 1;
	}
	puts("connected");
	while(1)
	{		
		printf("$ ");
		fgets(s,800,stdin);
		s[strlen(s)-1] = '\0';
		if(s[0]=='\0')
		{
			continue;
		}
		c = strcmp(s,"exit");
		if(c==0)
		{
			close(socket_desc);
			break;
		}
		c = strcmp(s,"help");
		if(c == 0)
		{
			puts("pingSites <website-names>");
			puts("showHandles");
			puts("showHandleStatus <handle>");
			continue;
		}
		if(send(socket_desc,s,strlen(s),0)<0)
		{
			puts("Send failed");
			return 1;
		}		
		int read_size;
		memset(server_reply,0,sizeof(server_reply));
		if((read_size = recv(socket_desc,server_reply,2000,0))<0)
		{
			puts("receive failed");
			return 1;
		}
		if(read_size ==0)
		{
			puts("Connection terminated");
			return 1;
		}
		puts(server_reply);
		memset(s,0,sizeof(s));
	}
	close(socket_desc);	
	return 0;
}
