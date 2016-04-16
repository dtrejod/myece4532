
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
#define LENP 1
#define LENM 10
#define PROBERR 0.1
#define ACKTIMEOUT 5000 // InMSEC

// We create structs for our message format
// For explanation of pragma see:
// http://stackoverflow.com/questions/1577161/passing-a-structure-through-sockets-in-c
#pragma pack(1)

// ACK Struct
struct myACK
{
    uint8_t sequence;
    char ackChar;
};

// WARNING IF myDataPacket length and myACKs are multiples of one another,
// the packet detection algo will fail. 
struct myDataPacket 
{
    uint8_t sequence;
    char data[DATALEN];
};
#pragma pack(0) // turn packing off

void DelayMsec(unsigned int msec);
void generateAlphabet(struct myDataPacket *tbfrData, int tlen) ;
double randMToN(double M, double N);
void shuffle(uint8_t *array, size_t n);

int main()
{
    // Initialize Sockets and IP address containers
    SOCKET serverSock, clientSock = INVALID_SOCKET;
    IP_ADDR curr_ip, ip;

    // loop variable
    int i,j;
    int temp; 

    // Delay interaction counter
    int delayCount = 0;    
    
    // Initialize buffer length variables
    int tlen, rlen;
    
    // Initialize the buffers for server
    struct myDataPacket *rbfrData;
    struct myACK rbfrAck[LENP];
    char rbfrRaw[256] = {0};
    uint8_t rbfrDataTracker[LENP] = {NULL};
    uint8_t rbfrDataTrackerI = 0;

    // Initialize the buffers for client
    struct myDataPacket tbfrData[MSGLEN];
    struct myACK *tbfrAck;
    struct myDataPacket tbfr[LENP];
    uint8_t tbfrDataTracker[LENP+LENM] = {NULL};
    uint8_t tbfrDataTrackerI = 0;
    uint8_t tbfrAckTracker[LENP+LENM] = {NULL};
    int tbfrAckTrackerI = 0;
    uint8_t tbfrSeqTracker = 0;
    uint8_t selectiveRepeat = 0;
    uint8_t flag;
    uint8_t testStarted = 0;
    int msgSent = 0;
    int offset;
    int count = 0;
    
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
                // Reset Delay Count
                delayCount = 0;
                // Check to see if message begins with
                // '0271' signifying message is a global reset
                // We use this as a signal to start the lab
                // experiment. 
                if ((testStarted == 0) && (rbfrRaw[0] == 02) && 
                        (rbfrRaw[1] == 71))
                {                        
                    // Reset Frame Sent Variables
                    tbfrDataTrackerI = 0;
                    tbfrAckTrackerI = 0;

                    // Reset Sequence Number
                    tbfrSeqTracker = 1;

                    // Reset total msg sent counter
                    msgSent = 0;
                    count = 0;
                    testStarted = 1;

                    for(tbfrDataTrackerI=0; tbfrDataTrackerI < LENP; 
                        tbfrDataTrackerI++)
                    {
                        // Check for seq rollover
                        if(tbfrSeqTracker >= LENM)
                        {
                            tbfrSeqTracker = 1;
                        }

                        // Adjust offset
                        if(tbfrDataTrackerI==0) offset=msgSent;
                        // Populate sequence number
                        tbfrData[tbfrDataTrackerI+offset].sequence = 
                            tbfrSeqTracker++;

                        // We keep track of the seq numbers we do send
                        tbfrDataTracker[tbfrDataTrackerI] = 
                            tbfrData[tbfrDataTrackerI+offset].sequence;

                        // Copy tbfr over to 
                        tbfr[tbfrDataTrackerI] = 
                            tbfrData[tbfrDataTrackerI+offset];

                        // Keep track of how much of the msg has been
                        // sent
                        msgSent++;

                        if (msgSent > MSGLEN)
                        {
                            break;
                        } 
                    }
                    mPORTDClearBits(BIT_0);
                    mPORTDSetBits(BIT_2);   // LED3=1
                    send(clientSock, tbfr, 
                        sizeof(struct myDataPacket)*tbfrDataTrackerI, 0);
                    mPORTDClearBits(BIT_2); // LED3=0
                    DelayMsec(100);
                }
                // If not prefixed we say client is sending back 
                // we need to parse to determine if message is an ACK or
                // the received data
                else if (testStarted==1)
                {
                    // Check what time of message was revived based on its
                    // size
                    // Check if received is an myDataPacket
                    if (rlen%sizeof(struct myDataPacket)==0)
                    {
                        i=0;
                        // Reset data packets received
                        rbfrDataTrackerI = 0;
                        
                        // Parse the receive buffer until end of buffer
                        while (sizeof(struct myDataPacket)*i < rlen)
                        {
                            // Convert the received data into a dataPacket 
                            // struct
                            rbfrData = (struct myDataPacket *) rbfrRaw+(i++);
                            // Retrieve sequence number and store
                            rbfrDataTracker[rbfrDataTrackerI++] = 
                                rbfrData->sequence;
                        }
                        // Shuffle order of rbfrDataTracker ACKs we will send 
                        // back
                        shuffle(rbfrDataTracker, rbfrDataTrackerI);
                        
                        j=0;
                        // Set sequence number P to zero and send LENP packets
                        for(i=0; i < rbfrDataTrackerI; i++)
                        {
                            // Send ACK for Random number of packets
                            if (randMToN(0.0,1.0) >= PROBERR)
                            {
                                rbfrAck[j].sequence = rbfrDataTracker[i];
                                rbfrAck[j].ackChar = 0x06;
                                j++;
                            }
                        }
                        mPORTDClearBits(BIT_0);
                        mPORTDSetBits(BIT_2);   // LED3=1
                        send(clientSock, rbfrAck, 
                            sizeof(struct myACK)*j, 0);
                        mPORTDClearBits(BIT_2); // LED3=0
                        DelayMsec(100);
                    }
                    // Check if received is an myACK
                    else if (rlen%sizeof(struct myACK)==0)
                    {
                        // Randomize the 
                        // Parse the receive buffer for myACK until end of
                        // buffer
                        i=0;
                        while (sizeof(struct myACK)*i < rlen)
                        {
                            // Convert the received data into a myAck struct
                            tbfrAck = (struct myACK *) rbfrRaw+(i++);
                            
                            // Retrieve sequence number and store
                            tbfrAckTracker[tbfrAckTrackerI++] = 
                                tbfrAck->sequence;
                        }
                        // Check if we recieved all the frames we orignally
                        // sent. If yes transfer the next M frames
                        if (tbfrAckTrackerI >= tbfrDataTrackerI)
                        {
                            if (msgSent > MSGLEN)
                            {
                                testStarted=0;
                            }
                            else 
                            {
                                // Check if there are more frames to send
                                // Reset Frame Sent Variables
                                tbfrAckTrackerI = 0;

                                for(tbfrDataTrackerI=0; tbfrDataTrackerI < LENP; 
                                    tbfrDataTrackerI++)
                                {
                                    // Check for seq rollover
                                    if(tbfrSeqTracker >= LENM)
                                    {
                                        tbfrSeqTracker = 1;
                                    }

                                    // Adjust offset
                                    if(tbfrDataTrackerI==0) offset=msgSent;
                                    // Populate sequence number
                                    tbfrData[tbfrDataTrackerI+offset].sequence = 
                                        tbfrSeqTracker++;

                                    // We keep track of the seq numbers we do send
                                    tbfrDataTracker[tbfrDataTrackerI] = 
                                        tbfrData[tbfrDataTrackerI+offset].sequence;

                                    // Copy tbfr over to 
                                    tbfr[tbfrDataTrackerI] = 
                                        tbfrData[tbfrDataTrackerI+offset];

                                    // Keep track of how much of the msg has been
                                    // sent
                                    msgSent++;

                                    if (msgSent > MSGLEN)
                                    {
                                        break;
                                    }
                                }
                                mPORTDClearBits(BIT_0);
                                mPORTDSetBits(BIT_2);   // LED3=1
                                send(clientSock, tbfr, 
                                    sizeof(struct myDataPacket)*tbfrDataTrackerI, 0);
                                mPORTDClearBits(BIT_2); // LED3=0

                                DelayMsec(100);
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
                delayCount++;
                
                // Check for ACK timeout
                if (delayCount*10 > ACKTIMEOUT)
                {
                    // Retransmit any packets we haven't received ACKs back
                    // for yet
                    delayCount = 0;

                    // Reset selective repeat
                    selectiveRepeat = 0;

                    // Set sequence number P to zero and send LENP 
                    // packets
                    for(i=0; i < tbfrDataTrackerI; i++)
                    {
                        flag = 0;
                        for(j=0; j< tbfrAckTrackerI; j++)
                        {
                            if(tbfrDataTracker[i]==tbfrAckTracker[j])
                            {
                                flag = 1;
                                break;
                            }
                        }
                        if (flag == 0)
                        {
                            tbfr[selectiveRepeat++] = tbfr[i];
                        }
                    }
                    mPORTDClearBits(BIT_0);
                    mPORTDSetBits(BIT_2);   // LED3=1
                    send(clientSock, tbfr, 
                        sizeof(struct myDataPacket)*selectiveRepeat, 0);
                    mPORTDClearBits(BIT_2); // LED3=0 
                    DelayMsec(100);
                    count++;
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

void generateAlphabet(struct myDataPacket *tbfrData, int tlen) 
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

void shuffle(uint8_t *array, size_t n)
{
    if (n > 1) 
    {
        size_t i;
        for (i = 0; i < n - 1; i++) 
        {
          size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          int t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}