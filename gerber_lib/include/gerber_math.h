#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    inline double round_precise(double x, int precision)
    {
        double p = pow(10, static_cast<double>(precision));
        return round(x * p) / p;
    }

    //////////////////////////////////////////////////////////////////////

    inline double deg_2_rad(double x)
    {
        return x * (M_PI / 180.0);
    }

    //////////////////////////////////////////////////////////////////////

    inline double rad_2_deg(double x)
    {
        return x * (180 / M_PI);
    }

}    // namespace gerber_lib