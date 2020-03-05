# 前言
这是一个展示epoll水平触发(level trigger)和边缘触发(edge trigger)，分别在read和write两种情况下有什么表现的实验，目的是彻底搞清楚epoll的水平触发和边缘触发的运作。

# 获取仓库
在linux机器上，输入如下指令
```
git clone https://github.com/Manistein/test_epoll_lt_and_et.git
```

# 编译
输入如下指令
```
gcc -o server server.c
gcc -o client client.c
```
得到server和client两个可执行文件

# 说明
server测试程序，用来测试LT和ET两种模式下，读取数据包的表现。client测试程序，用来测试LT和ET两种模式下，写入数据包的表现。

# 运行
由于我们的例子采用标准输入输出，应当在两个窗口，分别启动server测试程序和client测试程序。下面展示启动服务器的命令：  

```
./server lt
or
./server et
```
后面的lt参数表示level trigger模式，et表示edge trigger模式。而客户端的启动命令如下所示：

```
./client lt
or 
./client et
```
服务端每次epoll_wait被唤醒时，最多读取两个字节，客户端每次epoll_wait被唤醒时，最多写入八个字节。

# 读取数据包的测试结果
客户端以lt模式启动，从客户端输入aabbcc，并且按下回车(表示输入结束)，客户端发送包含aabbcc内容的数据包到服务端，在lt模式下，服务端的输出：

```
ubuntu@VM-46-92-ubuntu:~/lab/epolltest$ ./server lt
do_listen success fd:4
before epoll epoll_wait
after epoll_wait event n:1
accpet new connection fd:5
before epoll epoll_wait
after epoll_wait event n:1
read fd:5
read number 2
----aa
before epoll epoll_wait
after epoll_wait event n:1
read fd:5
read number 2
----bb
before epoll epoll_wait
after epoll_wait event n:1
read fd:5
read number 2
----cc
before epoll epoll_wait
```
在et模式下，服务端的输出：

```
ubuntu@VM-46-92-ubuntu:~/lab/epolltest$ ./server et
do_listen success fd:4
before epoll epoll_wait
after epoll_wait event n:1
accpet new connection fd:5
before epoll epoll_wait
after epoll_wait event n:1
read fd:5
read number 2
----aa
before epoll epoll_wait
```
因为客户端每次可以输入最多8个字节，而服务器每次最多只能读取两个字节，因为ET模式，每次有新数据包到达的时候，epoll_wait才会被唤醒，因此此时服务端会残留bbcc在接收缓存里，直到下次客户端有新数据包到达时，才会再次唤醒epoll_wait。  

# 写入数据包的测试结果
服务端以lt模式启动，客户端为lt的情况下，从客户端输入aabbccddeeffgghh，那么客户端的输出结果如下所示：  

```
ubuntu@VM-46-92-ubuntu:~/lab/epolltest$ ./client lt
client epoll set lt
connect to server success 
begin input
aabbccddeeffgghh
aabbccddeeffgghh
end input
before epoll_wait
after epoll_wait
begin to output
wsize:8 n:8
<<<< aabbccdd
end to output
begin input


end input
before epoll_wait
after epoll_wait
begin to output
wsize:8 n:8
<<<< eeffgghh
end to output
begin input


end input
before epoll_wait
```
因为在循环程序中，每次在开始的时候，要进行输入操作，按下回车表示结束，这里直接跳过后两次输入，客户端epoll_wait被唤醒两次，将所有的直接都写完，这里说明了在lt模式下，只要发送缓存可写入，epoll_wait就会不断被唤醒，直至程序将数据包写完，我们在首次输入的时候，为socket fd添加了EPOLLOUT事件，因此在写完数据包的时候，也要将EPOLLOUT从该fd，在epoll实例中移除，避免没东西写的时候唤醒epoll_wait。当客户端以et的模式启动时，从客户端输入aabbccddeeffgghh，那么客户端的输出结果如下所示。

```
ubuntu@VM-46-92-ubuntu:~/lab/epolltest$ clear
ubuntu@VM-46-92-ubuntu:~/lab/epolltest$ ./client et
client epoll set et
connect to server success 
begin input
aabbccddeeffgghh
aabbccddeeffgghh
end input
before epoll_wait
after epoll_wait
begin to output
wsize:8 n:8
<<<< aabbccdd
end to output
begin input


end input
before epoll_wait
```
客户端输出了8个字节，由于没有一次将16个字节的数据写完，因此我们不会对socket fd移除EPOLLOUT事件，而此时epoll_wait在整个过程中，只被唤醒了一次，如果我们不能et模式下，一次性将所有的数据写完，那么有可能导致再也没机会写完输入的数据的情况。