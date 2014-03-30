#ifndef _EZ_CLIENT_H
#define _EZ_CLIENT_H

#include "ez_conn.h"
#include "ez_client.h"

class ez_client
{
public:
    ez_client(ez_poll *poll): conn_(poll)
    {
    }
    ez_conn *get_conn()
    {
        return &conn_;
    }
    
private:
    ez_conn conn_;
};

#endif
