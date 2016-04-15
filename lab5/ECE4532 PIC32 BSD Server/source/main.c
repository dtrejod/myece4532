
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
#define LENP 3
#define LENM 7
#define PROBERR 0.1

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
#pragma pack(0)// turn packing off

void DelayMsec(unsigned int msec);
void generateAlphabet(struct myDataPacket *tbfrData, int tlen) ;

int main()
{
    // Initialize Sockets and IP address containers
    SOCKET serverSock, clientSock = INVALID_SOCKET;
    IP_ADDR curr_ip, ip;

    // loop variable
    int i = 0;
    int temp; 
    
    // Initialize buffer length variables
    int tlen, rlen;
    
    uint8_t recvP = 0;
    uint8_t transP, transM, recvM;
    uint8_t sentSeq[LENM], recvSeq[LENM];
    
    // Initialize the buffers for server
    struct myDataPacket *rbfrData;
    struct myACK rbfrAck;
    char rbfrRaw[256];
    
    // Clear rbfrRaw
    memset(rbfrRaw, 0, 256);

    // Initialize the buffers for client
    // We create our transmission data
    struct myDataPacket tbfrData[MSGLEN];
    struct myACK *tbfrAck;

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

    // We create our transmission data
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
                // If the received message first byte is '02' it signifies
                // a start of message
                if (rbfrRaw[0] == 2) 
                {
                    // Check to see if message begins with
                    // '0271' signifying message is a global reset
                    // We use this as a signal to start the lab
                    // experiment. 
                    if (rbfrRaw[1] == 71) 
                    {                        
                        // Set sequence number P to zero and send LENP 
                        // packets
                        for(transP=0; transP < LENP; transP++)
                        {
                            transM = transP;
                            tbfrData[transP].sequence = transM;
                            mPORTDClearBits(BIT_0);
                            mPORTDSetBits(BIT_2);   // LED3=1
                            send(clientSock, &tbfrData[transP], 
                                sizeof(struct myDataPacket), 0);
                            mPORTDClearBits(BIT_2); // LED3=0 
                        }
                    }
                }
                // If not prefixed we say client is sending back 
                // we need to parse to determine if message is an ACK or
                // the received data
                else
                {
                    // Check what time of message was revived based on its
                    // size
                    // First check if revived is an myACK
                    if (rlen%sizeof(struct myACK)==0)
                    {
                        // Parse the receive buffer for myACK until end of
                        // buffer
                        while (sizeof(struct myAck)*i < rlen)
                        {
                            // Convert the received data into a myAck struct
                            tbfrAck = (struct myACK *) rbfrRaw+i;
                            i++;
                            // Check sequence number and store it into array
                            recvP++;
                        }
                    }                    
                    // Check if recived is an myDataPacket
                    else if (rlen%sizeof(struct myDataPacket)==0)
                    {
                        // Parse the receive buffer until end of buffer
                        while (sizeof(struct myDataPacket)*i < rlen)
                        {
                            // Convert the received data into a dataPacket 
                            // struct
                            rbfrData = (struct myDataPacket *) rbfrRaw+i;
                            i++;
                            // Check sequence number and store it into array
                            recvP++;
                        }
                        // Set sequence number P to zero and send LENP packets
                        for(i=0; i < recvP; i++)
                        {
                           rbfrAck.sequence = i;
                           rbfrAck.ackChar = 0x06;
                           mPORTDClearBits(BIT_0);
                           mPORTDSetBits(BIT_2);   // LED3=1
                           send(clientSock, &rbfrAck, 
                               sizeof(struct myACK), 0);
                           mPORTDClearBits(BIT_2); // LED3=0 
                        }
                        recvP = 0;
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

