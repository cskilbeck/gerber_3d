//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <iostream>
#include <format>
#include <filesystem>

#include "gerber_lib.h"
#include "gerber_util.h"
#include "gerber_settings.h"
//#include "gdi_drawer.h"
//#include "occ_drawer.h"
#include "gl_window.h"

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

    // gerber_3d::gdi_drawer gdi;
    // gdi.create_window(850, 100, 700, 700);
    // gdi.load_gerber_file(filename);

    gerber_3d::gl_window gl;
    gl.create_window(100, 100, 700, 700);

    ShowWindow(gl.hwnd, SW_SHOW);

    bool done = false;
    while(!done) {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while(::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageA(&msg);
            if(msg.message == WM_QUIT) {
                done = true;
            }
        }
        if(done) {
            break;
        }
        if(::IsIconic(gl.hwnd) || !IsWindowVisible(gl.hwnd)) {
            ::Sleep(10);
            continue;
        }
        gl.draw();
    }
}
