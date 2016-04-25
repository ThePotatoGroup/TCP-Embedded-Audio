#include <lwip/sockets.h> 
#include "xil_printf.h"
#include "bufferPool_d.h"
#include "audioPlayer.h"
#include "EmbeddedAudioSharedTypes.h"

#define BOARD_BASE_PORT_NUM 12000
#define BOARD_IP_ADDRESS "192.168.1.10"

#define SERVER_BASE_PORT_NUM 5000
#define SERVER_IP_ADDRESS "192.168.1.1"

#define BUFF_SIZE 1024

typedef enum ConnectionState
{
	DISCONNECTED,
	STREAM_ONLY,
	CONTROL_ONLY,
	FULLY_CONNECTED
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

	  socket_fd = socket(PF_INET, SOCK_STREAM, 0);

	  if ( socket_fd < 0 ) {
	      printf("socket call failed");
	      return 1; // failed to created socket object
	  }

	  /* Bind the TCP socket to the port SENDER_PORT_NUM and to the current
	   * machines IP address (Its defined by SENDER_IP_ADDR).
	   * Once bind is successful for UDP sockets application can operate
	   * on the socket descriptor for sending or receiving data.
	   */
	  if (bind(socket_fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) == -1)
	  {
	    printf("Bind to Port Number %d ,IP address %s failed\n", board_address, board_port);
	    close(socket_fd);
	    return 2; // failed to bind to port
	  }

	  if(connect(socket_fd, (struct sockaddr_in*)&ra, sizeof(struct sockaddr_in)) < 0)
	  {
	    printf("connect failed \n");
	    close(socket_fd);
	    return 3; // failed to connect to server
	  }
}

void tcp_client(void* pThisArg) {
  audioRx_t *pThis = (audioRx_t*)pThisArg;
  int stream_socket_fd, control_socket_fd;
  struct sockaddr_in sa,ra;
  ConnectionState connection_state = DISCONNECTED;

  while(1)
  {
		  switch (connection_state)
		  {
		  	  case DISCONNECTED:
		  		  connect_to_server(&stream_socket_fd, BOARD_IP_ADDRESS, BOARD_BASE_PORT_NUM, SERVER_IP_ADDRESS, SERVER_BASE_PORT_NUM);
		  		  connect_to_server(&stream_socket_fd, BOARD_IP_ADDRESS, BOARD_BASE_PORT_NUM+1, SERVER_IP_ADDRESS, SERVER_BASE_PORT_NUM+1);
		  		  break;
		  	  case STREAM_ONLY:
		  		  connect_to_server(&stream_socket_fd, BOARD_IP_ADDRESS, BOARD_BASE_PORT_NUM, SERVER_IP_ADDRESS, SERVER_BASE_PORT_NUM);
		  		  break;
		  	  case CONTROL_ONLY:
		  		  connect_to_server(&stream_socket_fd, BOARD_IP_ADDRESS, BOARD_BASE_PORT_NUM+1, SERVER_IP_ADDRESS, SERVER_BASE_PORT_NUM+1);
		  		  break;
		  	  case FULLY_CONNECTED:
		  	  {

		  	  }
		  }
  }

  int recv_data;
  char data_buffer[BUFF_SIZE];
  /* Creates an TCP socket (SOCK_STREAM)
   * with Internet Protocol Family (PF_INET).
   * Protocol family and Address family related.
   * For example PF_INET Protocol Family and 
   * AF_INET family are coupled.
   */


  int chunkSpaceLeft = 0;
  chunk_d_t **pChunk = NULL;
  int i = 0;
  int dataToWrite = 0;
  int recv_data_idx;
  int chunk_space_idx = 0;
//  while (bufferPool_d_acquire(pThis->pBuffP, pChunk, BUFF_SIZE) != 1) {
//	  printf("vac = %ld\n",((*(u32 *)(FIFO_BASE_ADDR + FIFO_TX_VAC))&0xFFF));
//      printf("waiting %d \n", i++);
//      vTaskDelay( 1 );
//      //printf("No free buffer pool samples available\n");
//      //return -1;
//  }
  while (1) {
//    if (chunk_space_idx == BUFF_SIZE - 1) {
//      xQueueSend(pThis->queue, pChunk, portMAX_DELAY);
//      while (bufferPool_d_acquire(pThis->pBuffP, pChunk, BUFF_SIZE) != 1) {
//    	printf("vac = %ld\n",((*(u32 *)(FIFO_BASE_ADDR + FIFO_TX_VAC))&0xFFF));
//    	printf("waiting %d \n", i++);
//    	vTaskDelay( 1 );
//        //printf("No free buffer pool samples available\n");
//        //return -1;
//      }
//      chunk_space_idx = 0;
//    }
//    if (recv_data_idx == recv_data - 1) {
      recv_data = recv(stream_socket_fd, data_buffer, sizeof(data_buffer), 0);
      // TODO: Check recv is blocking
      // TODO: check that recv data always returns 0 or greater
      if(recv_data < 0) {
    	  printf("recv failed \n");
    	  close(stream_socket_fd);
    	  exit(2);
      }
//      printf("received data: %d\n",recv_data);
//      printf("\n",recv_data);

  	int samplesCount = recv_data/2;


  	int i = 0;
  	for ( ;i<samplesCount;i++)
  	{
        while (((*(u32 *)(FIFO_BASE_ADDR + FIFO_TX_VAC))&0xFFF) <= 0)
        {
        }
//  		printf("unshifted value: %d\n", *(((uint16_t*)data_buffer)+i));
        int32_t tmp  = *(((int16_t*)data_buffer)+i);
//  		printf("unshifted value: %d\n", tmp);
  		int32_t codecFormatSample = ((int32_t)*(((int16_t*)data_buffer)+i)) << 16;
//  		printf("%d,", codecFormatSample);
  		*(uint32_t *) (FIFO_BASE_ADDR + FIFO_TX_DATA) = codecFormatSample;
  		*(uint32_t *) (FIFO_BASE_ADDR + FIFO_TX_LENGTH) = 0x00000004; //data length is 4
  	}



      recv_data_idx = 0;
//    }
//    dataToWrite = (recv_data < chunkSpaceLeft)
//      ? recv_data : chunkSpaceLeft;
//    memcpy(&(*pChunk)->u08_buff + chunk_space_idx,
//	   data_buffer + recv_data_idx, dataToWrite);
//    recv_data_idx += dataToWrite;
//    chunk_space_idx += dataToWrite;
  }
}

