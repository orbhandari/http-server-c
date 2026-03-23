# HTTP Server in C

Mostly from scratch.

## Example usage
*wip*

## Motivation
*wip*

There are a few reasons why I wanted to do this. In no particular order:

- I first came across the idea from the Youtube channel Low Level. He suggested a good way to learn programming is to write an HTTP server in C.
- I'm interested in compilers, and since HTTP servers involves writing a parser for it, it seemed like a good start.
- To learn (under the hood) how network libraries works, at least at the application layer, and how it interacts with the Linux kernel through syscalls and the transport layer.
- I've never fully understood what the composite phrase "single-threaded non-blocking asynchronous I/O". Maybe this would teach me.

## Architecture
*wip*
- Insert diagram here.

## Writeup
*wip*

Things I want to cover:
- Why single-threading instead of multi-threading, and what challenges single-threading brings. This includes using `epoll`, how `recv` is blocking.
- How TCP is a stream-oriented protocol (and hence the name `SOCK_STREAM`), which means there's no message boundary for HTTP messages. Partial buffers means I required per-client queues. Can discuss memory tradeoff and how that may or may not be an issue. 
- How the two issues of array shifting problem and failed `check_if_complete` calls are alleviated.
- How the DataIO is still blocking and how I plan to improve that.
- The idea of a (finite) state machine and how it's implemented in the `HTTPParser` module.
- `epoll` issues with edge-triggered interfaces, and what design tradeoff it involves.

## Future plan
*wip*
- Async engine for the `DataIO` module.

## Learning notes
- Why use preprocessor macros instead of `const`?
- `select` vs `poll` vs `epoll`?
- `strncmp` vs `strcmp`
- Why always pass in pointers of caller-created objects into functions? 
- 
