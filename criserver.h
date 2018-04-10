/*
 *  criserver.h
 *  
 *  Sean Waclawik
 *  Garret Premo
 *  
 *  Homework 3
 *  Network Programming Spring 2018
 */

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 1024 // bytes
#define NAMESIZE 20 //chars wide


//return codes
#define ERR_NONAME -1
#define ERR_NOCLIENTLIST -2
#define MSG_SUCCESS 1

// This struct keeps track of each user's info linking to the next [list]
struct clientStruct
{
	char username[20];
	int sd;
	struct clientStruct * next;
	
	// dynamic array to hold channels this client is subscribed to
	UT_array *channels;
	// TODO need to do this before using:
	//////utarray_new(channels, &ut_str_icd);
};

// This struct keeps track of each channels's info linking to the next [list]
struct channelStruct
{
	char * name[20];
	struct channelStruct * next;
	
	// dynamic array to hold subscribed users
	UT_array *users;
	// TODO need to do this before using:
	utarray_new(users, &ut_str_icd);

	/////////////////////
	//UT_hash_handle hh; /* makes this structure hashable */
};

// prototypes
void error(char * msg);
int listen_socket(unsigned short port);
char * get_line(int fromsd);
int handle_accept(int fromsd, struct clientStruct ** headclient);
int handle_message(int fromsd, char * username, struct clientStruct ** headclient);
void send_message(char * msg, struct clientStruct * headclient);
void send_message(char * msg, struct channelStruct * headchannel);

//prints message and errors out
void error(char * msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

//returns a sock in listen mode bound to port
int listen_socket(unsigned short port)
{
	int sd = 0, len = 0;
	struct sockaddr_in6 server;

	if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("Creating listen socket");
	
	//set up the server address
	server.sin_family = AF_INET;
	server.sin_addr = inaddr_any;
	server.sin_port = htons(port);
	len = (int)sizeof(server);
	server.sin_len = (char)sizeof(server);
	
	//bind the socket to the local host on the requested port
	if((bind(sd, (struct sockaddr *)(&server), len)) < 0) error("Binding socket");
	
	//put the socket into passive mode
	if((listen(sd, 5)) < 0) error("Listening");
	
	return sd;
}

//read input from fromsd into a newly allocated buffer and return a pointer 
char * get_line(int fromsd)
{
	int numbytes = 0, bytesread = 0, bufsize = BUFSIZE;
	char * buffer = (char *)malloc(BUFSIZE);
	
	//read bytes from fromsd until we get a newline indicating the end
	buffer[0] = '\0';
	while((buffer[(bytesread - 1)]) != '\n')
	{
		numbytes = recv(fromsd, (buffer + bytesread), (bufsize - bytesread - 1), 0);
		
		//check the return value from recv to handle errors or a prematurely terminated connection
		if(numbytes < 0) error("Reading request from client");
		else if(numbytes == 0)
		{
			free(buffer);
			return NULL;
		}
		
		bytesread += numbytes;
		
		//pad the buffer to make it a proper C-string
		buffer[bytesread] = '\0';
		
		//resize the buffer if necessary
		if(bytesread >= (bufsize - 1))
		{
			bufsize += BUFSIZE;
			if((realloc((void *)buffer, bufsize)) == NULL)
			{
				fprintf(stderr, "Error reallocating memory!\n");
				exit(EXIT_FAILURE);
			}
		}
	}
	
	return buffer;
}

//process the first message a client sends after connecting (the message identifies the client's name)
int handle_accept(int fromsd, struct clientStruct ** headclient)
{
	//declare/initialize variables
	char * name = NULL;
	char * cmdstr = NULL;
	int len = 0;
	struct clientStruct * newclient = NULL;
	char * announcement = NULL;
	
	if(headclient == NULL) return ERR_NOCLIENTLIST;
	
	//get the entire command line sent by the client and parse out the user name
	name = get_line(fromsd);
	cmdstr = strsep((&name), " ");
	
	//check to make sure that we actually got a name
	if(name == NULL) return ERR_NONAME;
	
	//create a node for the new client in the client list and link it in
	newclient = (struct clientStruct *)malloc(sizeof(struct clientStruct));
	len = strlen(name);
	(newclient->username) = (char *)malloc(len);
	//ignore the \n at the end of the name string by setting len--
	len--;

	strncpy((newclient->username), name, len);
	(newclient->username)[len] = '\0';
	(newclient->sd) = fromsd;
	(newclient->next) = (*headclient);
	(*headclient) = newclient;
	
	//send a message announcing the arrival of the new client
	name[len]='\0';
	announcement = (char *)malloc(len + 9);
	strncpy(announcement, "Welcome ", 8);
	strcat(announcement, name);
	announcement[len] = '\0';

	send_message(announcement, (*headclient));
	
	free(announcement);
	free(cmdstr);
	
	return 0;
}

//get passed a connected socket with data waiting to be read and processes whatever request the client may have
//return some positive integer if the message was successfully processed, 0 (zero) if the peer disconnected (either gracefully or prematurely), or possibly a negative integer if some other error occurred
int handle_message(int fromsd, char * username, struct clientStruct ** headclient)
{

	char * inmsg = NULL;
	char * outmsg = NULL;
	struct clientStruct * thisclient = NULL;
	struct clientStruct * prevclient = NULL;
	
	if(username == NULL) return ERR_NONAME;
	if(headclient == NULL) return ERR_NOCLIENTLIST;
	if((*headclient) == NULL) return ERR_NOCLIENTLIST;
	
	thisclient = (*headclient);
	
	inmsg = get_line(fromsd);
	
	//check to see if the client terminated the connection
	if(inmsg == NULL)
	{
		//client is terminating session
		
		
		//remove the client's node from the list pointed to by headclient
		while(thisclient != NULL)
		{
			//if we found the node that send the message...
			if((thisclient->sd) == fromsd)
			{
				close(fromsd);
				
				//unlink the node from the list
				if(prevclient != NULL)
				{
					(prevclient->next) = (thisclient->next);
					free((thisclient->username));
					free(thisclient);
				}
				else
				{
					(*headclient) = ((*headclient)->next);
					free((thisclient->username));
					free(thisclient);
				}
				
				break;
			}
			
			prevclient = thisclient;
			thisclient = (thisclient->next);
		}

		free(outmsg);
		return 0;
	}
	
	//process the command message from the client
	//
	//
	//
	//
	//
	// TO DO: make function to handle various commands KICK, LSIT, JOIN, etc. 
	//
	//
	//
	//

		return 0;
	}
	
	outmsg = (char *)malloc((strlen(username)) + (strlen(inmsg)) + 3);
	sprintf(outmsg, "%s: %s", username, inmsg);
	
	//client sent a regular message
	send_message(outmsg, (*headclient));
	free(outmsg);
	free(inmsg);
	return MSG_SUCCESS;
}

//sends the string at msg to every client in the list pointed to by headclient
void send_message(char * msg, struct clientStruct * headclient)
{
	struct clientStruct * thisclient = headclient;
	
	//do some minor parameter checking
	if(msg == NULL) return;
	if(thisclient == NULL) return;
	
	//loop through all the clients in the list and send the message to each one
	while(thisclient != NULL)
	{
		if((send((thisclient->sd), msg, (strlen(msg)), 0)) < 0) error("Sending messages");
		thisclient = (thisclient->next);
	}
}
