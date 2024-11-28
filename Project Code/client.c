#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// PDU Struct
struct PDU {
	char type;
	char data[100];
	int size;
};

// Definitions
#define ACKNOWLEDGEMENT 'A'
#define BUFSIZE sizeof(struct PDU)
#define BUFLEN     	100
#define CONTENT 	'C'
#define DEREG     	'T'
#define DOWNLOAD 	'D'
#define ERROR     	'E'
#define MAX_LENGTH 	10
#define ONLINE    	'O'
#define QUIT    	'Q'
#define REGISTER 	'R'
#define SEARCH    	'S'
#define SERVER_PORT 3000
 
// s is the udp socket used to confer with the index
int    	s, n, type, new_tcp_socket;	 
char	peer_name[10];
 
// start up screen for a peer, all possible commands
void printOptions(){
	printf("\nPeer Commands: \n");
printf("'D' - download content\n");
	printf("'O' - list all content registered\n");
printf("'Q' - quit\n");
	printf("'R' - register content\n");
	printf("'S' - search content\n");
	printf("'T' - deregister content\n");
	
}
 
//content registration
void registerContent(char content_name[], char peer[]){
	// ensure file exists on local
	if (access(content_name, F_OK) != 0){
        	printf("This file does not exist");
        	return;
	}

	// tcp connection (start server for registered content)
	int 	tcp_socket, new_sd, client_len, port, alen, message_read_size, assigned_port;
	struct	sockaddr_in server, client;
	struct 	PDU msg, r_msg;
	char 	tcp_address[5], tcp_port[6], send_data[100];
	 
	if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        	fprintf(stderr, "Can't create a socket\n");
        	exit(1);
	}
 
	//create a new message to the index server to register the message
    
	memset(&msg, '\0', sizeof(msg));
	msg.type = REGISTER;

	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(0);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(tcp_socket, (struct sockaddr *)&server, sizeof(server)) == -1){
    	fprintf(stderr, "Can't bind name to socket\n");
    	exit(1);
	}

	// Get the assigned port
	socklen_t len = sizeof(server);
	if (getsockname(tcp_socket, (struct sockaddr *)&server, &len) == -1)
	{
	perror("getsockname failed");
	exit(1);
	}

	assigned_port = ntohs(server.sin_port);
	printf("Assigned port: %d\n", assigned_port);
	snprintf(tcp_port,sizeof(tcp_port),"%d", assigned_port);
	//tcp_port = (char)assigned_port
    
	alen = sizeof(struct sockaddr_in);
	getsockname(tcp_socket, (struct sockaddr*)&server, &alen);
 
    	//concatenate send_data with peer, content name and port
	strcpy(send_data, peer);  
	strcat(send_data, ";");
	strcat(send_data, content_name);  
	strcat(send_data, ";");
	strcat(send_data, tcp_port);
	strcpy(msg.data, send_data);
 
	// Send message to index
	write(s, &msg, sizeof(msg));
 
	// Reads response from index
	memset(&r_msg, '\0', sizeof(r_msg));
	read(s, &r_msg, sizeof(r_msg));
	 
	// If response from index is error, don't register content  
	if (r_msg.type == ERROR){
        	//handle an error
        	printf(r_msg.data);
	}
	// If response from index is acknowledgement, register content
	else if (r_msg.type == ACKNOWLEDGEMENT){
        	// Clear variables  
        	memset(&msg, '\0', sizeof(msg));
        	memset(&r_msg, '\0', sizeof(r_msg));

        	// Split into child and parent to host content
        	listen(tcp_socket, 5);

        	switch (fork()) {
          	case 0:  
            	while (1){
                	// Set up new branch
                	int client_length = sizeof(client);
                	new_tcp_socket = accept(tcp_socket, (struct sockaddr *)&client, &client_length);

                	// Listen to message from other peer
                	read(new_tcp_socket, &r_msg, sizeof(msg));

                	// If message from peer is type 'D', start download process
                	if (r_msg.type == DOWNLOAD){
                        	// Open up the file
                    	char *token = strtok(r_msg.data, ";"); // Assign the tokens to the strings
                    	if (token != NULL) {
                        	strncpy(peer_name, token, MAX_LENGTH - 1);
                        	peer_name[MAX_LENGTH - 1] = '\0'; // Ensure null-termination
                    	}
                    	token = strtok(NULL, ";");
                    	if (token != NULL) {
                        	strncpy(content_name, token, MAX_LENGTH - 1);
                        	content_name[MAX_LENGTH - 1] = '\0';
                    	}
       	 
                        	FILE *file;
                        	file = fopen(content_name, "rb");
                        	if (!file){
                            	printf("This file doesn't exist\n");
                        	}
                        	// If the file exists, iteratively send messages to peer with content of file
                        	else {
                              	// Send file data in chunks of 100 bytes
                            	struct 	PDU s_msg;
                            	int m;
                               	while ((m = fread(s_msg.data, 1, 100, file)) > 0) { //send each file in a pdu
                                  	s_msg.type = CONTENT; //data pdu for file creation
                                 	s_msg.size = m;
                                  	write(new_tcp_socket, &s_msg, sizeof(s_msg));
                            	}
                        	}
                        	// Close file and socket used to communicate with other peer
                        	// as other peer now becomes host of that content
                        	fclose(file);
                        	close(new_tcp_socket);
                        	break;
                	}
                	// If message != 'D' type
                	else {
                    	break;
                	}
            	} //end of while
            	break;
          	case -1:
            	fprintf(stderr, "fork: error\n");
            	break;
        	} //end of switch case
	} //end if type ACKNOWLEDGEMENT
} //end of register content function

//downloading content from another peer
void downloadContent(char content_name[], char address[], char port[]){
   	/* Create a stream socket */
	int             	sd, i, bytesRead, server_port, content_counter = 0;
	struct sockaddr_in  server;
	char            	*host, rbuf[BUFLEN];
	struct PDU      	msg, r_msg;
    
	host = address;
    
	//make TCP connection with peer server
	server_port = atoi(port);
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        	fprintf(stderr, "Can't create a socket\n");
        	exit(1);
	}
    
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(server_port);

	if (inet_pton(AF_INET, host, &server.sin_addr) <= 0) {
        	fprintf(stderr, "Can't get server's address\n");
        	exit(1);
	}

    
	if (connect(sd, (struct sockaddr *)&server, sizeof(server)) <0) {
        	fprintf(stderr, "Can't connect\n");
    	close(sd);
        	exit(1);
	}

	//send request to download content
	msg.type = DOWNLOAD;  
	strcpy(msg.data, peer_name);
	strcat(msg.data, ";");
	strcat(msg.data, content_name);
	write(sd, &msg, sizeof(msg));

	// open file with corresponding file name and insert received data from peer
	FILE *file = fopen(content_name, "wb");
	if (file == NULL) {
        	fprintf(stderr, "Error: can't open file\n");
        	close(sd);
        	return;
	}
    	memset(&r_msg, '\0', sizeof(r_msg));
    

	i = read(sd, & r_msg, sizeof(r_msg));

  	while (i > 0) {
        	// Data received 'C'
        	if (r_msg.type == CONTENT) {
              	fwrite(r_msg.data, sizeof(char), r_msg.size, file);
              	fflush(file);
              	i = read(sd, &r_msg, sizeof(r_msg)); //keep reading the incoming stream
              	// File not found error    
            	content_counter += 1;	 
        	}
    	else if (r_msg.type == ERROR) {
              	printf("%s\n", r_msg.data);
              	break;
        	}
  	}
     	 
	fclose(file);

	// If content was downloaded, register it
	if (content_counter > 0){
        	registerContent(content_name, peer_name);
	}
} //end of download content

// Main part of the program
int main(int argc, char **argv)
{	 
	char	*host = "localhost", now[100];
	int    	port = 3000;
	struct  hostent *phe;	 
	struct  sockaddr_in sin;	 
 
	int    	socket_read_size,c_read_size, error_found =0;
	char	socket_read_buf[BUFSIZE], c_read_buf[100], filename[10], data_send[100];
	struct 	PDU msg, r_msg;
	FILE*	fp;
	char	test_file_name[100] = "", choice[1], content_name[10], address[80]="placeholder address";
	char	temp_port[10], temp_addr[10], temp_content_name[10], temp_peer_name[10];
 
	// Set up the UDP socket
	switch (argc) {
    	case 1:
        	break;
    	case 2:
        	host = argv[1];
    	case 3:
        	host = argv[1];
        	port = atoi(argv[2]);
        	break;
    	default:
        	fprintf(stderr, "usage: UDP File Download [host [port]]\n");
        	exit(1);
	}
 
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
                                                                                    	 
	if ( phe = gethostbyname(host) ){
        	memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	}
	else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
        	fprintf(stderr, "Can't get host entry \n");
                                                                 	 
	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s < 0)
    	fprintf(stderr, "Can't create socket \n");
	 
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        	fprintf(stderr, "Can't connect to %s\n", host);
	}
 
	// get peer name
	memset(&peer_name, '\0', sizeof(peer_name));
	printf("Enter Peer Name:\n");
	c_read_size = read(0, peer_name, BUFSIZE);  
	if (c_read_size >= 10) {  
        	printf("ERROR\n");
        	return -1;
	}
	peer_name[strlen(peer_name)-1] = '\0';
 
	// main program loop
	while(1) {
        	// reset variables from prev. iteration
        	memset(&filename, 0, sizeof(filename));
        	memset(&msg, 0, sizeof(msg));    	 
        	 
        	//display options
        	printOptions();
        	read(0, choice, 2);
        	printf("your choice is: %s\n", choice);
 
       	 
        	if (choice[0] == 'O') { //LIST ONLINE CONTENT  
            	//write request to index
            	msg.type = ONLINE;
            	strcpy(msg.data, "online content");
            	write(s, &msg, BUFSIZE);
           	 
           	//receiver response from index
            	read(s, &r_msg, BUFSIZE);
            	printf("Available Content: ");
            	printf("%s\n", r_msg.data);
        	}
       	 
        	else if (choice[0] == 'R') { //REGISTER CONTENT    
        	msg.type = REGISTER;
            	printf("Please enter the name of the content you'd like to register: ");
            	scanf("%9s", filename);
            	registerContent(filename, peer_name);
        	}
       	 
        	else if (choice[0] == 'D') { //DOWNLOAD CONTENT
            	printf("Please enter the name of the content you'd like to download: ");
            	scanf("%9s", filename);
           	 
            	//send search request to index
            	msg.type = SEARCH;
            	strcpy(msg.data, peer_name);
            	strcat(msg.data, ";");
            	strcat(msg.data, filename);
            	write(s, &msg, BUFSIZE);

            	// Recieve message from index
            	read(s, &r_msg, BUFSIZE);

            	char *token = strtok(r_msg.data, ";"); // Assign the tokens to the strings

            	if (token != NULL) {
                    	strncpy(temp_content_name, token, MAX_LENGTH - 1);
                    	temp_content_name[MAX_LENGTH - 1] = '\0'; // Ensure null-termination
            	}
           	 
            	token = strtok(NULL, ";");
            	if (token != NULL) {
                	strncpy(temp_peer_name, token, MAX_LENGTH - 1);
                	temp_peer_name[MAX_LENGTH - 1] = '\0';
            	}

            	token = strtok(NULL, ";");
            	if (token != NULL) {
                	strncpy(temp_addr, token, MAX_LENGTH - 1);
                	temp_addr[MAX_LENGTH - 1] = '\0';
            	}

            	token = strtok(NULL, ";");  
            	if (token != NULL) {
                	strncpy(temp_port, token, MAX_LENGTH - 1);
                	temp_port[MAX_LENGTH - 1] = '\0';
            	}

            	// Download the content
            	downloadContent(temp_content_name,temp_addr ,temp_port);
        	}
       	 
        	else if (choice[0] == 'T'){ //DEREGISTER CONTENT
            	printf("Please enter the name of the content you'd like to deregister: ");
            	scanf("%s", filename);

            	// send deregister request to index
            	msg.type = DEREG;
            	printf("peer name: %s\n", peer_name);
            	strcpy(msg.data, peer_name);
            	strcat(msg.data, ";");
            	strcat(msg.data, filename);
            	write(s, &msg, BUFSIZE);

            	// receive response from index
            	read(s, &r_msg, BUFSIZE);
            	if (r_msg.type == ACKNOWLEDGEMENT){
                	printf("Content successfully deregistered\n");
            	}
            	else if (r_msg.type == ERROR){
                	printf("Error: %s\n", r_msg.data);
            	}
        	}
       	 
        	else if (choice[0] == 'S'){//SEARCH
            	printf("Please enter the name of the content you'd like to search for: ");
            	scanf("%9s", filename);

            	// send search request to index
            	msg.type = SEARCH;
            	strcpy(msg.data, peer_name);
            	strcat(msg.data, ";");
            	strcat(msg.data, filename);
            	write(s, &msg, BUFSIZE);
           	 
            	//clear content of r_msg
            	memset(&r_msg, '\0', sizeof(r_msg));
            	read(s, &r_msg, sizeof(r_msg));
           	 
            	// if error , don't register
            	if (r_msg.type == ERROR){
                	//handle an error
                	printf(r_msg.data);
            	}
            	// if search type, register
            	else if (r_msg.type == SEARCH){
                	// use strtok to split the input by semicolons
                	char *token = strtok(r_msg.data, ";"); // assign the tokens to the strings
                	if (token != NULL) {
                    	strncpy(temp_content_name, token, MAX_LENGTH - 1);
                    	temp_content_name[MAX_LENGTH - 1] = '\0'; // ensure null-termination
                	}

                	token = strtok(NULL, ";");
                	if (token != NULL) {
                    	strncpy(temp_peer_name, token, MAX_LENGTH - 1);
                    	temp_peer_name[MAX_LENGTH - 1] = '\0';
                	}

                	token = strtok(NULL, ";");
                	if (token != NULL) {
                    	strncpy(temp_addr, token, MAX_LENGTH - 1);
                    	temp_addr[MAX_LENGTH - 1] = '\0';
                	}

                	token = strtok(NULL, ";");  
                	if (token != NULL) {
                    	strncpy(temp_port, token, MAX_LENGTH - 1);
                    	temp_port[MAX_LENGTH - 1] = '\0';
                	}
    	 
                	printf("Here is the information you searched for: \n");
                	printf("Content Name: %s\n", temp_content_name);
                	printf("Peer Name: %s\n", temp_peer_name);
                	printf("Address: %s\n", temp_addr);
                	printf("Port Number: %s\n", temp_port);
            	}
       	}
       	 
        	else if (choice[0] == 'Q') { //QUIT
            	//send quit request to index
            	msg.type = QUIT;
            	strcpy(msg.data, peer_name);
            	write(s, &msg, BUFSIZE);

            	//receive response from index
            	read(s, &r_msg, BUFSIZE);

            	// If index returns back a QUIT then peer can quit(no content on register)
            	if (r_msg.type == QUIT){
                	break;
            	}
            	// If index returns back an ERROR then peer cannot QUIT
            	//peer still has content on server
            	else if (r_msg.type == ERROR){
                	printf("%s\n", r_msg.data);
            	}
        	}
	}
	close(s);
	exit(0);
}



