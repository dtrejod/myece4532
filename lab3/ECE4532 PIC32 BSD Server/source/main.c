
//	PIC32 Server - Microchip BSD stack socket API
//	MPLAB X C32 Compiler     PIC32MX795F512L
//      Microchip DM320004 Ethernet Starter Board
//
// ECE4532 - Lab 3 - Even Parity Check 
// Created on 20160308
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

#define bufferRows 5
#define bufferCols 6


void DelayMsec(unsigned int msec);
void evenParityEncoder(const char *myStr, char *transmitBuffer, int tlen);
int hasEvenParity(char x);
int hasPartialEvenParity(char x, int colCount);
int hasEvenParityArray(char *rowBuffer, int rowBufferLen);
void setColParity(char *rowBuffer, char *transmitBuffer, int colIndex);
void evenParityDecoder(char *recieveBuffer, int rlen);

int main()
{
    // Initialize Sockets and IP address containers
    //
    SOCKET serverSock, clientSock = INVALID_SOCKET;
    IP_ADDR curr_ip, ip;

    // Initialize buffer length variables
    //
    int tlen, rlen;

    // Initialize the Send/Recv buffers
    //
    char rbfr[256];

    // Socket struct descriptor
    //
    struct sockaddr_in addr;
    int addrlen = sizeof (struct sockaddr_in);

    // System clock containers
    //
    unsigned int sys_clk, pb_clk;

    // Initialize LED Variables:
    // Setup the LEDs on the PIC32 board
    // RD0, RD1 and RD2 as outputs
    //
    mPORTDSetPinsDigitalOut(BIT_0 | BIT_1 | BIT_2);
    mPORTDClearBits(BIT_0 | BIT_1 | BIT_2); // Clear previous LED status.

    // Setup the switches on the PIC32 board as inputs
    //		
    mPORTDSetPinsDigitalIn(BIT_6 | BIT_7 | BIT_13); // RD6, RD7, RD13 as inputs

    // Setup the system clock to use CPU frequency
    //
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
    //
    addr.sin_port = 6653;
    addr.sin_addr.S_un.S_addr = IP_ADDR_ANY;

    // Initialize TCP server socket
    //
    if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) ==
            SOCKET_ERROR) return -1;

    // Ensure we bound to the socket. End Program if bind fails
    //
    if (bind(serverSock, (struct sockaddr*) &addr, addrlen) ==
            SOCKET_ERROR)
        return -1;

    // Listen to up to five clients on server socket
    //
    listen(serverSock, 5);

    // Create our input transmit block
    //
    const char *myStr = "Devin Trejo test string for EE";

    //chartransmitBuffer[bufferRows*(bufferCols+1)];
    //evenParityEncoder(myStr, transmitBuffer);
    tlen = bufferRows*(bufferCols+1);

    //char *transmitBuffer = (char *)malloc(tlen*sizeof(char));
    char transmitBuffer[tlen];
    
    evenParityEncoder(myStr, transmitBuffer, tlen);

    // Loop forever
    //
    while (1) 
    {
        // Refresh TCIP and DHCP
        //
        TCPIPProcess();
        DHCPTask();

        // set the machines IP address and save to variable
        //
        ip.Val = TCPIPGetIPAddr();

        // DHCP server change IP address?
        //
        if (curr_ip.Val != ip.Val) curr_ip.Val = ip.Val;

        // TCP Server Code
        //
        if (clientSock == INVALID_SOCKET) 
        {
            // Start listening for incoming connections
            //
            clientSock = accept(serverSock, (struct sockaddr*) &addr, &addrlen);

            // Upon connection to a client blink LEDS.
            //
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
        } else 
        {
            // We are connected to a client already. We start
            // by receiving the message being sent by the client
            //
            rlen = recvfrom(clientSock, rbfr, sizeof (rbfr), 0, NULL,
                    NULL);

            // Check to see if socket is still alive
            //
            if (rlen > 0) 
            {
                // If the received message first byte is '02' it signifies
                // a start of message
                //
                if (rbfr[0] == 2) 
                {
                    // Check to see if message begins with
                    // '0271' signifying message is a global reset
                    if (rbfr[1] == 71) 
                    {
                        mPORTDSetBits(BIT_0); // LED1=1
                        DelayMsec(50);
                        mPORTDClearBits(BIT_0); // LED1=0
                    }
                    // Check to see if message begins with 
                    // '0284' signifying message is a start of a transfer
                    else if(rbfr[1]==84)
                    {
                        mPORTDClearBits(BIT_0);
                        mPORTDSetBits(BIT_1);   // LED3=1
                        send(clientSock, transmitBuffer, tlen, 0);
                        DelayMsec(50);
                        mPORTDClearBits(BIT_1);	// LED3=0
                    }
                mPORTDClearBits(BIT_0); // LED1=0
                }
                // If not prefixed we say client is sending back our
                // transmits corrupted packet.
                else
                {
                    // receive possible corrupted data
                    evenParityDecoder(rbfr, rlen);

                    //send data back across the socket 
                    // (for viewing in wireshark)
                    mPORTDSetBits(BIT_2);   // LED3=1
                    send(clientSock, rbfr, rlen, 0);
                    mPORTDClearBits(BIT_2);   // LED3=1
                }
                // The client has closed the socket so we close as well
                //
            }
            else if (rlen < 0) 
            {
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

void evenParityDecoder(char *recieveBuffer, int rlen)
{
    int i;
    int j = 0;
    int colError;
    
    // Plus one for the col parity bit character
    char rowBuffer[bufferCols+1];
    
    // First we check column parity chars
    for(i=0; i < rlen; i+=bufferCols+1)
    {
        // create a buffer to hold our row and col parity bit character
        memcpy(rowBuffer, recieveBuffer+i, bufferCols+1);
        
        // check if array of characters has even parity along the column
        colError = hasEvenParityArray(rowBuffer, bufferCols+1);

        // if a column char doesn't have even parity then we know 
        // there is an error
        if (colError >= 0)
        {
            // Set led 1 (red) high
            mPORTDSetBits(BIT_0);
            for(j=0; j < bufferCols; j++)
            {
                if (hasEvenParity(rowBuffer[j])==0)
                {
                    // flip the problem bit
                    rowBuffer[j] ^= (0x01 << colError);

                    // Correct the received data. 
                    recieveBuffer[i+j] = rowBuffer[j];
                    return;
                }
            }
            // We detected an error but is was a in the parity col. We blink 
            // led 1 (red))
            DelayMsec(100);
            mPORTDClearBits(BIT_0);
            DelayMsec(100);
            mPORTDSetBits(BIT_0);
            DelayMsec(100);
            mPORTDClearBits(BIT_0);
        }
    }
}

void evenParityEncoder(const char *myStr, char *transmitBuffer, int tlen) 
{
    int i;
    int j = 0;
    int m = 0;

    // Create last row buffer container
    char rowBuffer[bufferCols];

    for(i=0; i < tlen; i++)
    {
        // Check if we are calculating col parity
        if(i!=0 && (j==6))
        {
            setColParity(rowBuffer, transmitBuffer, i);          
            j = 0;
            continue;
        }
        // Copy one bit shifted string contents into transmit buffer
        transmitBuffer[i] = myStr[m++] << 1;
        
        // See if we need to set row parity bit high
        if (hasEvenParity(transmitBuffer[i])==0)
        {
            transmitBuffer[i] ^= 1;
        }
        rowBuffer[j++] = transmitBuffer[i];
    }
}

int hasEvenParity(char x) 
{
    int i;
    int count = 0;

    // Loop through each bit of the car
    //
    for (i = 0; i < 8; i++) 
    {
        if (x & (0x01 << i)) 
        {
            count++;
        }
    }
    if (count % 2) return 0;
    return 1;
}

int hasPartialEvenParity(char x, int colCount)
{
    int i;
    int count = 0;

    // Loop through each bit of the car
    //
    for (i = 0; i < colCount; i++) 
    {
        if (x & (0x01 << i)) 
        {
            count++;
        }
    }
    if (count % 2) return 0;
    return 1;
}

void setColParity(char *rowBuffer, char *transmitBuffer, int colIndex) 
{
    int i, j;
    int rowBufferLen = strlen(rowBuffer);
    char colBitBuffer;
    
    transmitBuffer[colIndex] = 0;
    
    for (i = 0; i < 8; i++) 
    {
        colBitBuffer = 0;
        // Last Row is reserved for parity
        //
        for (j = 0; j < rowBufferLen; j++) 
        {
            colBitBuffer |= ((rowBuffer[j] & (0x01 << i)) >> i) << j;
        }
        if (hasPartialEvenParity(colBitBuffer, rowBufferLen)==0)
        {
            transmitBuffer[colIndex] |= (1 << i);
        }
    }
}

int hasEvenParityArray(char *rowBuffer, int rowBufferLen)
{
    int i, j;
    char colBitBuffer;
    
    for (i = 0; i < 8; i++) 
    {
        colBitBuffer = 0;
        // Last Row is reserved for parity
        //
        for (j = 0; j < rowBufferLen; j++) 
        {
            colBitBuffer |= ((rowBuffer[j] & (0x01 << i)) >> i) << j;
        }
        if (hasPartialEvenParity(colBitBuffer, rowBufferLen)==0)
        {
            return i;
        }
    }
    return -1;
}
