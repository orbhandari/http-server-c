#ifndef HTTP_DEFINITIONS_H
#define HTTP_DEFINITIONS_H

// Max URI length is not specified in RFC, so it seems we have freedom
// to choose it. Common maximum is 2000-2048 bytes, however in our toy
// project, we go lower.
#define G_MAX_URI_LEN 1024

#define SP ' '

// Defined by HTTP/0.9 RFC.
#define G_REQUEST_LINE_NUM_TOKENS 2

#endif
