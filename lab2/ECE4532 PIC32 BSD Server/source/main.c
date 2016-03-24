
//	PIC32 Server - Microchip BSD stack socket API
//	MPLAB X C32 Compiler     PIC32MX795F512L
//      Microchip DM320004 Ethernet Starter Board

// Created on 20160216
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

#define tlen1 50

void DelayMsec(unsigned int);

int main() {
    // Initialize Sockets and IP address containers
    //
    SOCKET 	serverSock, clientSock = INVALID_SOCKET;
    IP_ADDR	curr_ip, ip;

    // Initialize buffer length variables
    //
    int rlen, sent, bytesSent;
    
    // Initialize the Send/Recv buffers
    //
    char rbfr[10];

    // Socket struct descriptor
    //
    struct sockaddr_in addr;
    int addrlen = sizeof(struct sockaddr_in);

    // System clock containers
    //
    unsigned int sys_clk, pb_clk;

    // Initialize LED Variables:
    // Setup the LEDs on the PIC32 board
    // RD0, RD1 and RD2 as outputs
    //
    mPORTDSetPinsDigitalOut(BIT_0 | BIT_1 | BIT_2 ); 
    mPORTDClearBits(BIT_0 | BIT_1 | BIT_2); // Clear previous LED status.
    
    // Setup the switches on the PIC32 board as inputs
    //		
    mPORTDSetPinsDigitalIn(BIT_6 | BIT_7 | BIT_13);     // RD6, RD7, RD13 as inputs

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
    if((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == 
        SOCKET_ERROR)  return -1;
       
    // Ensure we bound to the socket. End Program if bind fails
    //
    if(bind(serverSock, (struct sockaddr*) &addr, addrlen ) == 
        SOCKET_ERROR)
        return -1;

    // Listen to up to five clients on server socket
    //
    listen(serverSock, 5);

    // We store our desired transfer paragraph 
    //
    char myStr[] = "TCP/IP (Transmission Control Protocol/Internet Protocol) is "
        "the basic  communication language or protocol of the Internet. "
        "It can also be used as a communications protocol in a private "
        "network (either an intranet or an extranet). When you are set up "
        "with direct access to the Internet, your computer is provided "
        "with a copy of the TCP/IP program just as every other computer "
        "that you may send messages to or get information from also has "
        "a copy of TCP/IP. TCP/IP is a two-layer program. The higher "
        "layer, Transmission Control Protocol, manages the assembling "
        "of a message or file into smaller packets that are transmitted "
        "over the Internet and received by a TCP layer that reassembles "
        "the packets into the original message. The lower layer, "
        "Internet Protocol, handles the address part of each packet so "
        "that it gets to the right destination. Each gateway computer on "
        "the network checks this address to see where to forward the "
        "message. Even though some packets from the same message are "
        "routed differently than others, they'll be reassembled at the "
        "destination.\0";
    
    // Chunk up our data
    //
    // Copy our string into our buffer
    //
    int tlen = strlen(myStr);
    
    char tbfr1[tlen1+1];

    // Loop forever
    //
    while(1) {
        // Refresh TCIP and DHCP
        //
        TCPIPProcess();
        DHCPTask();

        // Get the machines IP address and save to variable
        //
        ip.Val = TCPIPGetIPAddr();

        // DHCP server change IP address?
        //
        if(curr_ip.Val != ip.Val) curr_ip.Val = ip.Val;	

        // TCP Server Code
        //
        if(clientSock == INVALID_SOCKET) {
            // Start listening for incoming connections
            //
            clientSock = accept(serverSock, (struct sockaddr*) &addr, &addrlen);

            // Upon connection to a client blink LEDS.
            //
            if(clientSock != INVALID_SOCKET) {
                setsockopt(clientSock, SOL_SOCKET, TCP_NODELAY, 
                    (char*)&tlen, sizeof(int));
                mPORTDSetBits(BIT_0);   // LED1=1
                DelayMsec(50);
                mPORTDClearBits(BIT_0); // LED1=0
                mPORTDSetBits(BIT_1);   // LED2=1
                DelayMsec(50);
                mPORTDClearBits(BIT_1); // LED2=0
                mPORTDSetBits(BIT_2);   // LED3=1
                DelayMsec(50);
                mPORTDClearBits(BIT_2); // LED3=0
            }
        }
        else {
            // We are connected to a client already. We start
            // by receiving the message being sent by the client
            //
            rlen = recvfrom(clientSock, rbfr, sizeof(rbfr), 0, NULL, 
                NULL);

            // Check to see if socket is still alive
            //
            if(rlen > 0) {
                // If the received message first byte is '02' it signifies
                // a start of message
                //
                if (rbfr[0]==2) {
                    //mPORTDSetBits(BIT_0);	// LED1=1

                    // Check to see if message begins with
                    // '0271' to see if the message is a a global reset
                    //
                    if(rbfr[1]==71) {
                        mPORTDSetBits(BIT_0);   // LED1=1
                        DelayMsec(50);
                        mPORTDClearBits(BIT_0); // LED1=0
                    }
                }
                // If the received message starts with a second byte is
                // '84' it signifies a initiate transfer
                //
                if(rbfr[1]==84){
                    mPORTDSetBits(BIT_2);   // LED3=1
                    bytesSent = 0;
                    //sent = 0;
                    while (bytesSent < tlen){
                        memcpy(tbfr1, myStr+bytesSent, tlen1);
                        if (bytesSent > 1049){
                            tbfr1[tlen-bytesSent+1] = '\0';
                            send(clientSock, tbfr1, tlen-bytesSent+1, 0);
                        }
                        else{
                            tbfr1[tlen1] = '\0';
                            // Loop until we send the full message
                            //
                            send(clientSock, tbfr1, tlen1+1, 0);
                        }
                        bytesSent += tlen1;
                        DelayMsec(50);
                    }
                    mPORTDClearBits(BIT_2);	// LED3=0
                }
                mPORTDClearBits(BIT_0); // LED1=0
            }

            // The client has closed the socket so we close as well
            //
            else if(rlen < 0) {
                closesocket(clientSock);
                clientSock = SOCKET_ERROR;
            }
        }
    }
}

// Function : DelayMsec( )
// 
// Delays the program by specified millisecond passed
//
void DelayMsec(unsigned int msec){
    unsigned int tWait, tStart;
    tWait=(SYS_FREQ/2000)*msec;
    tStart=ReadCoreTimer();
    while((ReadCoreTimer()-tStart)<tWait);
}
