//
// Created by ilya on 10/1/19.
//

#ifndef LAB_6_PROTO_H
#define LAB_6_PROTO_H

// 4 bytes for uint code
// 4 bytes for uint file size
// 4 bytes for uint chunk size
// 4 bytes for file name length
// N bytes for file name without \0
#define COMMAND_SEND 1

// 4 bytes for uint code
// 4 bytes for uint file name (if changed)
// N bytes (if previous != 0)
#define RESPONSE_SEND_OK 0

// 4 bytes for uint code
// 4 bytes of data size
// N bytes of data
#define COMMAND_SEND_CHUNK 2

// 4 bytes for uint code, zero after
#define RESPONSE_SEND_CHUNK_OK 0

// 4 bytes of uint code
#define COMMAND_SEND_COMMIT 3

// 4 bytes of uint code
#define RESPONSE_SEND_COMMIT_OK 0

// 4 bytes for uint code
#define COMMAND_SEND_RESET 4

// 4 bytes for uint code
#define RESPONSE_SEND_RESET_OK 0

// 4 bytes for uint code,
// 4 bytes for uint message size
// n bytes without \0 for error message
#define RESPONSE_FAIL 1

#endif //LAB_6_PROTO_H
