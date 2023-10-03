#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
	typedef struct {
		time_t timestamp;
		int i;
		char c;
		float f;
	} mensaje;

	const char* server_name = "localhost";
	const int server_port = 8877;

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// creates binary representation of server name
	// and stores it as sin_addr
	// http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
	inet_pton(AF_INET, server_name, &server_address.sin_addr);

	// htons: port in network order format
	server_address.sin_port = htons(server_port);

	// open socket
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Error en creacion de socket\n");
		return 1;
	}

	// data that will be sent to the server
	mensaje data_to_send;
	data_to_send.timestamp = time(NULL);
	data_to_send.i = rand();
	data_to_send.c = 65 + rand()%26;
	data_to_send.f = ((float) rand() / (float) rand());

	sendto(sock, &data_to_send, sizeof(data_to_send), 0,
	           (struct sockaddr*)&server_address, sizeof(server_address));

	// receive

	int n = 0;
	int len = 0, maxlen = 500;
	char buffer[maxlen];
	char* pbuffer = buffer;

	while (1) {
		mensaje data_to_send;
		data_to_send.timestamp = time(NULL);
		data_to_send.i = rand();
		data_to_send.c = 65 + rand()%26;
		data_to_send.f = ((float) rand() / (float) rand());

		sendto(sock, &data_to_send, sizeof(data_to_send), 0,
	           (struct sockaddr*)&server_address, sizeof(server_address));

		pbuffer += n;
		maxlen -= n;
		len += n;

		memcpy(buffer, &data_to_send, sizeof(data_to_send));

		mensaje sended_data;
		memcpy(&sended_data, buffer, sizeof(sended_data));

		printf("Recibido:\n %ld \n %d \n %c \n %f \n",
			sended_data.timestamp, 
			sended_data.i, 
			sended_data.c, 
			sended_data.f
		);
		sleep(1);
	}

	// close the socket
	close(sock);
	return 0;
}
