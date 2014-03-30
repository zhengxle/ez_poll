#include "ez_proto.h"
#include "ez_conn.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char *ez_proto::magic_num = "E1Z2_3P4O5L6L7";
const size_t ez_proto::magic_len = 14;

const std::string &ez_proto::pack_message(uint64_t id, uint64_t command, const char *reserved, const char *body, uint64_t bodylen)
{
    assert(reserved && body);
    char str_id[32], str_command[32], str_bodylen[32];
    snprintf(str_id, sizeof(str_id), "%"PRIu64, id);
    snprintf(str_command, sizeof(str_command), "%"PRIu64, command);
    snprintf(str_bodylen, sizeof(str_bodylen), "%"PRIu64, bodylen);
    packet_.clear();
    packet_.append(magic_num);
    packet_.append("\n");
    packet_.append(str_id);
    packet_.append("\n");
    packet_.append(str_command);
    packet_.append("\n");
    packet_.append(reserved);
    packet_.append("\n");
    packet_.append(str_bodylen);
    packet_.append("\n");
    packet_.append(body, bodylen);
    return packet_;
}

int ez_proto::unpack_message(ez_conn *conn)
{
    assert(conn);
    const char *msg;
    size_t len;
    conn->get_message(&msg, &len);

    uint64_t beg = 0, end = 0;
    for (int header_idx = 0; header_idx != 5; ++header_idx)
    {
        int ret = unpack_field(msg, len, &beg, &end, header_idx);
        if (ret <= 0)
            return ret;
        beg = ++end;
    }
    if (check_magic() == -1 || check_id() == -1 || check_command() == -1 || check_reserved() == -1 || check_bodylen() == -1)
        return -1;
    if (len - beg >= bodylen_)
    {
        body_.assign(msg + beg, bodylen_);
        uint64_t packlen = beg + bodylen_;
        assert(conn->use_message(packlen) == 0);
        return packlen;
    }
    return 0;
}
    
int ez_proto::unpack_field(const char *msg, size_t len, uint64_t *beg, uint64_t *end, int header_idx)
{
    for ( ; *end < len; ++*end)
    {
        if (*end - *beg > 64)
            return -1;
        if (msg[*end] == '\n')
        {
            headers_[header_idx].assign(msg + *beg, msg + *end);
            return *end - *beg;
        }
    }
    return 0;
}

uint64_t ez_proto::get_id()
{
    return id_;
}
    
uint64_t ez_proto::get_command()
{
    return command_;
}
    
const std::string &ez_proto::get_reserved()
{
    return headers_[3];
}
    
const std::string &ez_proto::get_body()
{
    return body_;
}

int ez_proto::check_magic()
{
    if (headers_[0].size() != magic_len || memcmp(headers_[0].c_str(), magic_num, magic_len) != 0)
    {
        return -1;
    }
    return 0;
}

int ez_proto::check_uint64(const std::string &str)
{
    if (str.size() == 0 || str.size() > 20)
        return -1;
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (!isdigit(str[i]))
            return -1;
    }
    return 0;
}

int ez_proto::check_id()
{
    if (check_uint64(headers_[1]) == -1)
        return -1;
    id_ = strtoull(headers_[1].c_str(), NULL, 10);
    return 0;
}

int ez_proto::check_command()
{
    if (check_uint64(headers_[2]) == -1)
        return -1;
    command_ = strtoull(headers_[2].c_str(), NULL, 10);
    return 0;
}

int ez_proto::check_reserved()
{
    return 0;
}

int ez_proto::check_bodylen()
{
    if (check_uint64(headers_[4]) == -1)
        return -1;
    bodylen_ = strtoull(headers_[4].c_str(), NULL, 10);
    return 0;
}
