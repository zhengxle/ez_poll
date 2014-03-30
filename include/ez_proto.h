#ifndef _EZ_PROTO_H
#define _EZ_PROTO_H

#include <inttypes.h>
#include <string>

// simple & readable protocol for network programming, it is easy to pack or unpack packets~
class ez_conn;
class ez_proto
{
public:
    const std::string &pack_message(uint64_t id, uint64_t command, const char *reserved, 
            const char *body, uint64_t bodylen);
    int unpack_message(ez_conn *conn);
    
    uint64_t get_id();
    uint64_t get_command();
    const std::string &get_reserved();
    const std::string &get_body();
    uint64_t get_bodylen() { return bodylen_; }
private:
    static const char *magic_num;
    static const size_t magic_len;
    
    int unpack_field(const char *msg, size_t len, uint64_t *beg, uint64_t *end, int header_idx);
    int check_magic();
    int check_id();
    int check_command();
    int check_reserved();
    int check_bodylen();
    int check_uint64(const std::string &str);
    
    // header format: MAGIC\nID\nCOMMAND\nRESERVED\nBODYLEN\n
    std::string headers_[5];
    uint64_t id_;
    uint64_t command_;
    // body 
    std::string body_;
    uint64_t bodylen_;
    // built packet
    std::string packet_;
};

#endif
