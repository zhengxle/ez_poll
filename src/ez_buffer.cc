#include "ez_buffer.h"
#include <string.h>

const size_t ez_buffer::init_buffer_size = (16 * 1024); 
const size_t ez_buffer::shrink_buffer_size = (10 * 1024 * 1024); 

ez_buffer::ez_buffer()
{
    buffer_size_ = init_buffer_size;
    buffer_base_ = new char[init_buffer_size];
    read_position_ = write_position_ = 0;
}

ez_buffer::~ez_buffer()
{
    delete []buffer_base_;
}

bool ez_buffer::append_buffer(const char *data, size_t length)
{
    if (!data || !reserve_space(length))
    {
        return false;
    }
    memcpy(buffer_base_ + write_position_, data, length);
    write_position_ += length;
    return true;
}

bool ez_buffer::append_buffer_ex(size_t length)
{
    if (length > buffer_size_ - write_position_)
    {
        return false;
    }
    write_position_ += length;
    return true;
}

bool ez_buffer::erase_buffer(size_t length)
{
    if (write_position_ - read_position_ < length)
    {
        return false;
    }
    read_position_ += length;

    if (buffer_size_ >= shrink_buffer_size && write_position_ - read_position_ < init_buffer_size)
    {
        char *smaller_buffer = new char[init_buffer_size];
        memcpy(smaller_buffer, buffer_base_ + read_position_, write_position_ - read_position_);
        delete []buffer_base_;

        buffer_base_ = smaller_buffer;
        buffer_size_ = init_buffer_size;
        write_position_ -= read_position_;
        read_position_ = 0;
    }
    
    return true;
}

bool ez_buffer::reserve_space(size_t length)
{
    if (buffer_size_ - write_position_ >= length)
    { 
    }
    else if (buffer_size_ - write_position_ + read_position_ >= length)
    {
        memmove(buffer_base_, buffer_base_ + read_position_, write_position_ - read_position_);
        write_position_ -= read_position_;
        read_position_ = 0;
    }
    else
    {
        size_t new_buffer_size = write_position_ - read_position_ + length;

        char *new_buffer = new char[new_buffer_size];
        memcpy(new_buffer, buffer_base_ + read_position_, write_position_ - read_position_);
        delete []buffer_base_;
        
        buffer_base_ = new_buffer;
        buffer_size_ = new_buffer_size;
        write_position_ -= read_position_;
        read_position_ = 0;
    }
    return true;
}
 
void ez_buffer::get_buffer_begin(const char **buffer, size_t *length)
{
    if (buffer)
    {
        *buffer = buffer_base_ + read_position_;
    }
    if (length)
    {
        *length = write_position_ - read_position_;
    }
}
        
size_t ez_buffer::get_buffer_length() const
{
    return write_position_ - read_position_;
}
         
void ez_buffer::get_space_begin(char **buffer, size_t *length)
{
    if (buffer)
    {
        *buffer = buffer_base_ + write_position_;
    }
    if (length)
    {
        *length = buffer_size_ - write_position_;
    }
}
