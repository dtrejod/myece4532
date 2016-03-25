
//	PIC32 Server - Microchip BSD stack socket API
//	MPLAB X C32 Compiler     PIC32MX795F512L
//      Microchip DM320004 Ethernet Starter Board
//
// ECE4532 - Lab 4 - Linear Block Code Error Correction
// Created on 20160324
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

#define PACKETLEN 3
#define CODEWORDLEN 6
#define MSGLEN 42

void DelayMsec(unsigned int msec);
void hammingDecoder(char *recieveBuffer, int rlen);
void hammingEncoder(const char *myStr, char *tbfr, int tlen);
char getPacket(const char *myStr, int packetNum, int packetSize);
int divideDown(int a, int b);
char getEncodeCodeword (char message);
char getDecodeCodeword (char codeword);
char hammingErrorDetectorCorrector(char codeword);


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
    const char *myStr = "EE is my avocation";

    // To find tlen we take the total number of bits minus the redundant
    // MSB zero and divide by our packet size rounded up.
    //tlen = ( (strlen(myStr)*8-strlen(myStr)) + PACKETLEN - 1 )/PACKETLEN;
    tlen = MSGLEN;
    char tbfr[MSGLEN];
    
    hammingEncoder(myStr, tbfr, tlen);
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
                        mPORTDSetBits(BIT_2);   // LED3=1
                        send(clientSock, tbfr, tlen, 0);
                        DelayMsec(50);
                        mPORTDClearBits(BIT_2);	// LED3=0
                    }
                mPORTDClearBits(BIT_0); // LED1=0
                }
                // If not prefixed we say client is sending back our
                // transmits corrupted packet.
                else
                {
                    // receive possible corrupted data
                    hammingDecoder(rbfr, rlen);

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

void hammingDecoder(char *recieveBuffer, int rlen)
{
    int i;
    char codewordResized = 0x00;
    char receievedMessage;
    char z = 0x00;

    for(i=0; i < rlen; i++)
    {
        recieveBuffer[i] = hammingErrorDetectorCorrector(recieveBuffer[i]);
        receievedMessage = getDecodeCodeword(z); 
    }
}

void hammingEncoder(const char *myStr, char *tbfr, int tlen) 
{
    int i;
    char messageResized = 0x00;
    
    for(i=0; i < tlen; i++)
    {
        messageResized = getPacket(myStr, i, PACKETLEN);
        tbfr[i] = getEncodeCodeword(messageResized);
    }
}

char getPacket(const char *myStr, int packetNum, int packetSize)
{
    // We return a char containing packetSize bits
    char bitPacket = 0x00;

    // We look to see at what bit interval our packet starts on

    int stringBitStart = packetNum*packetSize;
    int charBitStart = 6-(stringBitStart%7);
    // loop tracker
    signed int i, maskPos;

    for(i=0; i < packetSize; i++)
    {
        // We create our own modulus by making sure negative numbers round
        // down to the lower negative number.
        if (charBitStart-i < 0) maskPos = divideDown(charBitStart-i,7);
        else maskPos = (charBitStart-i)/7;
        maskPos = (charBitStart-i)-7*maskPos;
        // We use seven since our char MSB is irrelevant
        bitPacket |= ((myStr[(stringBitStart+i)/7]
                        & (0x01 << maskPos))
                                >> maskPos)
                                << (packetSize-i-1);
    }

    return bitPacket;
}
int divideDown(int a, int b)
{
    int r=a/b;
    if (r<0 && r*b!=a)
        return r;
    return r-1;
}

// Takes a first PACKETLEN bits in a char and converts it to a 8 bit char
// encoded codeword
char getEncodeCodeword (char message)
{
    // Generator Matrix
    int G[PACKETLEN][CODEWORDLEN] = {{1, 0, 0, 1, 1, 0},
                                     {0, 1, 0, 1, 1, 1},
                                     {0, 0, 1, 1, 0 ,1}};
    int i, j;
    char codeword = 0x00;
    int codewordPos = CODEWORDLEN -1;

    // Matrix math at the bit level.
    for(j=0; j < CODEWORDLEN; j++)
    {
        for(i=0; i < PACKETLEN; i++){
            // Exclusive OR is same as adding two binary bits
            codeword ^= (((message & (0x01  << PACKETLEN-1-i)) 
                                            >> PACKETLEN-1-i)* G[i][j]) 
                                            << codewordPos;
        }
        // Move codeword position
        codewordPos--;
    }
    return codeword;
}


// Takes a code of CODEWORDLEN bits in a char and converts it to a 
// PACKETLEN bit ASCII decoded char
char getDecodeCodeword (char codeword)
{

    // Generator Matrix
    int R[CODEWORDLEN][PACKETLEN] = {{1, 0, 0},
                                     {0, 1, 0},
                                     {0, 0, 1},
                                     {0, 0, 0},
                                     {0, 0, 0},
                                     {0, 0, 0}};
    int i, j;
    char message = 0x00;
    int messagePos = PACKETLEN -1;

    // Matrix math at the bit level.
    for(j=0; j < PACKETLEN; j++)
    {
        for(i=0; i < CODEWORDLEN; i++){
            // Exclusive OR is same as adding two binary bits
            message ^= (((codeword & (0x01  << CODEWORDLEN-1-i)) 
                                            >> CODEWORDLEN-1-i)* R[i][j]) 
                                            << messagePos;
        }
        // Move codeword position
        messagePos--;
    }
    return message;
}

char hammingErrorDetectorCorrector(char codeword)
{
    // Generator Matrix
    int H[CODEWORDLEN][PACKETLEN] = {{1, 1, 0},
                                     {1, 1, 1},
                                     {1, 0, 1},
                                     {1, 0, 0},
                                     {0, 1, 0},
                                     {0, 0, 1}};

    int i, j;
    char syndrome = 0x00;
    int syndromePos = PACKETLEN-1;  

    // Matrix math at the bit level.
    for(j=0; j < PACKETLEN; j++)
    {
        for(i=0; i < CODEWORDLEN; i++){
            // Exclusive OR is same as adding two binary bits
            syndrome ^= (((codeword & (0x01 << CODEWORDLEN-1-i)) 
                                            >> CODEWORDLEN-1-i)* H[i][j]) 
                                            << syndromePos;
        }
        // Move codeword position
        syndromePos--;
    }

    // check if syndrome is zero
    if (syndrome != 0x00)
    {
        // if non-zero we say we have an error. We check to see if we can
        // correct the codeword.
        // Set led 1 (red) high
        mPORTDSetBits(BIT_1);
        DelayMsec(100);
        // Bit 5 an error
        if (syndrome == 0b00000110) 
        {
            // Reset LED saying we fixed the error
            mPORTDClearBits(BIT_1);
            DelayMsec(100);
            codeword ^= 0x01 << 5;
        }

        // Bit 4 an error
        else if (syndrome == 0b00000111)
        {
            // Reset LED saying we fixed the error
            mPORTDClearBits(BIT_1);
            DelayMsec(100);
            codeword ^= 0x01 << 4;
        }
        
        // Bit 3 an error
        else if (syndrome == 0b00000101)
        {
            // Reset LED saying we fixed the error
            mPORTDClearBits(BIT_1);
            DelayMsec(100);
            codeword ^= 0x01 << 3;
        }

        // Bit 2 an error
        else if (syndrome == 0b00000100)
        {
            // Reset LED saying we fixed the error
            mPORTDClearBits(BIT_1);
            DelayMsec(100);
            codeword ^= 0x01 << 2;
        }

        // Bit 1 an error
        else if (syndrome == 0b00000010)
        {
            // Reset LED saying we fixed the error
            mPORTDClearBits(BIT_1);
            DelayMsec(100);
            codeword ^= 0x01 << 1;
        }

        // Bit 0 an error
        else if (syndrome == 0b00000001)
        {
            // Reset LED saying we fixed the error
            mPORTDClearBits(BIT_1);
            DelayMsec(100);
            codeword ^= 0x01 << 0;
        }

        // Otherwise there is an uncorrectable error. We set to zero
        // for analysis purposes.
        else{
            mPORTDSetBits(BIT_0);
            codeword = 0x00;
        } 

    }
    return codeword;

}
