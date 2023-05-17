#include "packetdriver.h"
#include "BoundedBuffer.h"
#include "freepacketdescriptorstore__full.h"
#include <pthread.h>
#include <stdio.h>

#define UNUSED __attribute__((unused))
#define BB_SIZE 1

NetworkDevice *n = NULL;
pthread_t receive = 0;
pthread_t send = 0;
FreePacketDescriptorStore *fpds = NULL;
BoundedBuffer *in_buffer[MAX_PID + 1];
BoundedBuffer *out_buffer[MAX_PID + 1];

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* thread helpers */
void wait_for_signal()
{
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);
        pthread_mutex_unlock(&mutex);
}

void send_signal()
{
        pthread_mutex_lock(&mutex);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
}

/* thread functions */
static void *receive_thread(UNUSED void *arg)
{
        PacketDescriptor *pd = NULL;
        while (1)
        {
                fpds->blockingGet(fpds, &pd);
                initPD(pd);
                n->registerPD(n, pd);
                n->awaitIncomingPacket(n);
                pid_t pid = getPID(pd);

                BoundedBuffer *bb = in_buffer[pid];
                bb->blockingWrite(bb, pd);

                fpds->blockingPut(fpds, pd);
        }
        return NULL;
}

static void *send_thread(UNUSED void *arg)
{
        PacketDescriptor *pd = NULL;
        int j = 0;

        while (1)
        {
                wait_for_signal();
                for (int i = 1; i < MAX_PID + 1; i++)
                {
                        BoundedBuffer *bb = out_buffer[i];

                        if (!bb->nonblockingRead(bb, (void **)&pd))
                                continue;

                        for (j = 0; j < 3; j++)
                                if (n->sendPacket(n, pd))
                                        break;
                }
        }

        return NULL;
}

/* cool init thingy */
void init_packet_driver(NetworkDevice *nd,
                        void *mem_start,
                        unsigned long mem_length,
                        FreePacketDescriptorStore **fpds_ptr)
{
        *fpds_ptr = FreePacketDescriptorStore_create(mem_start, mem_length);
        fpds = *fpds_ptr;
        n = nd;
        for (int i = 0; i < MAX_PID + 1; i++)
        {
                in_buffer[i] = BoundedBuffer_create(BB_SIZE);
                out_buffer[i] = BoundedBuffer_create(BB_SIZE);
        }
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&receive, &attr, &receive_thread, NULL);
        pthread_create(&send, &attr, &send_thread, NULL);
}

/* sending stuff */
void blocking_send_packet(PacketDescriptor *pd)
{
        BoundedBuffer *bb = out_buffer[getPID(pd)];
        bb->blockingWrite(bb, pd);
        send_signal();
}

int nonblocking_send_packet(PacketDescriptor *pd)
{
        BoundedBuffer *bb = out_buffer[getPID(pd)];

        int yeah = bb->nonblockingWrite(bb, pd);

        if (yeah)
                send_signal();

        return yeah;
}

/* reveiving stuff */
void blocking_get_packet(PacketDescriptor **pd, PID pid)
{
        BoundedBuffer *bb = in_buffer[pid];
        bb->blockingRead(bb, (void **)pd);
}

int nonblocking_get_packet(PacketDescriptor **pd, PID pid)
{
        BoundedBuffer *bb = in_buffer[pid];
        return bb->nonblockingRead(bb, (void **)pd);
}