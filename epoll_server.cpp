#include <iostream>

#include <sys/socket.h> // for socket()
#include <netdb.h> // addrinfo
#include <unistd.h> // close
#include <cstring>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>
#include <list>
#include <utility>


class TCPServer 
{
    private:
  
    int m_SocketFD;
    std::list<std::pair<int, int>> m_ActiveClients;

    private:
    int create_epoll(int fd, uint32_t flags = EPOLLIN)
    {
        int efd = epoll_create1(0);
        if(efd == -1)
        {
            std::cout<<"Error in EPOLL Create"<<strerror(errno)<<"\n";
            return -1;
        }


        epoll_event e;
        e.data.fd = fd;
        e.events = 0;
        e.events |= flags;
        int err = epoll_ctl(efd, EPOLL_CTL_ADD, e.data.fd, &e);
        if(err == -1)
        {
            std::cout<<"Error in EPOLL CTL "<<strerror(errno)<<"\n";
            return -1;
        }

        // the epoll is now created successfully and listens for Input events on the file descriptor fd 
        return efd;
    }

    int check_epoll_for_new_data(int epfd, int fd, uint32_t flags = EPOLLIN)
    {
        epoll_event e;
        e.data.fd = fd;
        e.events = 0;
        e.events |= EPOLLIN;
        
        // check the fd to see if any event did happen
        // if we have an event we will get value greater than zer 
        int eventHappened = epoll_wait(epfd, &e, 1/* max number of event*/, 0/* timeout: how long should we wait till event happen*/);
        if(eventHappened == -1)
        {
            std::cout<<"Error in EPOLL WAIT\n";
            return -1;
        }
        return eventHappened;
    }

    public:
    TCPServer(const char* host_name, const char* port_number)
    {
        m_SocketFD = -1;
        // create our TCP socket address info which is required for furhter system calls to create the TCP socket 
        
        addrinfo tcpSocketAddressInfoHint;
        addrinfo* tcpSocketAddressInfoResult;
        tcpSocketAddressInfoHint.ai_family = AF_INET;  // ipV4
        tcpSocketAddressInfoHint.ai_socktype = SOCK_STREAM; // TCP  
        tcpSocketAddressInfoHint.ai_protocol = 0;
        tcpSocketAddressInfoHint.ai_flags = AI_PASSIVE;
        tcpSocketAddressInfoHint.ai_addr = NULL;
        tcpSocketAddressInfoHint.ai_next = NULL;
        tcpSocketAddressInfoHint.ai_canonname = NULL;

        getaddrinfo(host_name, port_number, &tcpSocketAddressInfoHint, &tcpSocketAddressInfoResult);

        for(addrinfo* res = tcpSocketAddressInfoResult; res != NULL; res = tcpSocketAddressInfoResult->ai_next)
        {
            // create TCP socket file descriptor 
            int socketFD = socket(res->ai_family , res->ai_socktype, res->ai_protocol);
            if(socketFD == -1)
                continue; // Try next address
            
            // bind the socket to the address
            int err = bind(socketFD, res->ai_addr, res->ai_addrlen);
            if(err == 0) // we bind successfully 
            {
                m_SocketFD = socketFD;
                break;
            }
        }
    };

    // this will block so should be last thing called in the code
    int Listen()
    {  
        if(m_SocketFD == -1)
        {
            // we couldn't create TCP server successfully 
            std::cout<<"TCP server wasn't created successfully\n";
            return -1;
        }
        // listen on the port
        int err = listen(m_SocketFD, 5 /* Queue up to 5 connections */);

        if(err)
        {
            std::cout<<"TCP server Can't listen on port\n";
            return -1;
        }

        // add the socket fd to epoll so we can listen to events if clients tried to connect to the server
        int socketEpoll = create_epoll(m_SocketFD, EPOLLIN | EPOLLET);
        if(socketEpoll == -1)
        {
            std::cout<<"Couldn't create epoll for our server instance\n";
            return -1;
        }
        while(1)
        {

            // check if a new client is trying to connect to our serve 
            int newClient =  check_epoll_for_new_data(socketEpoll, m_SocketFD, EPOLLIN|EPOLLET);
            if(newClient > 0)
            {
                // a client connected to us 
                // accept this client 
                sockaddr_in clientAddress;
                socklen_t clientAddressSize = sizeof(clientAddressSize);
                // when we accept a connection we will get back a file descriptor 
                // that can be used to communicate with the client 
                int newSocketFD = accept(m_SocketFD,(sockaddr *)&clientAddress, &clientAddressSize);
                std::cout<<"new Socket File Descriptor "<<newSocketFD<<"\n";

                // now we should create an epoll for this new socket file descriptor so we can listen on events 
                int efd = create_epoll(newSocketFD);

                if(efd == -1) continue; // couldn't create epoll 
                
                // add the epoll file descriptor as well as the new socket file descriptor to our active clients list 
                m_ActiveClients.push_back({ efd, newSocketFD  });
            }

            
            

            // poll our active clients to see if any of their sockets have data written to them that needs reading 
            auto it = m_ActiveClients.begin();
            while(it != m_ActiveClients.end())
            {
                int eventHappend = check_epoll_for_new_data(it->first, it->second);
                
                if(eventHappend == -1) continue; // an error occured 

                if(eventHappend > 0)
                {
                    // some data is written to the socket 
                    // lets read it 
                    constexpr const unsigned int MAX_BUFF_SIZE = 256; 
                    char buffer[MAX_BUFF_SIZE];
                    // got input 
                    // get bytes sent from client if there are any
                    int numberOfBytesRead = read(it->second, buffer, MAX_BUFF_SIZE);
                    if(numberOfBytesRead > 0)
                    {

                        // the client sent something to us 
                        // for now we just reply back to him and close the connection so we can connect to other clients 
                        
                        // add  null terminator to the string 
                        buffer[numberOfBytesRead] = '\0';
                        std::cout<<"Recieved from client "<<numberOfBytesRead<<" Bytes\nThe Client Says: "<<buffer<<"\n";
 

                        // if message is close then close the connection
                        if(strcmp("close\n",buffer) == 0)
                        {
                            strcpy(buffer, "closing");
                            send(it->second, buffer, strlen(buffer), 0 );
                            close(it->second);

                            // remove active client 
                            it = m_ActiveClients.erase(it);
                            continue;
                        }
                        else 
                        {
                            // send to client that we got his message 
                            strcpy(buffer, "I recieved your message\n");
                            send(it->second, buffer, strlen(buffer), 0 );
                        }
                    }
                }
                ++it;
            }
        }
    };
};

int main()
{
    TCPServer tcpServer(NULL, "3137");
    tcpServer.Listen(); 
    // no code will be executed after listen


    return 0;
}