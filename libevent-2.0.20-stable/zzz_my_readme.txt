https://www.pacificsimplicity.ca/blog/libevent-echo-server-tutorial
gcc -Wall my_server.c -levent -o server
g++ -Wall my_server.cpp -levent -o server
./server
echo -n 'test' | nc -U /tmp/mysocket
