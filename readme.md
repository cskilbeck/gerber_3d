# Gerber 3D

Attempt to make a 3D representation of a Gerber file

Parser is based on gerbv by Stefan Petersen (spe@stacken.kth.se)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

## Building

Currently, it will only build for Windows due to the GDI Drawer. This could be replaced with, for example a Cairo drawer.

The build system is CMake

It depends on Open Cascade for the 3D bit, see: https://dev.opencascade.org/doc/occt-7.4.0/overview/html/occt_dev_guides__building_cmake.html


