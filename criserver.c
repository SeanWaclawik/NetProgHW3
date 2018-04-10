/*
 *  criserver.c
 *  
 *  Sean Waclawik
 *  Garret Premo
 *  
 *  Homework 3
 *  Network Programming Spring 2018
 */
 

#include "criserver.h"

int main(int argc, char ** argv)
{
	//declare/initialize variables
	int listensock = 0, nsds = 4, newsock = 0, len = 0, sd = 0;
	struct sockaddr_in6 client;
	fd_set rsds;
	fd_set asds;
	unsigned short port = 0;
	struct client_conn * headclient = NULL;
	struct client_conn * thisclient = NULL;


	
	//check that the user passed in a port argument and set port to its value
	if(argc != 2)
	{
		perror("ERROR: Specify port to run on\n");
		exit(EXIT_FAILURE);
	}

	port = (unsigned short)atoi(argv[1]);
	
	//get a socket in listen mode
	listensock = listen_socket(port);
	
	FD_ZERO(&asds);
	FD_SET(listensock, &asds);
	
	//loop forever both accepting new connections and handling data received on existing connections
	while(1)
	{
		memcpy((char *)(&rsds), (char *)(&asds), sizeof(rsds));
		if((select(nsds, (&rsds), NULL, NULL, NULL)) < 0) error("Detecting which socket(s) have data");
		
		//if there is a new connection pending, handle it
		if((FD_ISSET(listensock, (&rsds))) != 0)
		{
			len = sizeof(client);
			if((newsock = accept(listensock, (struct sockaddr *)(&client), (&len))) < 0) error("Accepting new connection");
			FD_SET(newsock, (&asds));
			nsds++;
			
			//call handle_accept to take care of reading the initial "Name:" identifier message
			if((handle_accept(newsock, (&headclient))) < 0)
			{
				//some sort of non-fatal error occurred in the call to handle_accept - close the connection on this side
				close(newsock);
				FD_CLR(newsock, (&asds));
				nsds--;
			}
		}
		
		//loop through all the open connections and deal with all the ones that have data on them
		thisclient = headclient;
		while(thisclient != NULL)
		{
			if((FD_ISSET((thisclient->sd), (&rsds))) != 0)
			{
				sd = (thisclient->sd);
				//data is waiting on sd.. call handle_message to process the request and inspect the return code
				if((handle_message((thisclient->sd), (thisclient->username), (&headclient))) <= 0)
				{
					//either the client gracefully dropped the connection or a non-fatal error occurred.. remove the socket descriptor
					FD_CLR(sd, (&asds));
					nsds--;
				}
			}
			
			//update the pointer
			thisclient = (thisclient->next);
		}
	}
	
	return 0;
}
