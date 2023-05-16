#include "packetdriver.h"
#include "BoundedBuffer.h"
#include "freepacketdescriptorstore__full.h"
#include <pthread.h>
#include <stdio.h>

NetworkDevice *n = NULL;
pthread_t receive = 0;
pthread_t send = 0;
FreePacketDescriptorStore *fpds = NULL;
BoundedBuffer *buffer = NULL;


/* thread helpers */
static void *receive_thread(void *arg)
{
    PacketDescriptor *pd = NULL;
    while (1)
    {
        fpds->blockingGet(fpds, &pd);
        initPD(pd);
        n->registerPD(n, pd);
        n->awaitIncomingPacket(n);
        printf("[RecThread> received packet for pid %d\n", getPID(pd));
        buffer->blockingWrite(buffer, pd);
        fpds->blockingPut(fpds, pd);
    }
    return NULL;
}

static void *send_thread(void *arg)
{
    PacketDescriptor *pd = NULL;
    while (1)
    {
        /* watch for packet descriptor to be sent */
        for (int i = 0; i < 3; i++)
        {
            printf("Packet from PID %d send attempt %d\n", getPID(pd), i + 1);
            if (n->sendPacket(n, pd))
            {
                printf("Packet sent from PID %d\n", getPID(pd));
                return;
            }
        }
        printf("Packet send from PID %d failed\n", getPID(pd));
    }
}

/* cool init thingy */
void init_packet_driver(NetworkDevice               *nd, 
                        void                        *mem_start, 
                        unsigned long               mem_length,
                        FreePacketDescriptorStore **fpds_ptr)
{
    *fpds_ptr = FreePacketDescriptorStore_create(mem_start, mem_length);
    fpds = *fpds_ptr;
    n = nd;
    buffer = BoundedBuffer_create(MAX_PID+1);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&receive, &attr, &receive_thread, NULL);
    pthread_create(&send, &attr, &send_thread, NULL);
}


/* sending stuff */
void blocking_send_packet(PacketDescriptor *pd)
{ printf("[BlockSend> blocking_send from %d to %p\n", getPID(pd), getDestination(pd)); }

int  nonblocking_send_packet(PacketDescriptor *pd)
{ printf("[NonblockSend> nonblocking_send from %d to %p\n", getPID(pd), getDestination(pd));return 0; }


/* reveiving stuff */
void blocking_get_packet(PacketDescriptor **pd, PID pid)
{ printf("[BlockGet> blocking_get from %d\n", pid); *pd = NULL; }

int  nonblocking_get_packet(PacketDescriptor **pd, PID pid)
{ printf("[NonblockGet> nonblocking_get from %d\n", pid); *pd = NULL; return 0; }