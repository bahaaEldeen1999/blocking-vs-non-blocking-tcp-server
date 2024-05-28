# Blocking Vs Non-Blocking TCP server implementation in C++ 

both implementation will run on a single thread

## Blocking Implementation 

Will use the linux Network stack as it is in sequentional matter
<br>
[explaination](blocking_server.md) 

## Non-Blocking implementation

Will Use the EPOLL system call to achive event based programming, where we listen for events on file descriptors, and only take action once events arrived,
and we don't just block the main thread waiting for event to occur
<br>
[explaination](non_blocking_server.md) 