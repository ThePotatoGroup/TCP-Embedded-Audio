#include <lwip/sockets.h> 
#include "xil_printf.h"
#include "bufferPool_d.h"
#include "audioPlayer.h"
#include "zedboard_freertos.h"
#include "EmbeddedAudioSharedTypes.h"

#define BOARD_BASE_PORT_NUM 12000
#define BOARD_IP_ADDRESS "192.168.1.10"

#define SERVER_BASE_PORT_NUM 5000
#define SERVER_IP_ADDRESS "192.168.1.1"

#define BUFF_SIZE 1024
#define HEAP_SAMPLES_COUNT 20000

#define VOLUME_MAX (0x3F)
#define VOLUME_MIN (0x00)

int convert_volume_percent(unsigned int volume_percent)
{
	printf("put in %d\n",volume_percent);
	unsigned int range = VOLUME_MAX - VOLUME_MIN;
	unsigned int output = (volume_percent*range)/100 + VOLUME_MIN;
	printf("got out %d\n",output);
	return output;
}

xQueueHandle commandQueue;

typedef enum ConnectionState
{
	DISCONNECTED,
	CONNECTED
} ConnectionState;


int connect_to_server(int *socket_fd, char* board_address, int board_port, char* server_address, int server_port)
{
	  struct sockaddr_in sa,ra;

	  // setup addresses
	  memset(&sa, 0, sizeof(struct sockaddr_in));
	  sa.sin_family = AF_INET;
	  sa.sin_addr.s_addr = inet_addr(board_address);
	  sa.sin_port = htons(board_port);

	  /* Receiver connects to server ip-address. */

	  memset(&ra, 0, sizeof(struct sockaddr_in));
	  ra.sin_family = AF_INET;
	  ra.sin_addr.s_addr = inet_addr(server_address);
	  ra.sin_port = htons(server_port);

	  *socket_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);

	  if ( *socket_fd < 0 ) {
	      printf("socket call failed");
	      return 1; // failed to created socket object
	  }

//	  /* Bind the TCP socket to the port SENDER_PORT_NUM and to the current
//	   * machines IP address (Its defined by SENDER_IP_ADDR).
//	   * Once bind is successful for UDP sockets application can operate
//	   * on the socket descriptor for sending or receiving data.
//	   */
//	  if (bind(*socket_fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) == -1)
//	  {
//	    printf("Bind to Port Number %d ,IP address %s failed\n", board_address, board_port);
//	    close(*socket_fd);
//	    return 2; // failed to bind to port
//	  }

	  if(lwip_connect(*socket_fd, (struct sockaddr*)&ra, sizeof(ra)) != 0)
	  {
	    printf("connect failed \n");
	    close(*socket_fd);
	    return 3; // failed to connect to server
	  }
	  printf("Successfully connected to port: %d , at IP address %s\n", server_port, server_address);
	  return 0;
}

void tcp_client(void* pThisArg)
{
	printf("TCP Client starting.\n");

	tAdau1761 codec;
	adau1761_init(&codec);

	adau1761_setVolume(&codec,(convert_volume_percent(50) << 2)|0x02);

	int control_socket_fd, stream_socket_fd;

	uint16_t stream_buffer[BUFF_SIZE];
	control_command_t command_buffer;

	ConnectionState control_connection_status = DISCONNECTED;
	ConnectionState stream_connection_status = DISCONNECTED;

	xQueueHandle sample_queue;

	sample_queue = xQueueCreate(HEAP_SAMPLES_COUNT, sizeof(uint16_t));

	int return_data_len;

	  int i;
	  int ret;
	  fd_set readset;
	  fd_set writeset;
	  fd_set errset;
	  struct timeval tv;
	  int fifo_vac;
	  uint16_t active_sample;

	  tv.tv_sec = 0;
	  tv.tv_usec = 1000;

	  int watchdog = 0;

	  while (1)
	  {
		  if ((control_connection_status == DISCONNECTED) || (stream_connection_status == DISCONNECTED) || (watchdog > 700000))
		  {
			  watchdog = 0;
			  printf("Connection state is bad... reconnecting.\n");
			  close(stream_socket_fd);
			  close(control_socket_fd);
			  vTaskDelay(100);
			  if (!connect_to_server(&stream_socket_fd, BOARD_IP_ADDRESS, BOARD_BASE_PORT_NUM, SERVER_IP_ADDRESS, SERVER_BASE_PORT_NUM))
			  {
				  stream_connection_status = CONNECTED;
			  }

			  if (!connect_to_server(&control_socket_fd, BOARD_IP_ADDRESS, BOARD_BASE_PORT_NUM+1, SERVER_IP_ADDRESS, SERVER_BASE_PORT_NUM+1))
			  {
				  control_connection_status = CONNECTED;
			  }
		  }
		  else
		  {
 			  FD_ZERO(&readset);
			  FD_ZERO(&errset);
			  FD_SET(stream_socket_fd, &readset);
			  FD_SET(stream_socket_fd, &errset);

			  ret = lwip_select(stream_socket_fd + 1, &readset, NULL, &errset, &tv);

			  if (FD_ISSET(stream_socket_fd, &readset))
			  {
				  watchdog = 0;

				  if (uxQueueMessagesWaiting(sample_queue) < (HEAP_SAMPLES_COUNT-BUFF_SIZE))
				  {
					  return_data_len = lwip_read(stream_socket_fd, &stream_buffer, sizeof(stream_buffer));
					  if (return_data_len < 0)
					  {
						  printf("stream is disconnected\n");
						  stream_connection_status = DISCONNECTED;
					  }
					  i=0;
					  for (;i<return_data_len/2;i++)
					  {
						  xQueueSendToBack(sample_queue, (int16_t*)stream_buffer+i,0);
					  }
				  }
				  fifo_vac = ((*(u32 *)(FIFO_BASE_ADDR + FIFO_TX_VAC))&0xFFF);
				  i = 0;
				  if (uxQueueMessagesWaiting(sample_queue) > fifo_vac)
				  {
				  for (;i<fifo_vac;i++)
				  {
					  	xQueueReceive(sample_queue, &active_sample, 0);
				  		int32_t codecFormatSample = ((int32_t)active_sample) << 16;
				  		*(uint32_t *) (FIFO_BASE_ADDR + FIFO_TX_DATA) = codecFormatSample;
				  		*(uint32_t *) (FIFO_BASE_ADDR + FIFO_TX_LENGTH) = 0x00000004; //data length is 4
				  }
				  }

			  }

 			  FD_ZERO(&readset);
			  FD_ZERO(&errset);
			  FD_SET(control_socket_fd, &readset);
			  FD_SET(control_socket_fd, &errset);

			  ret = lwip_select(control_socket_fd + 1, &readset, NULL, &errset, &tv);

			  if (FD_ISSET(control_socket_fd, &readset))
			  {
				  watchdog = 0;
				  return_data_len = lwip_read(control_socket_fd, &command_buffer, sizeof(command_buffer));
				  if (return_data_len < 0)
				  {
					  printf("control is disconnected\n");
					  stream_connection_status = DISCONNECTED;
				  }

				  switch (command_buffer.type)
				  {
				  	  case PLAY:
				  		  printf("Got play command\n");
				  		  break;
				  	  case PAUSE:
				  		  printf("Got pause command\n");
				  		  break;
				  	  case SET_VOLUME:
				  		  printf("Got set_volume command\n");
				  		  printf("setting vol: %d\n",convert_volume_percent((unsigned char)command_buffer.value));
				  		  adau1761_setVolume(&codec,(convert_volume_percent((unsigned char)command_buffer.value) << 2)|0x02);
				  		  break;
				  	  case MUTE:
				  		  printf("the queue has %d samples in it\n",(uxQueueMessagesWaiting(sample_queue)));
				  		  printf("Got mute command\n");
				  		  break;
				  	  case FLUSH_BUFFER:
				  		  while (uxQueueMessagesWaiting(sample_queue) != 0)
				  		  {
					  		  xQueueReceive(sample_queue,&active_sample, 0);
				  		  }
				  		  adau1761_init(&codec);
				  		  break;
				  }
			  }
			  watchdog += 1;
		  }

	  }

	  while (1)
	  {




//		  printf("select says %d\n",FD_ISSET(s, &readset));



//	      recv_data = recv(control_socket_fd, data_buffer, sizeof(data_buffer), 0);
//		  recv_data = lwip_read(control_socket_fd, data_buffer, sizeof(data_buffer));

	  }
}



