//////////////////////////////////////////////////////////////////////
// LOG renderer for debugging the gerber nets

#pragma once

#include "gerber_lib.h"
#include "gerber_2d.h"
#include "gerber_draw.h"

namespace gerber_3d
{
    using namespace gerber_lib;

    //////////////////////////////////////////////////////////////////////

    struct log_drawer : gerber_lib::gerber_draw_interface
    {
        log_drawer() = default;

        void set_gerber(gerber_lib::gerber *g, int hide_elements = gerber_lib::hide_element_none) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity) override;

        gerber *gerber_file{ nullptr };
        int elements_to_hide{ 0 };
    };

}    // namespace gerber_3d
