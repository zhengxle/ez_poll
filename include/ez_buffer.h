#ifndef _EZ_BUFFER_H
#define _EZ_BUFFER_H

#include <sys/types.h>

class ez_buffer
{
public:
    ez_buffer();
    ~ez_buffer();

    bool append_buffer(const char *data, size_t length);
    bool append_buffer_ex(size_t length);
    bool erase_buffer(size_t length);
    bool reserve_space(size_t length);
    void get_buffer_begin(const char **buffer, size_t *length);
    size_t get_buffer_length() const;
    void get_space_begin(char **buffer, size_t *length);
    void reset_buffer()
    {
        read_position_ = write_position_ = 0;
    }

private:
    static const size_t init_buffer_size;    
    static const size_t shrink_buffer_size;  

    char *buffer_base_;     
    size_t read_position_;  
    size_t write_position_; 
    size_t buffer_size_;    
};

#endif
