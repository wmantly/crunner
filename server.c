#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "cJSON/cJSON.h"

int main(){
	int create_socket, new_socket;
	socklen_t addrlen;
	int bufsize = 2048000;
	int port = atoi( (getenv("runnerPort") != NULL) ? getenv("runnerPort") : "15000" );
	char *buffer = malloc(bufsize);
	struct sockaddr_in address;

	if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) > 0){
		printf("The socket was created\n");
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(create_socket, (struct sockaddr *) &address, sizeof(address)) == 0){
		printf("Binding Socket on port %i\n", port);
	} else {
		printf("%s\n", "Can not bind to socket!");
		exit(1);
	}

	// loop waiting for client to connect
	while (1){
		if (listen(create_socket, 10) < 0) {
			perror("server: listen");
			exit(1);
		}

		if ((new_socket = accept(create_socket, (struct sockaddr *) &address, &addrlen)) < 0) {
			perror("server: accept");
			exit(1);
		}

		if (new_socket > 0){
			printf("The Client is connected...\n");
		}

		// read content from socket
		recv(new_socket, buffer, bufsize, 0);

		// tell the client to wait for a response
		write(new_socket, "HTTP/1.1 100 Continue\r\n", 28);

		// parse body out of request
		char *body = strstr(buffer,"\r\n\r\n");

		// parse code from the json
		cJSON *root = cJSON_Parse(body);
		char *code = cJSON_GetObjectItem(root, "code")->valuestring;

		// set up code string for POPEN
		char codeToRun[strlen(code)+15];
		strcpy(codeToRun, code);
		strcat(codeToRun, " 2>&1|base64");

		// set up POPEN
		FILE *fp;
		int status;
		if (!(fp = popen(codeToRun, "r"))){
			printf("DIED");
		}

		// build string from popen return
		char *line = NULL, *tmp = NULL;
		size_t size = 0, index = 0;
		int ch = EOF;

		while (ch) {
			ch = getc(fp);
			/* Check if we need to stop. */
			if (ch == EOF){
					ch = 0;
			}
			/* Check if we need to expand. */
			if (size <= index) {
				size += 50;
				tmp = realloc(line, size);
				if (!tmp) {
					free(line);
					line = NULL;
					break;
				}
				line = tmp;
			}
			if(ch == '\n'){
				continue;
			}
			// Actually store the thing.
			line[index++] = ch;
		}

		// clear buffer for next request
		fflush(fp);
		printf("%d\n",pclose(fp));

		// Format JSON response
		char rjson[strlen(line)+20];
		strcpy(rjson,"{\"res\":\"");
		strcat(rjson, line);
		strcat(rjson,"\"}\0");
		
		int rjsonLength = strlen(rjson)+0;
		char rjsonLength_char[20];
		sprintf(rjsonLength_char, "%d", rjsonLength);
		
		// format headers and add body
		char rex[rjsonLength+105];
		strcpy(rex, "HTTP/1.0 200 OK\r\n");
		strcat(rex, "Content-Type: application/json\r\n");
		strcat(rex, "Content-Length: ");
		strcat(rex, rjsonLength_char);
		strcat(rex, "\r\n\r\n");
		strcat(rex, rjson);

		// send response to client
		write(new_socket, rex, strlen(rex));    
		close(new_socket);
	}
	close(create_socket);    
	return 0;    
}
