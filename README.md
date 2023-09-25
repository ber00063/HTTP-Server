# HTTP-Server

This is a project that I did for my operating systems course. I developed a simple HTTP server that uses a TCP socket to accept client HTTP requests for a file 
and respond with a full resource path to the file. The code implements signal handlers to properly respond to signals and shut down server. In part two,
I extended the implementation to a multi-threaded version that allows the server to concurrently communicate with multiple clients. This multi-threaded implementation 
utilizes mutexes to safely update the server queue.

Every function implements error handling for function and system calls and safely responds to errors.

The server files were provided by the professor.
