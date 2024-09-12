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

        void set_gerber(gerber_lib::gerber *g) override;
        void fill_elements(gerber_lib::gerber_draw_element const *elements, size_t num_elements, gerber_lib::gerber_polarity polarity, int entity_id) override;

        void on_finished_loading() override
        {
        }
        // void on_gerber_finished();

        void create_window(int x, int y, int w, int h);

        occ_viewer vout{};

        TopoDS_Shape main_face{};
        TopoDS_Shape current_face{};

        bool previous_fill{ false };
        bool current_fill{ false };

        gerber_lib::gerber *gerber_file{ nullptr };
    };

}    // namespace gerber_util