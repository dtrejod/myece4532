
//	PIC32 Server - Microchip BSD stack socket API
//	MPLAB X C32 Compiler     PIC32MX795F512L
//      Microchip DM320004 Ethernet Starter Board
//
// ECE4532 - Lab 5 - Stop and Wait Protocol
// Created on 20160414
// Author: Devin Trejo
//	main.c

//      Starter Board Resources:
//		LED1 (RED)	RD0
//		LED2 (YELLOW)	RD1
//		LED3 (GREEN)	RD2
//		SW1		RD6
//		SW2		RD7
//		SW3		RD13

//	Control Messages
//		02 start of message
//		03 end of message


#include <string.h>
#include <time.h>
#include <plib.h>		// PIC32 Peripheral library functions and macros
#include "tcpip_bsd_config.h"	// in \source
#include <TCPIP-BSD\tcpip_bsd.h>

#include "hardware_profile.h"
#include "system_services.h"
#include "display_services.h"

#include "mstimer.h"

#define PC_SERVER_IP_ADDR "192.168.2.105"  // check ipconfig for IP address
#define SYS_FREQ (80000000)

// Project specific constants
#define MSGLEN 26
#define DATALEN 16
#define LENM 15
#define FRAMEDELAY 3
#define PROBSENTERR 0.5
#define PROBACKERR 0.0

#define TRANSMISSIONDELAY 100 // Time to wait between data transmissions
#define ACKTIMEOUT 1000 // In MSEC. Time to wait before 

// We create structs for our message format
// For explanation of pragma see:
// http://stackoverflow.com/questions/1577161/passing-a-structure-through-sockets-in-c
#pragma pack(1)

// ACK Struct
typedef struct myACK
{
    uint8_t sequence;
    char ackChar;
} myACK;

// WARNING IF myDataPacket length and myACKs are multiples of one another,
// the packet detection algo will fail. 
typedef struct myDataPacket 
{
    uint8_t sequence;
    char data[DATALEN];
} myDataPacket;
#pragma pack(0) // turn packing off

typedef struct Queue
{
        int capacity;
        int size;
        int front;
        int rear;
        int *elements;
} Queue;

void DelayMsec(unsigned int msec);
void generateAlphabet(myDataPacket *tbfrData, int tlen) ;
double randMToN(double M, double N);
Queue * createQueue(int maxElements);
void Dequeue(Queue *Q);
int front(Queue *Q);
int rear(Queue *Q);
void Enqueue(Queue *Q, int element);
void clearQueue(Queue *Q);

int main()
{
    // Initialize Sockets and IP address containers
    SOCKET serverSock, clientSock = INVALID_SOCKET;
    IP_ADDR curr_ip, ip;
    
    // Delay interaction counter
    int ackDelayCount = 0;
    int transDelayCount = 0;
    
    // Initialize buffer length variables
    int tlen, rlen;
    
    // Initialize the buffers for server
    myDataPacket *rbfrData;
    myACK rbfrAck;
    char rbfrRaw[256] = {0};
    Queue *rbfrDataQueue = createQueue(FRAMEDELAY);
    uint8_t rbfrSeqTracker = 0;

    // Initialize the buffers for client
    myDataPacket tbfrData[MSGLEN];
    myACK *tbfrAck;
    myDataPacket tbfr;
    Queue *tbfrAckQueue = createQueue(LENM);
    uint8_t tbfrSeqTracker = 0;
    uint8_t testStarted = 0;
    
    // Message progress (expirment) trackers
    int msgSent = 0;
    int endMsg = 0;

    // Socket struct descriptor
    struct sockaddr_in addr;
    int addrlen = sizeof (struct sockaddr_in);

    // System clock containers
    unsigned int sys_clk, pb_clk;

    // Initialize LED Variables:
    // Setup the LEDs on the PIC32 board
    // RD0, RD1 and RD2 as outputs
    mPORTDSetPinsDigitalOut(BIT_0 | BIT_1 | BIT_2);
    mPORTDClearBits(BIT_0 | BIT_1 | BIT_2); // Clear previous LED status.

    // Setup the switches on the PIC32 board as inputs
    mPORTDSetPinsDigitalIn(BIT_6 | BIT_7 | BIT_13); // RD6, RD7, RD13 as inputs

    // Setup the system clock to use CPU frequency
    sys_clk = GetSystemClock();
    pb_clk = SYSTEMConfigWaitStatesAndPB(sys_clk);

    // interrupts enabled
    INTEnableSystemMultiVectoredInt();

    // system clock enabled
    SystemTickInit(sys_clk, TICKS_PER_SECOND);

    // Initialize TCP/IP
    //
    TCPIPSetDefaultAddr(DEFAULT_IP_ADDR, DEFAULT_IP_MASK, DEFAULT_IP_GATEWAY,
            DEFAULT_MAC_ADDR);

    if (!TCPIPInit(sys_clk)) return -1;
    DHCPInit();

    // Port to bind socket to
    addr.sin_port = 6653;
    addr.sin_addr.S_un.S_addr = IP_ADDR_ANY;

    // Initialize TCP server socket
    if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) ==
            SOCKET_ERROR) return -1;

    // Ensure we bound to the socket. End Program if bind fails
    if (bind(serverSock, (struct sockaddr*) &addr, addrlen) ==
            SOCKET_ERROR)
        return -1;

    // Listen to up to five clients on server socket
    listen(serverSock, 5);

    // We create our transmission data using the alphabet. 26 packets total
    tlen = MSGLEN;
    generateAlphabet(tbfrData, tlen);
    
    // Loop forever
    while (1) 
    {
        // Refresh TCIP and DHCP
        TCPIPProcess();
        DHCPTask();

        // set the machines IP address and save to variable
        ip.Val = TCPIPGetIPAddr();

        // DHCP server change IP address?
        if (curr_ip.Val != ip.Val) curr_ip.Val = ip.Val;

        // TCP Server Code
        if (clientSock == INVALID_SOCKET) 
        {
            // Start listening for incoming connections
            clientSock = accept(serverSock, 
                (struct sockaddr*) &addr, &addrlen);

            // Upon connection to a client blink LEDS.
            if (clientSock != INVALID_SOCKET) 
            {
                setsockopt(clientSock, SOL_SOCKET, TCP_NODELAY, 
                    (char*)&tlen, sizeof(int));
                mPORTDSetBits(BIT_0); // LED1=1
                DelayMsec(50);
                mPORTDClearBits(BIT_0); // LED1=0
                mPORTDSetBits(BIT_1); // LED2=1
                DelayMsec(50);
                mPORTDClearBits(BIT_1); // LED2=0
                mPORTDSetBits(BIT_2); // LED3=1
                DelayMsec(50);
                mPORTDClearBits(BIT_2); // LED3=0
            }
        } 
        else 
        {
            // We are connected to a client already. We start
            // by receiving the message being sent by the client
            rlen = recvfrom(clientSock, rbfrRaw, sizeof (rbfrRaw), 0, NULL,
                    NULL);

            // Check to see if socket is still alive
            if (rlen > 0) 
            {
                // Check to see if message begins with
                // '0271' signifying message is a global reset
                // We use this as a signal to start the lab
                // experiment. 
                if ((testStarted == 0) && (rbfrRaw[0] == 02) && 
                        (rbfrRaw[1] == 71))
                {                        
                    // Reset Sequence Number
                    tbfrSeqTracker = 0;

                    // Reset total msg sent counter
                    msgSent = 0;
                    endMsg=0;
                    testStarted = 1;
                    
                    // reset delay counts
                    transDelayCount = 0;
                    ackDelayCount = 0;

                    // Check for seq rollover
                    if(tbfrSeqTracker > LENM)
                    {
                        tbfrSeqTracker = 0;
                    }

                    // Populate sequence number
                    tbfrData[msgSent].sequence = 
                        tbfrSeqTracker++;

                    // Copy tbfrData over to tbfr and keep track
                    // of how much msg has been sent thus far
                    tbfr = tbfrData[msgSent++];

                    mPORTDClearBits(BIT_0);
                    mPORTDSetBits(BIT_2);   // LED3=1
                    send(clientSock, &tbfr, sizeof(myDataPacket), 0);
                    mPORTDClearBits(BIT_2); // LED3=0
                    DelayMsec(100);

                    // Mark frame as sent by queuing up sequence in ACK 
                    // awaiting response.
                    Enqueue(tbfrAckQueue, tbfr.sequence);
                }
                // If not prefixed we say client is sending back 
                // we need to parse to determine if message is an ACK or`
                // the received data
                else if (testStarted==1)
                {
                    // Check what time of message was revived based on its
                    // size
                    // Check if received is an myDataPacket
                    if (rlen%sizeof(myDataPacket)==0)
                    {                       
                        // Convert the received data into a dataPacket 
                        // struct
                        rbfrData = (myDataPacket *) rbfrRaw;

                        // Store sequence in receive Queue if it is 
                        // next in Queue
                        
                        // Check for start of tranmission OR
                        // Check for sequential sequence number OR
                        // Check for sequential sequence rollover
                        if(rbfrSeqTracker == (rbfrData->sequence))
                        {
                            rbfrSeqTracker++;
                            // Check for seq rollover
                            if(rbfrSeqTracker > LENM)
                            {
                                rbfrSeqTracker = 0;
                            }
                            Enqueue(rbfrDataQueue, rbfrData->sequence);
                        }

                        // If FRAMEDELAY Equal to size
                        if(rbfrDataQueue->size == FRAMEDELAY || endMsg == 1)
                        {
                            // Send ACK for Random number of packets
                            if (randMToN(0.0,1.0) >= PROBACKERR)
                            {
                                rbfrAck.sequence = front(rbfrDataQueue);
                                rbfrAck.ackChar = 0x06;
                                mPORTDClearBits(BIT_0);
                                mPORTDSetBits(BIT_2);   // LED3=1
                                send(clientSock, &rbfrAck, 
                                    sizeof(myACK), 0);
                                mPORTDClearBits(BIT_2); // LED3=0
                                DelayMsec(100);
                            }

                            // Remove the sent ACK from receive Q
                            Dequeue(rbfrDataQueue);
                        }
                    }
                    // Check if received is an myACK
                    else if (rlen%sizeof(myACK)==0)
                    {
                        // Convert the received data into a myAck struct
                        tbfrAck = (myACK *) rbfrRaw;
                        
                        // Check if ACK
                        if(tbfrAck->ackChar == 0x06)
                        {
                            if(front(tbfrAckQueue) == (tbfrAck->sequence))
                            {
                                Dequeue(tbfrAckQueue);
                                ackDelayCount = 0;
                            }
                            // Check if end of expirment
                            if (tbfrAckQueue->size == 0 && endMsg == 1)
                            {
                                testStarted = 0;
                            }
                        }
                    }                    
                }
            }
            else if (rlen < 0) 
            {
                // The client has closed the socket so we close as well
                //
                closesocket(clientSock);
                clientSock = SOCKET_ERROR;
            }

            // If rlen is zero length we increment delay count
            else if (testStarted == 1)
            {
                DelayMsec(10);
                ackDelayCount++;
                transDelayCount++;

                // Check if time to send another DataPacket and if 
                // we have more msg to send.
                if (transDelayCount*10 > TRANSMISSIONDELAY && msgSent < MSGLEN)
                {
                    // reset delay counts
                    transDelayCount = 0;

                    // Check for seq rollover
                    if(tbfrSeqTracker > LENM)
                    {
                        tbfrSeqTracker = 0;
                    }

                    // Populate sequence number
                    tbfrData[msgSent].sequence = 
                        tbfrSeqTracker++;

                    // Copy tbfrData over to tbfr and keep track
                    // of how much msg has been sent thus far
                    tbfr = tbfrData[msgSent++];

                    // Send FRAME with random error change
                    if (randMToN(0.0,1.0) >= PROBSENTERR)
                    {
                        mPORTDClearBits(BIT_0);
                        mPORTDSetBits(BIT_2);   // LED3=1
                        send(clientSock, &tbfr, sizeof(myDataPacket), 0);
                        mPORTDClearBits(BIT_2); // LED3=0
                        DelayMsec(100);
                    }

                    // Mark frame as sent by queuing up sequence in ACK 
                    // awaiting response.
                    Enqueue(tbfrAckQueue, tbfr.sequence);
                    
                    // Check to see if we hit FRAMEDELAY Limit. If so we
                    // start the ackDelay Count (or in other words don't reset)
                    if (tbfrAckQueue->size <= FRAMEDELAY)
                    {
                        ackDelayCount = 0;
                    }
                    
                    // Check to see if end of tranmission
                    if (msgSent == MSGLEN) endMsg = 1;
                }
                // Check for ACK timeout
                else if (ackDelayCount*10 > ACKTIMEOUT)
                {
                    // reset delay counts
                    transDelayCount = 0;
                    ackDelayCount = 0;

                    // Reset msgSent tracker
                    msgSent = msgSent - tbfrAckQueue->size;

                    // Clear sent queue
                    clearQueue(tbfrAckQueue);

                    // Copy tbfrData over to tbfr and keep track
                    // of how much msg has been sent thus far
                    tbfr = tbfrData[msgSent++];
                    
                    // Reset seq tracker
                    tbfrSeqTracker = tbfr.sequence+1;
                    
                    // Send FRAME with random error change
                    if (randMToN(0.0,1.0) >= PROBSENTERR)
                    {
                        mPORTDClearBits(BIT_0);
                        mPORTDSetBits(BIT_2);   // LED3=1
                        send(clientSock, &tbfr, sizeof(myDataPacket), 0);
                        mPORTDClearBits(BIT_2); // LED3=0
                        DelayMsec(100);
                    }

                    // Mark frame as sent by queuing up sequence in ACK 
                    // awaiting response.
                    Enqueue(tbfrAckQueue, tbfr.sequence);
                }
            }
        }   
    }
}

// Function : DelayMsec( )
// 
// Delays the program by specified millisecond passed
void DelayMsec(unsigned int msec) 
{
    unsigned int tWait, tStart;
    tWait = (SYS_FREQ / 2000) * msec;
    tStart = ReadCoreTimer();
    while ((ReadCoreTimer() - tStart) < tWait);
}

void generateAlphabet(myDataPacket *tbfrData, int tlen) 
{
    // Loop tracker
    int i, j;

    for(i=0; i < tlen; i++)
    {
        for(j=0; j < DATALEN; j++)
        {
            // We start populating data with ascii A
            tbfrData[i].data[j] = 0x41 + i;
        }
    }
}

double randMToN(double M, double N)
{
    return M + (rand() / ( RAND_MAX / (N-M) ) ) ;  
}

// Queue Data Structure
// Source From:
// http://www.thelearningpoint.net/computer-science/data-structures-queues--with-c-program-source-code

// crateQueue function takes argument the maximum number of elements the
// Queue can hold, creates
// a Queue according to it and returns a pointer to the Queue.
Queue * createQueue(int maxElements)
{
    // Create a Queue
    Queue *Q;
    Q = (Queue *)malloc(sizeof(Queue));
    // Initialize its properties
    Q->elements = (int *)malloc(sizeof(int)*maxElements);
    Q->size = 0;
    Q->capacity = maxElements;
    Q->front = 0;
    Q->rear = -1;
    /* Return the pointer */
    return Q;
}

void Dequeue(Queue *Q)
{
    // If Queue size is zero then it is empty. So we cannot pop
    if(Q->size==0)
    {
        printf("Queue is Empty\n");
        return;
    }
    // Removing an element is equivalent to incrementing index of front 
    // by one
    else
    {
            Q->size--;
            Q->front++;
            // As we fill elements in circular fashion
            if(Q->front==Q->capacity)
            {
                Q->front=0;
            }
    }
    return;
}

int front(Queue *Q)
{
    if(Q->size==0)
    {
        printf("Queue is Empty\n");
        exit(0);
    }
    // Return the element which is at the front
    return Q->elements[Q->front];
}

int rear(Queue *Q)
{
    if(Q->size==0)
    {
        printf("Queue is Empty\n");
        exit(0);
    }
    // Return the element which is at the front
    return Q->elements[Q->rear];
}

void Enqueue(Queue *Q, int element)
{
    // If the Queue is full, we cannot push an element into it as 
    // there is no space for it.
    if(Q->size == Q->capacity)
    {
        printf("Queue is Full\n");
    }
    else
    {
            Q->size++;
            Q->rear = Q->rear + 1;
            // As we fill the queue in circular fashion
            if(Q->rear == Q->capacity)
            {
                Q->rear = 0;
            }
            // Insert the element in its rear side
            Q->elements[Q->rear] = element;
    }
    return;
}

void clearQueue(Queue *Q)
{
    if(Q->size==0)
    {
        printf("Queue is Empty\n");
        exit(0);
    }
    else
    {
        Q->front = 0;
        Q->rear = -1;
        Q->size = 0;
    }
    return;
}