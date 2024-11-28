#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
 
//PDU struct
struct PDU {
	char type;
	char data[100];
};
 
//registered content
typedef struct{
	char content[10];
	char peer[10];
	char address[10];
	char port_number[10];
} reg_data;
 
// Definitions
#define BUFSIZE 	sizeof(struct PDU)
#define ERROR     	'E' //report error
#define ONLINE    	'O'
#define QUIT    	'Q'
#define REGISTER	'R'
#define DEREG    	'T'
#define SEARCH    	'S'
#define ACKNOWLEDGEMENT	'A'
#define MAX_LENGTH 	10
 
//main
int main(int argc, char *argv[])
{
	// Variables
	struct  sockaddr_in fsin;
	struct  sockaddr_in sin;  	 
	char	*pts, buf[100];	 
	int	sock,alen,s, type, error_found =0, port=3000;    	;    	 
	time_t	now;	 

	 
	struct PDU msg, send_msg;
	int 	socket_read_size, i=0,file_size, registered=0, found = 0, placement = 0,reg_or_dereg[] = {0,0,0,0,0,0,0,0,0,0};
//used to keep track of whtat "exists" in index
//this includes content that has been deregistered (0 is deregistered, 1 is registered)
	char	socket_buf[BUFSIZE],file_name[100], online_content[100];
	char data_recieved[100], peer_name[10], content_name[10], address[10], port_num[10];
	reg_data reg_content[10];
 

	switch(argc){
    	case 1:
        	break;
    	case 2:
        	port = atoi(argv[1]);
        	break;
    	default:
        	fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        	exit(1);
	}
 	// udp connection
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
                                                                                             	 
	/* Allocate a socket */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
    	fprintf(stderr, "can't creat socket\n");
	}
	 
	/* Bind the socket */
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    	fprintf(stderr, "can't bind to %d port\n",port);
	}
 
	listen(s, 5);	 
	alen = sizeof(fsin);
 
	//main index loop
	while (1) {
	 
        	//reset memory of variables
        	memset(&send_msg, '\0', sizeof(send_msg));
        	memset(&data_recieved, '\0', sizeof(data_recieved));
        	registered = 0;
        	found = 0;
        	error_found = 0;
        	 
        	//receive message from peer
        	if (recvfrom(s, socket_buf, sizeof(socket_buf), 0, (struct sockaddr *)&fsin, &alen) < 0) {
        	printf("recvfrom error\n");
        	}  
        	//put message content into PDU
        	memcpy(&msg, socket_buf, sizeof(socket_buf));  
        	 
       	 
        	if (msg.type == ONLINE) {//LIST ONLINE CONTENT
        	printf("Online content request\n");
        	memcpy(data_recieved, msg.data, sizeof(socket_buf) - 1);
        	send_msg.type = ONLINE;
        	//divide content with a comma
        	for (i=0; i<10; ++i){
                	if (reg_or_dereg[i]==1){
                	strcat(send_msg.data, reg_content[i].content);
                	strcat(send_msg.data, ", ");
                	}
        	}
        	//send to peer
        	sendto(s, &send_msg, BUFSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin));
        	}
    	 
        	if (msg.type == DEREG) {//DEREGISTER
        	printf("Deregister content request\n");
        	memcpy(data_recieved, msg.data, BUFSIZE - 1);
        	// Use strtok to split the input by semicolons
        	char *token = strtok(data_recieved, ";"); // Assign the tokens to the strings
        	if (token != NULL) {
            	strncpy(peer_name, token, MAX_LENGTH - 1);
            	peer_name[MAX_LENGTH - 1] = '\0'; // Ensure null-termination
        	} token = strtok(NULL, ";");
         	if (token != NULL) {
            	strncpy(content_name, token, MAX_LENGTH - 1);
            	content_name[MAX_LENGTH - 1] = '\0';
        	} token = strtok(NULL, ";");  

        	//print peer name and content they want to deregister
        	printf("peer name: %s\n", peer_name);
        	printf("content name: %s\n", content_name);
	 
        	//check to see content exists
        	for (i=0; i<10; ++i){
                	if ((strcmp(reg_content[i].content, content_name)==0) &&
                	(reg_or_dereg[i]==1) && (strcmp(reg_content[i].peer, peer_name)==0)){
                    	placement = i;
                    	found = 1;
                	}
        	}
        	// if content isnt found return an error
        	if (found == 0){
                	send_msg.type = ERROR;
                	strcpy(send_msg.data, "This content is not registered with this peer");
        	}
        	// if content found, dereg and send ack
        	else if (found == 1){
                	reg_or_dereg[placement] = 0;
                	send_msg.type = ACKNOWLEDGEMENT;  
                	strcpy(send_msg.data, "content has been deregistered");
        	}
        	//send to peer
        	sendto(s, &send_msg, BUFSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin));
        	}
       	 
        	else if (msg.type == SEARCH){//SEARCH
        	printf("Search content request\n");
        	memcpy(data_recieved, msg.data, BUFSIZE - 1);

        	// Use strtok to split the input by semicolons
         	char *token = strtok(data_recieved, ";"); // Assign the tokens to the strings
        	if (token != NULL) {
            	strncpy(peer_name, token, MAX_LENGTH - 1);
            	peer_name[MAX_LENGTH - 1] = '\0'; // Ensure null-termination
        	}
        	token = strtok(NULL, ";");
        	if (token != NULL) {
            	strncpy(content_name, token, MAX_LENGTH - 1);
            	content_name[MAX_LENGTH - 1] = '\0';
        	}
	 

        	// print peer name and content being search for
        	printf("peer name: %s\n", peer_name);
        	printf("content name: %s\n", content_name);
	 
        	// check if content exists
        	for (i=9; i>=0; --i){
            	if ((strcmp(reg_content[i].content, content_name)==0) && reg_or_dereg[i]==1){
                	found = 1;
        	placement=i;
break;
            	}
        	}
        	// if content does not exist return back error
        	if (found == 0){
            	send_msg.type = ERROR;
            	strcpy(send_msg.data, "No content by this name and peer was found");
        	}
        	//If the content exists, send back an 'S' type message with the address and port number
        	else if (found == 1){
                	send_msg.type = SEARCH;
                	strcpy(send_msg.data, reg_content[placement].content);
                	strcat(send_msg.data, ";");
                	strcat(send_msg.data, reg_content[placement].peer);
                	strcat(send_msg.data, ";");
                	strcat(send_msg.data, reg_content[placement].address);
                	strcat(send_msg.data, ";");
                	strcat(send_msg.data, reg_content[placement].port_number);
        	}
        	// send to peer
        	sendto(s, &send_msg, BUFSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin));
        	}
       	 
        	else if (msg.type == QUIT){
        	printf("The peer wants to quit the program\n");
        	memcpy(peer_name, msg.data, BUFSIZE - 1);
        	//check to see if they have any content open
        	for (i=0; i<10; ++i){
                	if ((strcmp(reg_content[i].peer, peer_name)==0) && reg_or_dereg[i]==1){
                	found = 1;
                	}
        	}
        	// if they have content still registered, return error to dereg content first
        	if (found == 1){
                	send_msg.type = ERROR;
                	strcpy(send_msg.data, "unable to close, content still registered to this peer");
                	sendto(s, &send_msg, BUFSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin));
        	}
        	// if good to quit, then send back quit type
        	else if (found == 0){
                	send_msg.type = QUIT;
                	sendto(s, &send_msg, BUFSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin));
        	}
        	}
       	 
        	else if (msg.type == REGISTER){//REGISTER
        	char client_ip[INET_ADDRSTRLEN];
       	 
            	//get ip of client
            	if (inet_ntop(AF_INET, &fsin.sin_addr, client_ip, sizeof(client_ip)) == NULL) {
            	perror("inet_ntop failed");
            	continue;
        	}

        	printf("The peer wants to register new data\n");
        	memcpy(data_recieved, msg.data, BUFSIZE - 1);

        	// Use strtok to split the input by semicolons
        	char *token = strtok(data_recieved, ";"); // Assign the tokens to the strings
        	if (token != NULL) {
            	strncpy(peer_name, token, MAX_LENGTH - 1);
            	peer_name[MAX_LENGTH - 1] = '\0'; // Ensure null-termination
        	}
        	token = strtok(NULL, ";");
        	if (token != NULL) {
            	strncpy(content_name, token, MAX_LENGTH - 1);
            	content_name[MAX_LENGTH - 1] = '\0';
        	}
        	token = strtok(NULL, ";");
        	if (token != NULL) {
            	strncpy(port_num, token, MAX_LENGTH - 1);
             	port_num[MAX_LENGTH - 1] = '\0';
        	}


        	//print info received
        	printf("peer name: %s\n", peer_name);
        	printf("content name: %s\n", content_name);
        	printf("address: %s\n", client_ip);
        	printf("port: %s\n", port_num);
        	//ensure peer isnt trying to register same file twice
        	for (i=0; i<10; ++i){
                	if ((strcmp(reg_content[i].peer,peer_name)==0) &&
                	(strcmp(reg_content[i].content, content_name)==0) &&
                 	reg_or_dereg[i]==1){
                    	error_found = 1;
                	}
        	}
        	//update content register
        	if (error_found ==0){
                	for (i=0; i<10; ++i){
                	if ((reg_or_dereg[i]==0) && (registered ==0)){
                        	strcpy(reg_content[i].content, content_name);
                        	strcpy(reg_content[i].peer, peer_name);
                        	strcpy(reg_content[i].address, client_ip);
                        	strcpy(reg_content[i].port_number, port_num);
                        	registered = 1;
                        	reg_or_dereg[i] = 1;
                	}
                	}	 
        	}    
        	//send ack that content was successfully registered
        	if (registered == 1){
                	send_msg.type = ACKNOWLEDGEMENT;  
                	strcpy(send_msg.data, "content has been registered");
        	}
        	//send an error that the content was already registered
        	else if (registered == 0){
                	send_msg.type = ERROR;  
                	strcpy(send_msg.data, "content was already previously registered");
        	}
        	//send message to peer
        	sendto(s, &send_msg, BUFSIZE, 0, (struct sockaddr *)&fsin, sizeof(fsin));
	 
        	}
	}
	//exit
	close(s);
	exit(0);
}










