/**
 *@file audioTx.c
 *
 *@brief
 *  - receive audio samples from DMA
 *
 * Target:   TLL6527v1-0      
 * Compiler: VDSP++     Output format: VDSP++ "*.dxe"
 *
 * @author  Gunar Schirner
 *          Rohan Kangralkar
 * @date 03/15/2009
 *
 * LastChange:
 * $Id: audioTx.c 814 2013-03-12 03:59:36Z ovaskevi $
 *
 *******************************************************************************/
#include "audioTx.h"
#include "bufferPool_d.h"

int audioTx_init(audioTx_t *pThis, bufferPool_d_t *pBuffP)
{
    // paramter checking
    if ( NULL == pThis || NULL == pBuffP) {
        printf("[ATX]: Failed init\r\n");
        return -1;
    }
    
    // store pointer to buffer pool for later access     
    pThis->pBuffP       = pBuffP;

    pThis->pPending     = NULL; // nothing pending
    pThis->running      = 0;    // DMA turned off by default
    
    // init queue 
    pThis->queue = xQueueCreate(AUDIOTX_QUEUE_DEPTH, sizeof(chunk_d_t*));

    // note ISR registration done in _start
    
    printf("[ARX]: TX init complete\r\n");
    
    return 1;
}

int audioTx_put(audioTx_t *pThis, chunk_d_t *pChunk)
{
    
    if ( NULL == pThis || NULL == pChunk ) {
        printf("[TX]: Failed to put\r\n");
        return -1;
    }


	int samplesCount = (pChunk->bytesUsed)/4;
    while (((*(u32 *)(FIFO_BASE_ADDR + FIFO_TX_VAC))&0xFFF) < samplesCount)
    {
    }
    printf("\n");
	int i = 0;
	for ( ;i<samplesCount;i++)
	{
		int32_t codecFormatSample = pChunk->u32_buff[i] << 16;
		printf("%d,", codecFormatSample);
		*(uint32_t *) (FIFO_BASE_ADDR + FIFO_TX_DATA) = codecFormatSample;
		*(uint32_t *) (FIFO_BASE_ADDR + FIFO_TX_LENGTH) = 0x00000004; //data length is 4
	}


    bufferPool_d_release(pThis->pBuffP, pChunk);
    return 0;
}
