//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <thread>
#include <iostream>
#include <format>

#include "gerber_lib.h"
#include "gdi_drawer.h"
#include "occ_drawer.h"

#define SHOW_3D
#define SHOW_GDI

//////////////////////////////////////////////////////////////////////

int main()
{
    LOG_CONTEXT("main", debug);

    using namespace gerber_lib;

    log_set_emitter_function(puts);
    log_set_level(log_level_debug);

    int hide = 0;

    char const *filename = "gerber_test_files\\test_outline_Copper_Signal_Top.gbr";
    // char const *filename = "gerber_test_files\\2-13-1_Two_square_boxes.gbr";

    // char const *filename = "gerber_test_files\\clock_Profile.gbr";
    // char const *filename = "gerber_test_files\\clock_Copper_Signal_Bot.gbr";
    // char const *filename = "gerber_test_files\\clock_Copper_Signal_Top.gbr";

    // char const *filename = "gerber_test_files\\TimerSwitch.GTL";
    // char const *filename = "gerber_test_files\\TimerSwitch_regions_only.GTL";

    // char const *filename = "gerber_test_files\\controller_Copper_Signal_Top.gbr";
    // char const *filename = "gerber_test_files\\react4_Copper_Signal_Bot.gbr";
    // char const *filename = "gerber_test_files\\ble_gadget_Copper_Signal_Top.gbr";
    // char const *filename = "gerber_test_files\\clutch_pcb_Copper_Signal_Top.gbr";

    // char const *filename = "gerber_test_files\\SMD_prim_21.gbr";
    // char const *filename = "gerber_test_files\\SMD_prim_21_single.gbr";
    // char const *filename = "gerber_test_files\\region.gbr";

    // char const *filename = "gerber_test_files\\arc_1.gbr";
    // char const *filename = "gerber_test_files\\arc_2.gbr";
    // char const *filename = "gerber_test_files\\arc_3.gbr";
    // char const *filename = "gerber_test_files\\arc_4.gbr";
    // char const *filename = "gerber_test_files\\arc_5.gbr";

    // char const *filename = "gerber_test_files\\wch554g_Copper_Signal_Bot.gbr";
    // char const *filename = "gerber_test_files\\wch554g_Soldermask_Bot.gbr";
    // char const *filename = "gerber_test_files\\wch554g_Profile.gbr";

    // char const *filename = "gerber_test_files\\buck4_Copper_Signal_Top.gbr";

    gerber g;

    if(g.parse_file(filename) != ok) {
        return 1;
    }

#if 0
    hide |= hide_element_lines;
    // hide |= hide_element_arcs;
    hide |= hide_element_circles;
    hide |= hide_element_rectangles;
    hide |= hide_element_ovals;
    hide |= hide_element_polygons;
    hide |= hide_element_outlines;
    hide |= hide_element_macros;
#endif

#if defined(SHOW_GDI)
    gerber_3d::gdi_drawer gdi;
    gdi.create_window(850, 100, 700, 700);
    gdi.set_gerber(&g, hide);
#endif

    // For OCC, create the mesh in a thread

    HANDLE occ_event = CreateEvent(nullptr, false, false, nullptr);

#if defined(SHOW_3D)
    gerber_3d::occ_drawer occ;
    occ.show_percent_progress = true;
    occ.create_window(100, 100, 700, 700);
    std::thread([&]() {
        occ.set_gerber(&g, hide);

        // tell main thread that the mesh is ready to add to the scene
        SetEvent(occ_event);
    }).detach();
#endif

    MSG msg;

    HANDLE events[] = { occ_event };

    while(true) {

        switch(MsgWaitForMultipleObjects((DWORD)array_length(events), events, false, 12, QS_ALLEVENTS)) {

        case WAIT_OBJECT_0:
            // add mesh to scene
            occ.on_gerber_finished();
            break;

        case WAIT_OBJECT_0 + 1:
            while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {

                if(msg.message == WM_QUIT) {
                    gdi.cleanup();
                    return 0;
                }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            break;
        }
    }
}
