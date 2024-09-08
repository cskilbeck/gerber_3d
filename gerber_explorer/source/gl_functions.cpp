#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>

#include "Wglext.h"
#include "glcorearb.h"

#include "gl_functions.h"

//////////////////////////////////////////////////////////////////////

namespace
{

template <typename T> void get_proc(char const *function_name, T &function_pointer)
{
    function_pointer = reinterpret_cast<T>(wglGetProcAddress(function_name));
    if(function_pointer == nullptr) {
        fprintf(stderr, "ERROR: Can't get proc address for %s\n", function_name);
        ExitProcess(1);
    }
}

}    // namespace

#define GET_PROC(x) get_proc(#x, x)

void init_gl_functions()
{
#undef GL_FUNCTION
#define GL_FUNCTION(fn_type, fn_name) GET_PROC(fn_name)
#include "gl_function_list.h"
#undef GL_FUNCTION
}

#undef GL_FUNCTION
#define GL_FUNCTION(fn_type, fn_name) fn_type fn_name
#include "gl_function_list.h"
#undef GL_FUNCTION
