# Multi-Client-Proxy-Server
---

## Description
A HTTP Proxy that gets requests from the user and check if the requested file appears in the locally stored files, if yes, 
the file is sent from the local filesystem, otherwise, it constructs a HTTP request based on the user’s 
command line input, sends the request to a Web server, receives the reply from the server, saves the 
file locally and sends the reply to the user.
This Proxy server can handle multiple clients at the same time.
This Proxy server can block certain websites, included in a filter file.
This Proxy only support IPv4 connections, and only HTTP GET requests.

## The ThreadPool
The pool is implemented by a queue. When there is a new job to perform, it should be place in the queue. When there will be available thread (can be immediately), it will handle this job.


## How to run
To compile: 
in a linux/unix terminal run: `gcc proxyServer.c threadpool.c -o proxyServer -lpthread`
a compiled file will be created with the name proxy.

To run:
in a linux/unix terminal run: `./proxyServer <port> <pool-size> <max-number-of-request> <filter>`

