//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <iostream>
#include <format>
#include <filesystem>

#include "gerber_lib.h"
#include "gerber_util.h"
#include "gerber_settings.h"
#include "gdi_drawer.h"
#include "occ_drawer.h"

//////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    LOG_CONTEXT("main", debug);

    using namespace gerber_lib;

    log_set_emitter_function(puts);
    log_set_level(log_level_debug);

    std::string filename;

    if(argc == 2) {
        filename = argv[1];
    } else {
        gerber_util::load_string("filename", filename);
    }

    gerber_3d::gdi_drawer gdi;
    gdi.create_window(850, 100, 700, 700);
    gdi.load_gerber_file(filename);

    while(true) {

        switch(MsgWaitForMultipleObjects(0, nullptr, false, 12, QS_ALLEVENTS)) {

        case WAIT_OBJECT_0: {
            MSG msg;
            while(PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {

                if(msg.message == WM_QUIT) {

                    gdi.cleanup();

                    if(!gerber_util::save_string("filename", std::filesystem::absolute(gdi.current_filename()).string())) {
                        LOG_ERROR("Huh?");
                    }
                    return 0;
                }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        } break;
        }
    }
}
