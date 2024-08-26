//////////////////////////////////////////////////////////////////////

#pragma once

#include "occ_viewer.h"
#include "occ_viewer_interactor.h"

#include "gerber_lib.h"
#include "gerber_draw.h"

#include <TopoDS_Shape.hxx>

//////////////////////////////////////////////////////////////////////

namespace gerber_3d
{
    struct occ_drawer : gerber_lib::gerber_draw_interface
    {
        occ_drawer() = default;

        void set_gerber(gerber_lib::gerber *g, int hide_elements = gerber_lib::hide_element_none) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity) override;

        void on_gerber_finished();

        void create_window(int x, int y, int w, int h);

        int elements_to_hide{ 0 };

        occ_viewer vout{};

        TopoDS_Shape main_face{};

        gerber_lib::gerber *gerber_file{ nullptr };
    };

}    // namespace gerber_util