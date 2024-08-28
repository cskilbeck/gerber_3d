#pragma once

// An entity might be one of:
// outline
// linear track
// arc
// flashed aperture

namespace gerber_lib
{
    struct gerber_net;

    struct gerber_entity
    {
        int line_number_begin;
        int line_number_end;
        size_t net_index;

        gerber_entity(int begin, int end, size_t net_id) : line_number_begin(begin), line_number_end(end), net_index(net_id)
        {
        }
    };

}    // namespace gerber_lib