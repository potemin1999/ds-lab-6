mkdir server_workdir/

gcc -o ./server_workdir/server -lstdc++ -lpthread server.cpp
gcc -o client -lstdc++ -lpthread client.cpp

cd ./server_workdir || exit
./server 9911
