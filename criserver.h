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
#include <netdb.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFSIZE 1024 // bytes
#define NAMESIZE 20 //chars wide


//return codes
#define ERR_NONAME -1
#define ERR_NOCLIENTLIST -2
#define MSG_SUCCESS 1

char *USER = "USER";
char *LIST = "LIST";
char *JOIN = "JOIN";
char *PART = "PART";
char *OPERATOR = "OPERATOR";
char *KICK = "KICK";
char *PRIVMSG = "PRIVMSG";
char *QUIT = "QUIT";

// This struct keeps track of each user's info linking to the next [list]
struct clientStruct
{
	char *username;
	int sd;
	bool op;
	struct clientStruct * next;
	
	struct channelStruct ** channels;
	// dynamic array to hold channels this client is subscribed to
	// UT_array *channels;
	// TODO need to do this before using:
	//////utarray_new(channels, &ut_str_icd);
};

// This struct keeps track of each channels's info linking to the next [list]
struct channelStruct
{
	char *name;
	struct channelStruct * next;
	
	struct clientStruct ** users;
	int num_users;
	// dynamic array to hold subscribed users
	// UT_array *users;
	// TODO need to do this before using:
	// utarray_new(users, &ut_str_icd);

	/////////////////////
	//UT_hash_handle hh; /* makes this structure hashable */
};

// prototypes
void error(char * msg);
int listen_socket(unsigned short port);
char * get_line(int fromsd);
int handle_accept(int fromsd, struct clientStruct ** headclient);
int handle_message(int fromsd, char * username, struct clientStruct ** headclient, 
							struct channelStruct ** headchannel);
void send_message(char * msg, struct clientStruct * headclient);
// void send_message(char * msg, struct channelStruct * headchannel);
char *get_command(char *line, int size);

bool is_cmd_USER(char *cmd) {
	return !strcmp(cmd, USER);
}

bool is_cmd_QUIT(char *cmd) {
	return !strcmp(cmd, QUIT);
}

bool is_cmd_LIST(char *cmd) { return !strcmp(cmd, LIST); }
bool is_cmd_JOIN(char *cmd) { return !strcmp(cmd, JOIN); }
bool is_cmd_PART(char *cmd) { return !strcmp(cmd, PART); }
bool is_cmd_KICK(char *cmd) { return !strcmp(cmd, KICK); }
bool is_cmd_PRIVMSG(char *cmd) { return !strcmp(cmd, PRIVMSG); }
bool is_cmd_OPERATOR(char *cmd) { return !strcmp(cmd, OPERATOR); }

bool is_cmd_ANY(char *cmd) {
	return  strcmp(cmd, USER) == 0 ||
			strcmp(cmd, LIST) == 0 ||
			strcmp(cmd, JOIN) == 0 ||
			strcmp(cmd, PART) == 0 ||
			strcmp(cmd, OPERATOR) == 0 ||
			strcmp(cmd, KICK) == 0 ||
			strcmp(cmd, PRIVMSG) == 0 ||
			strcmp(cmd, QUIT) == 0;
}

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
	struct sockaddr_in server;
	// socklen_t server_len;

	if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("Creating listen socket");
	
	//set up the server address
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port);
	len = (int)sizeof(server);
	// server_len = sizeof(server);
	
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
	char * buffer = (char *)calloc(BUFSIZE, sizeof(char));
	
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

bool channelExists(char *channelname, struct channelStruct ** headchannel) {
	struct channelStruct *thischannel = (*headchannel);
	while (thischannel != NULL) {
		if(!strcmp(channelname, thischannel->name)) {
			return true;
		}
		thischannel = thischannel->next;
	}
	return false;
}

//process the first message a client sends after connecting (the message identifies the client's name)
int handle_accept(int fromsd, struct clientStruct ** headclient)
{
	//declare/initialize variables
	char * buffer = NULL;
	char * name = NULL;
	char * cmd = NULL;
	// char * cmdstr = NULL;
	int len = 0;
	struct clientStruct * newclient;
	newclient = (struct clientStruct *)calloc(1, sizeof(struct clientStruct));
	newclient->sd = fromsd;
	char * announcement = NULL;
	
	if(headclient == NULL) {
		free(newclient);
		return ERR_NOCLIENTLIST;
	}
	
	//get the entire command line sent by the client and parse out the user name
getname:
	buffer = get_line(fromsd);
	len = strlen(buffer);

	//get command from client
	cmd = get_command(buffer, len);
	int clen = strlen(cmd);

	// cmdstr = strsep((&name), " ");
	if(is_cmd_USER(cmd)) {
		
		//get the name that follows the USER command
		name = (char *)calloc(len - clen - 1, sizeof(char));
		strncpy(name, buffer + clen + 1, len - clen - 1);

		//check to make sure that we actually got a name
		if(name == NULL || len-clen-1 == 0) {
			free(newclient); 
			// free(name);
			return ERR_NONAME;
		}
		
		//create a node for the new client in the client list and link it in
		// newclient = (struct clientStruct *)malloc(sizeof(struct clientStruct));
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
		announcement = (char *)calloc(len + 10, sizeof(char));
		strncpy(announcement, "Welcome ", 8);
		strcat(announcement, name);
		strcat(announcement, "\n");
		announcement[len+10] = '\0';

		send_message(announcement, (*headclient));
	}
	else if (is_cmd_QUIT(cmd)) {
		free(cmd);
		free(buffer);
		free(newclient);
		return ERR_NONAME;
	}
	else if (is_cmd_ANY(cmd)) {
		announcement = "Invalid command, please identify yourself with USER.\n";
		printf("%s", announcement);
		send_message(announcement, newclient);
		free(cmd);
		free(buffer);
		goto getname;
	}
	else {
		announcement = "Invalid command.\n";
		printf("%s", announcement);
		send_message(announcement, newclient);
		free(cmd);
		free(buffer);
		goto getname;
	}
	
	free(announcement);
	free(name);
	free(cmd);
	free(buffer);
	return 0;
}

//get passed a connected socket with data waiting to be read and processes whatever request the client may have
//return some positive integer if the message was successfully processed, 0 (zero) if the peer disconnected (either gracefully or prematurely), or possibly a negative integer if some other error occurred
int handle_message(int fromsd, char * username, struct clientStruct ** headclient, struct channelStruct ** headchannel)
{

	char * inmsg = NULL;
	char * outmsg = NULL;
	char * cmd = NULL;
	struct clientStruct * thisclient = NULL;
	struct clientStruct * prevclient = NULL;
	struct channelStruct * thischannel = NULL;
	// struct channelStruct * prevchannel = NULL;

	if(username == NULL) return ERR_NONAME;
	if(headclient == NULL) return ERR_NOCLIENTLIST;
	if((*headclient) == NULL) return ERR_NOCLIENTLIST;
	
	thisclient = (*headclient);
	
	inmsg = get_line(fromsd);
	int len = strlen(inmsg)-1;

	// get command
	cmd = get_command(inmsg, (int)strlen(inmsg));
	int clen = strlen(cmd);

	//check to see if the client terminated the connection
	if(inmsg == NULL || is_cmd_QUIT(cmd))
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
		free(cmd);
		free(outmsg);
		return 0;
	}

	if(is_cmd_ANY(cmd)) {

		/* List command */
		if(is_cmd_LIST(cmd)) {
			int count = 0;
			thischannel = (*headchannel);

			char *channel = NULL; 
			char *channels_string = calloc(BUFSIZE, sizeof(char));
			strcat(channels_string, "* ");
			if(len-clen > 1) {
				channel = (char *)calloc(len - clen, sizeof(char));
				strncpy(channel, inmsg + clen +1, len - clen -1);
			}

			// check if channel exists
			// output users in channel
			while(thischannel != NULL) {
				if(channel != NULL) {
					if(!strcmp(channel, thischannel->name)) {
						char *msg = (char *)calloc(BUFSIZE, sizeof(char));
						int i;
						sprintf(msg, "There are currently %d members.\n%s members: ", thischannel->num_users, channel);
						for(i = 0; i < thischannel->num_users; i++) {
							strcat(msg, thischannel->users[i]->username);
							if(i != thischannel->num_users-1)
								strcat(msg, " ");
						}
						strcat(msg, "\n");
						send_message(msg, (*headclient));
						free(msg);
						free(channels_string);
						channels_string = NULL;
						break;
					}
				}
				strcat(channels_string, thischannel->name);
				if(thischannel->next != NULL) {
					strcat(channels_string, "\n * ");
				}
					
				// prevchannel = thischannel;
				thischannel = thischannel->next;
				count++;
			}
			if(channels_string != NULL) {
				char *msg = calloc(BUFSIZE, sizeof(char));
				sprintf(msg, "There are currently %d channels.\n%s\n", count, channels_string);
				send_message(msg, (*headclient));
				free(msg);
			}
		
			free(channel);
			free(channels_string);
		}
		/* Join command */
		else if(is_cmd_JOIN(cmd)) {
			printf("JOIN COMMAND\n");

			char *channel = NULL;

			if(len-clen-1 > 0) {
				channel = (char *)calloc(len - clen - 1, sizeof(char));
				strncpy(channel, inmsg + clen + 1, len - clen - 1);
			}

			if(channel != NULL) {
				if(channel[0] != '#') {
					printf("channel names must begin with #\n");
				}
				thischannel = (*headchannel);
				while(thischannel != NULL) {
					if(!strcmp(channel, thischannel->name)) {
						break;
					}
					// prevchannel = thischannel;
					thischannel = thischannel->next;
				}

				if(thischannel != NULL) {
					// join channel
				}
				else { // create channel
					thischannel = (struct channelStruct *)calloc(1, sizeof(struct channelStruct));
					thischannel->name = (char *)calloc(20, sizeof(char));
					memcpy(thischannel->name, channel, 20);
					thischannel->num_users = 0;
					thischannel->users = (struct clientStruct **)calloc(1024, sizeof(struct clientStruct*));
					// thischannel->users[thischannel->num_users]->username = username;
					// memcpy(*(thischannel->users[thischannel->num_users]), (*headclient), sizeof(struct clientStruct));
					thischannel->users[thischannel->num_users] = (*headclient);
					thischannel->num_users++;
					thischannel->next = *headchannel;
					*headchannel = thischannel;

					char * announcement = "Joined channel ";
					outmsg = (char *)calloc(strlen(announcement) + strlen(channel)+1, sizeof(char));
					sprintf(outmsg, "%s%s\n", announcement, channel);
					send_message(outmsg, (*headclient));

					free(outmsg);
				}
			}
			// TO DO: join channel that exists 

			// TO DO: if channel does not exist, create new channel
			if(channel != NULL)
				free(channel);
		}
		else if(is_cmd_PART(cmd)) {
			// TO DO: leave channel if we are in specified channel
		}
		else if(is_cmd_KICK(cmd)) {
			// TO DO: check if this user is an operator
		}
		else if(is_cmd_PRIVMSG(cmd)) {
			// TO DO: send message to every user in a channel

			// TO DO: send message to an individual user
		}
		else if(is_cmd_OPERATOR(cmd)) {
			// TO DO: bestow operator status (change flag)
		}
		else {
			// command passed must have been USER...
		}
	} else {
		// invalid command
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

	// 	return 0;
	// }
	// outmsg = (char *)malloc((strlen(username)) + (strlen(inmsg)) + 3);
	// sprintf(outmsg, "%s: %s", username, inmsg);
	
	// //client sent a regular message
	// send_message(outmsg, (*headclient));
	// free(outmsg);
	free(inmsg);
	free(cmd);
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

char *get_command(char *line, int size) {
	int i;
	char *first_word = (char *)calloc(size, sizeof(char));
	for(i = 0; i < size; i++) {
		char c = line[i];
		if(c == ' ' || c == '\t' || c == '\n' || c == '\0')
			break;
		first_word[i] = c;
	}
	return first_word;
}