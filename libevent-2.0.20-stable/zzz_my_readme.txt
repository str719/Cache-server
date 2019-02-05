https://www.pacificsimplicity.ca/blog/libevent-echo-server-tutorial
gcc -Wall my_server.c -levent -o server
g++ -Wall my_server.cpp -levent -o server
g++ -Wall my_file_tester.cpp -levent -o server_file_tester -std=c++11
./server
./server_file_tester
echo -n 'test' | nc -U /tmp/mysocket
cd 'Cache server/libevent-2.0.20-stable'
