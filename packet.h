#ifndef __PACKET_H__
#define __PACKET_H__

struct packet {
    char *source;
    char *dest;
    int src_port;
    int dst_port;
    char *payload;
};

#endif
