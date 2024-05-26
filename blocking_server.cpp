#include <iostream>

#include <sys/socket.h> // for socket()
#include <netdb.h> // addrinfo
#include <unistd.h> // close
#include <cstring>

int main()
{
    // 1 - create our TCP socket address info which is required for furhter system calls to create the TCP socket 
    addrinfo tcpSocketAddressInfoHint;
    addrinfo* tcpSocketAddressInfoResult;
    tcpSocketAddressInfoHint.ai_family = AF_INET;  // ipV4
    tcpSocketAddressInfoHint.ai_socktype = SOCK_STREAM; // TCP  
    tcpSocketAddressInfoHint.ai_protocol = 0;
    tcpSocketAddressInfoHint.ai_flags = AI_PASSIVE;
    tcpSocketAddressInfoHint.ai_addr = NULL;
    tcpSocketAddressInfoHint.ai_next = NULL;
    tcpSocketAddressInfoHint.ai_canonname = NULL;

    int err = getaddrinfo(NULL /* localhost */, "3331" /* port number*/, &tcpSocketAddressInfoHint, &tcpSocketAddressInfoResult);
    if(err != 0)
    {
        std::cout<<"Can't Get Address Info For Socket\n"<<gai_strerror(err)<<"\n";
        return 0;
    }
    // 2 - we try each address in the tcp result info, till we can bind successfully 
    for(addrinfo* res = tcpSocketAddressInfoResult; res != NULL; res = tcpSocketAddressInfoResult->ai_next)
    {
        // 2.1 - create TCP socket file descriptor 
        int socketFD = socket(res->ai_family , res->ai_socktype, res->ai_protocol);
        if(socketFD == -1)
            continue; // Try next address
        // 2.2 - bind the socket to the address
        int err = bind(socketFD, res->ai_addr, res->ai_addrlen);
        if(err == 0) // we bind successfully 
        {
            // 2.3 listen for incoming connections
            std::cout<<"Listening For Incoming Connections\n";
            int err = listen(socketFD, 5 /* Queue up to 5 connections */);
            if(err)
            {
                std::cout<<"Couldn't listen on the socket file descriptor\n";
                return -1;
            }
            while(1)
            {
                // 2.4 accept the incoming connection 
                // create address for client that will be connected
                sockaddr_in clientAddress;
                socklen_t clientAddressSize = sizeof(clientAddressSize);
                // when we accept a connection we will get back a file descriptor 
                // that can be used to communicate with the client 
                int newSocketFD = accept(socketFD,(sockaddr *)&clientAddress, &clientAddressSize);
                std::cout<<"new Socket File Descriptor "<<newSocketFD<<"\n";
                if(newSocketFD < 0)
                {
                    std::cout << "Error accepting request from client!\n";
                    return 0;
                }
                // 2.5 enter an infinite blocking loop where the server can communicate with the client
                while(1)
                {   
                    std::cout<<"A Client Has Connected\n";
                    // create a buffer to store the bytes sent from the client
                    constexpr const unsigned int MAX_BUFF_SIZE = 256; 
                    char buffer[MAX_BUFF_SIZE];

                    // get bytes sent from client if there are any
                    int numberOfBytesRead = recv(newSocketFD, buffer, MAX_BUFF_SIZE, 0 );
                    if(numberOfBytesRead > 0)
                    {

                        // the client sent something to us 
                        // for now we just reply back to him and close the connection so we can connect to other clients 
                        
                        // add  null terminator to the string 
                        *(buffer+numberOfBytesRead) = '\0';
                        std::cout<<"Recieved from client "<<numberOfBytesRead<<" Bytes\nThe Client Says: "<<buffer<<"\n";
                        // send to client that we got his message 
                        strcpy(buffer, "I recieved your message and will close connection now\n");
                        send(newSocketFD, buffer, strlen(buffer), 0 );
                        close(newSocketFD);
                        break;
                    }
                }
            } 
            break;
        }
    }


    return 0;
}