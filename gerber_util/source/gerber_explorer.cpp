//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <iostream>
#include <format>

#include "gerber_lib.h"
#include "gdi_drawer.h"
#include "occ_drawer.h"

#define SHOW_3D

//////////////////////////////////////////////////////////////////////

int main()
{
    LOG_CONTEXT("main", debug);

    using namespace gerber_lib;

    log_set_emitter_function(puts);
    log_set_level(log_level_debug);

    // char const *filename = "gerber_test_files\\test_outline_Copper_Signal_Top.gbr";
    // char const *filename = "F:\\test_pcb\\test_outline_Copper_Signal_Top.gbr";
    // char const *filename = "gerber_test_files\\2-13-1_Two_square_boxes.gbr";

    // char const *filename = "gerber_test_files\\clock_Profile.gbr";

    // char const *filename = "gerber_test_files\\gnarly_jlcpcb_production.gbr";
    // char const *filename = "gerber_test_files\\minimal_jlcpcb.gbr";

    // char const *filename = "gerber_test_files\\TimerSwitch_local_origin.GTL";
    // char const *filename = "gerber_test_files\\TimerSwitch.GTL";
    // char const *filename = "gerber_test_files\\TimerSwitch_regions_only.GTL";
    // char const *filename = "gerber_test_files\\TimerSwitch_Copper_Signal_Top.gbr";

    // char const *filename = "gerber_test_files\\controller_Copper_Signal_Top.gbr";
    // char const *filename = "gerber_test_files\\react4_Copper_Signal_Bot.gbr";
    // char const *filename = "gerber_test_files\\ble_gadget_Copper_Signal_Top.gbr";
    char const *filename = "gerber_test_files\\clutch_pcb_Copper_Signal_Top.gbr";
    // char const *filename = "gerber_test_files\\buck4_Copper_Signal_Top.gbr";
    // char const *filename = "gerber_test_files\\wch554g_Copper_Signal_Bot.gbr";
    // char const *filename = "gerber_test_files\\clock_Copper_Signal_Bot.gbr";
    // char const *filename = "gerber_test_files\\clock_Copper_Signal_Top.gbr";

    // char const *filename = "gerber_test_files\\SMD_prim_21.gbr";
    // char const *filename = "gerber_test_files\\SMD_prim_21_single.gbr";
    // char const *filename = "gerber_test_files\\region.gbr";

    // char const *filename = "gerber_test_files\\arc_1.gbr";
    // char const *filename = "gerber_test_files\\arc_2.gbr";
    // char const *filename = "gerber_test_files\\arc_3.gbr";
    // char const *filename = "gerber_test_files\\arc_4.gbr";
    // char const *filename = "gerber_test_files\\arc_5.gbr";

    // char const *filename = "gerber_test_files\\wch554g_Soldermask_Bot.gbr";
    // char const *filename = "gerber_test_files\\wch554g_Profile.gbr";

    gerber g;

    if(g.parse_file(filename) != ok) {
        return 1;
    }

    gerber_3d::gdi_drawer gdi;
    gdi.create_window(850, 100, 700, 700);
    gdi.set_gerber(&g);

    while(true) {

        switch(MsgWaitForMultipleObjects(0, nullptr, false, 12, QS_ALLEVENTS)) {

        case WAIT_OBJECT_0: {
            MSG msg;
            while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {

                if(msg.message == WM_QUIT) {
                    gdi.cleanup();
                    return 0;
                }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        } break;
        }
    }
}
