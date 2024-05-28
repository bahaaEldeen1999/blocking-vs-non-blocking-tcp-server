 we implemented a simple TCP server using linux networking system calls, and we had a pretty simple server that can reply to client messages, and thats it.

However our server had one major issue, that is we can only handle only one client at a time, and we can never have 2 active clients at the same time, this is a pretty major downside, in the world where hundreds of requests per minute is a common sight, we cannot tolerate such limitation.

#### How Can We Solve the one active client issue ? 

Well, we have 2 solutions:

1. We Can leverage the CPU threads, and use multi-threading approach where each client will have a separate thread
2. We Can using I/O polling, and respond to events when ever they occur, so we don't just block the main thread waiting for a client to send us something, instead we check periodically on the client, and whenever he sent something to us, then and only then do we handle his request

#### Multi-Threading

A Multi-Threaded approach will actually work fine, and will mostly solve out problems, However there is a major downside for this approach, and that threads are not free, we can't just make 1000 threads and call it a day, we actually pay some resources to use threads, and the main resource that gets consumed by threads is Memory(RAM), each thread will have its own memory, so we can't just keep making new thread for new clients, especially if we anticipate that our server can have hundreds of users connected to it simultaneously. 

So we will not go forward with this solution. 

#### I/O Polling 


Lets see how in code we can do I/O polling, First lets identify our problem: the problem is n the "accept" system call, which is called whenever a client tries to connect to us, whenever a client connect to us, we block the main thread waiting for clients messages, or waiting for the client to close the connection, this is a perfect example to introduce I/O polling in Linux.

In our case, instead of calling "accept", and wait till a client send data to us, we can actually, listen for events on our TCP socket in an event loop, and whenever a new event happens to out TCP socket we can then simply call "accept", that will establish a new socket between us and the client, and by the same principle, we can listen on events on the new socket between us and the client.

Our event loop will listen for events on both our TCP server socket, and on the new sockets between us and the connected clients, and we are interested in listening for Write events, whenever a client try to write something to us, we need to listen to those events, and decide how we want to handle them.

###### Lets implements out new non-blocking server 

We will introduce OOP to our code, and will represent out TCP Server as a class, which has 2 main functions 
1. the constructor that will create our TCP server 
2. event loop that will handle all the requests to our server

```cpp
class TCPServer
{
	public:
	TCPServer(const char* host_name, const char* port_number);
	int Listen();
};
```

in the constructor we will just create the TCP server as before:

```cpp
TCPServer(const char* host_name, const char* port_number)

{

m_SocketFD = -1;

	// create our TCP socket address info which is required for furhter system calls to create the TCP socket
	
	addrinfo tcpSocketAddressInfoHint;
	
	addrinfo* tcpSocketAddressInfoResult;
	
	tcpSocketAddressInfoHint.ai_family = AF_INET; // ipV4
	
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
```


in the "Listen"function is where all the magic happens:

first we will make out TCP server listen to connections just as before

```cpp
int Listen()

{
	// listen on the port
	
	int err = listen(m_SocketFD, 5 /* Queue up to 5 connections */);
	// rest of code goes here
};
```

Then we will create a struct called epoll, which is a file descriptor, that Linux uses to I/O polling, Epoll basically monitor file descriptors and notify us if events occured to them.

To create an epoll and make it monitor a file descriptor events we need three tings:
1. a file descriptor to monitor (in out case will be the TCP server and the new sockets between our server and the clients )
2. Which events are we interested in:
	1. reading
	2. writing
	3. .....etc

So lets make a helper function in our class that will create epolls and attach it to a file descriptor, and the flags argument will be to control which events we are interested in, as well as some other epoll flags that Linux provides:

```cpp
int create_epoll(int fd, uint32_t flags = EPOLLIN)
{

	int efd = epoll_create1(0);
	
	epoll_event e;
	
	e.data.fd = fd;
	
	e.events = 0;
	
	e.events |= flags;
	
	int err = epoll_ctl(efd, EPOLL_CTL_ADD, e.data.fd, &e);
	
	// the epoll is now created successfully and listens for Input events on the file descriptor fd
	return efd;

}
```

out next step is to use our create_epoll function, and create a new epoll attached to our socket file descriptor, and will also listen for Write events.

```cpp
// add the socket fd to epoll so we can listen to events if clients tried to connect to the server

int socketEpoll = create_epoll(m_SocketFD, EPOLLIN);
```

EPOLLIN: specifies that we get events whenever the attached file descriptor is ready for "read", which means it was written to.

now that we have a way for creating epolls, we need for epolls to tell us whenever an event has happened, and this is done using the Linux system call: "epoll_wait", which takes as an arguments: the epoll file descriptor that was returned from "create_epoll", and takes a struct that will store the info about the event that occurred in (we can ignore this for now), and a maxevents integer which specif the maximum amount of events we want to store, and lastly it takes a timeout integer.

The timeout specify how long do we want to block till an event happens on this file descriptor, the value is in milliseconds, a value of 0 means we don't wait at all basically non-blocking, if event occurred then return it, otherwise return immediately, and a value of -1 will make the wait block till event occurs, we are interested in 0, since we want no blocking at all 

So lets create another helper function, that will take as an input the epoll file descriptor and will return the number of events to us

```cpp
int check_epoll_for_new_data(int epfd, int fd, uint32_t flags = EPOLLIN)

{

	epoll_event e;
	
	e.data.fd = fd;
	
	e.events = 0;
	
	e.events |= EPOLLIN;
	
	// check the fd to see if any event did happen
	
	// if we have an event we will get value greater than zer
	
	int eventHappened = epoll_wait(epfd, &e, 1/* max number of event*/, 0/* timeout: how long should we wait till event happen*/);
	
	
	return eventHappened;

}
```

Now in out Listen function, we will start our event loop, which is simple an infinite loop 

```cpp
while(1)
{
	// remainig code goes here
}
```

The the first thing we will check on, is our TCP socket epoll, to see if we have new connections or not 
```cpp
int newClient = check_epoll_for_new_data(socketEpoll, m_SocketFD, EPOLLIN);
```

if we have a new connection, the returned value will be greater than zero, so we can create an epoll for the new client, and add the new epoll tor our active clients list, and in the event loop, we will check each client in this list if he sent a message to us or not 

first, lets create an epoll for our new client, and add it to the list 

```cpp
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
	
	m_ActiveClients.push_back({ efd, newSocketFD });

}
```

and next in the event loop, lets loop over each active client, and check if any of them sent us something, and whenever we get a message we will either reply to them, or we will close the connection if we received the word "close"

```cpp
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
			
			// add null terminator to the string
			
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
```


and That's it, we can then initialize out TCP server class in the "main" function:

```cpp
TCPServer tcpServer(NULL, "3137");

tcpServer.Listen();
```

and compile our server just like before, now we can multiple clients connected to us in the same time.



