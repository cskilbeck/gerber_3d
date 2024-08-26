//-----------------------------------------------------------------------------
// Created on: 24 August 2017
//-----------------------------------------------------------------------------
// Copyright (c) 2017, Sergey Slyadnev (sergey.slyadnev@gmail.com)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//    * Neither the name of the copyright holder(s) nor the
//      names of all contributors may be used to endorse or promote products
//      derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

// Own include
#include "gerber_lib.h"
#include "occ_viewer.h"

// OpenCascade includes
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Handle.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_AmbientLight.hxx>
#include <V3d_DirectionalLight.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>
#include <STEPControl_Writer.hxx>
#include <ShapeFix_Shape.hxx>

LOG_CONTEXT("occ", debug);

namespace
{
    //////////////////////////////////////////////////////////////////////
    //! Adjust the style of local selection.
    //! \param[in] context the AIS context.

    void adjust_selection_style(const Handle(AIS_InteractiveContext) & context)
    {
        // Initialize style for sub-shape selection.
        Handle(Prs3d_Drawer) renderer = new Prs3d_Drawer;
        renderer->SetLink(context->DefaultDrawer());
        renderer->SetFaceBoundaryDraw(true);
        renderer->SetDisplayMode(1);    // Shaded
        renderer->SetTransparency(0.5f);
        renderer->SetZLayer(Graphic3d_ZLayerId_Topmost);
        renderer->SetColor(Quantity_NOC_HOTPINK);
        renderer->SetBasicFillAreaAspect(new Graphic3d_AspectFillArea3d());

        // Adjust fill area aspect.
        const Handle(Graphic3d_AspectFillArea3d) &fill_area = renderer->BasicFillAreaAspect();
        fill_area->SetInteriorColor(Quantity_NOC_BLACK);
        fill_area->SetBackInteriorColor(Quantity_NOC_BLACK);
        fill_area->ChangeFrontMaterial().SetMaterialName(Graphic3d_NameOfMaterial_Copper);
        fill_area->ChangeFrontMaterial().SetTransparency(0.4f);
        fill_area->ChangeBackMaterial().SetMaterialName(Graphic3d_NameOfMaterial_Copper);
        fill_area->ChangeBackMaterial().SetTransparency(0.4f);

        renderer->UnFreeBoundaryAspect()->SetWidth(1.0);

        context->SetHighlightStyle(Prs3d_TypeOfHighlight_LocalSelected, renderer);
    }
}    // namespace

//////////////////////////////////////////////////////////////////////

void occ_viewer::save_brep()
{
    LOG_CONTEXT("export", info);
    char const *brep_filename = "F:\\test.brep";
    char const *step_filename = "F:\\test.step";

    LOG_INFO("Export begins, applying ShapeFix_Shape...");

    TopoDS_Compound compound;
    STEPControl_Writer step_writer;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    int num_shapes = 0;
    for(auto const &sh : shapes) {

        ShapeFix_Shape fixer(sh);
        fixer.Perform();
        builder.Add(compound, fixer.Shape());
        num_shapes += 1;
    }

    LOG_INFO("ShapeFix_Shape applied to {} shapes", num_shapes);

    LOG_INFO("Saving BREP to {}", brep_filename);
    BRepTools::Write(compound, "F:\\test.brep", true, false, TopTools_FormatVersion_VERSION_2);

    LOG_INFO("Saving STEP to {}", step_filename);
    step_writer.Transfer(compound, STEPControl_AsIs);
    step_writer.Write("F:\\test.step");

    LOG_INFO("Export complete");
}

//////////////////////////////////////////////////////////////////////

HRESULT occ_viewer::create_window(int left, int top, int width, int height)
{
    char const *window_class_name = "Gerber_3D_Window_Class";

    static HINSTANCE app_instance = NULL;

    if(app_instance == NULL) {

        app_instance = GetModuleHandleA(NULL);

        WNDCLASSA WC{};
        WC.cbClsExtra = 0;
        WC.cbWndExtra = 0;
        WC.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        WC.hCursor = LoadCursor(NULL, IDC_ARROW);
        WC.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        WC.hInstance = app_instance;
        WC.lpfnWndProc = (WNDPROC)wnd_proc_proxy;
        WC.lpszClassName = window_class_name;
        WC.lpszMenuName = 0;
        WC.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;

        if(!RegisterClassA(&WC)) {
            DWORD err = GetLastError();
            LOG_ERROR("Failed to register window class: {}", err);
            return HRESULT_FROM_WIN32(err);
        }
    }

    RECT rc;
    SetRect(&rc, left, top, left + width, top + height);

    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    hwnd = CreateWindowA(window_class_name, "Gerber 3D", WS_OVERLAPPEDWINDOW, rc.left, rc.top, w, h, NULL, NULL, app_instance, this);

    if(hwnd == NULL) {
        DWORD err = GetLastError();
        LOG_ERROR("Failed to create window: {}", err);
        return HRESULT_FROM_WIN32(err);
    }

    ShowWindow(hwnd, TRUE);

    init((HANDLE)hwnd);

    return S_OK;
}

//////////////////////////////////////////////////////////////////////

void occ_viewer::add_shape(const TopoDS_Shape &shape)
{
    shapes.push_back(shape);
}

//////////////////////////////////////////////////////////////////////

void occ_viewer::add_shapes_to_scene()
{
    AIS_DisplayMode mode = AIS_Shaded;

    LOG_INFO("Adding shapes to scene");

    int shape_index = 0;
    for(auto const &sh : shapes) {
        LOG_INFO("Adding shape {}", shape_index);
        Handle(AIS_Shape) shape = new AIS_Shape(sh);
        interactive_context->SetAngleAndDeviation(shape, 1 * M_PI / 180, false);
        interactive_context->Display(shape, true);
        interactive_context->SetDisplayMode(shape, mode, true);
        adjust_selection_style(interactive_context);
        interactive_context->Activate(4, true);    // faces
        interactive_context->Activate(2, true);    // edges
        interactive_context->SetAutomaticHilight(Standard_True);
        interactive_context->SetToHilightSelected(Standard_False);
        shape_index += 1;
    }
    LOG_INFO("Added {} shapes to scene", shape_index);
    view->SetProj(V3d_TypeOfOrientation_Zup_Top);
    view->FitAll();
}

//////////////////////////////////////////////////////////////////////

void occ_viewer::init(const HANDLE &windowHandle)
{
    static Handle(Aspect_DisplayConnection) displayConnection;

    HWND winHandle = (HWND)windowHandle;

    if(winHandle == NULL) {
        return;
    }

    if(displayConnection.IsNull()) {
        displayConnection = new Aspect_DisplayConnection();
    }

    Handle(OpenGl_GraphicDriver) graphicDriver = new OpenGl_GraphicDriver(displayConnection, false);

    viewer = new V3d_Viewer(graphicDriver);

    Handle(V3d_DirectionalLight) LightDir = new V3d_DirectionalLight(V3d_Zneg, Quantity_Color(Quantity_NOC_WHITE), 1);
    Handle(V3d_AmbientLight) LightAmb = new V3d_AmbientLight();

    LightDir->SetDirection(1.0, -2.0, -10.0);
    viewer->AddLight(LightDir);
    viewer->AddLight(LightAmb);
    viewer->SetLightOn(LightDir);
    viewer->SetLightOn(LightAmb);

    interactive_context = new AIS_InteractiveContext(viewer);

    const Handle(Prs3d_Drawer) &contextDrawer = interactive_context->DefaultDrawer();

    if(!contextDrawer.IsNull()) {
        const Handle(Prs3d_ShadingAspect) &SA = contextDrawer->ShadingAspect();
        const Handle(Graphic3d_AspectFillArea3d) &FA = SA->Aspect();
        contextDrawer->SetFaceBoundaryDraw(true);
        FA->SetEdgeOff();

        // Fix for inifinite lines has been reduced to 1000 from its default value 500000.
        contextDrawer->SetMaximalParameterValue(1000);
    }

    viewer->SetDefaultBackgroundColor(Quantity_NOC_GRAY66);
    viewer->SetDefaultTypeOfView(V3d_PERSPECTIVE);
    view = viewer->CreateView();
    view->SetImmediateUpdate(false);

    // Event manager is constructed when both contex and view become available.
    viewer_interactor = new occ_viewer_interactor(view, interactive_context, this);

    wnt_window = new WNT_Window(winHandle);
    view->SetWindow(wnt_window, nullptr);

    if(!wnt_window->IsMapped()) {
        wnt_window->Map();
    }
    view->MustBeResized();
    view->SetShadingModel(V3d_PHONG);

    Graphic3d_RenderingParams &RenderParams = view->ChangeRenderingParams();
    RenderParams.IsAntialiasingEnabled = true;
    RenderParams.NbMsaaSamples = 8;
    RenderParams.IsShadowEnabled = false;
    RenderParams.CollectedStats = Graphic3d_RenderingParams::PerfCounters_NONE;
}

//////////////////////////////////////////////////////////////////////

LRESULT WINAPI occ_viewer::wnd_proc_proxy(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if(message == WM_CREATE) {
        CREATESTRUCTA *pCreateStruct = (CREATESTRUCTA *)lparam;
        SetWindowLongPtrA(hwnd, int(GWLP_USERDATA), (LONG_PTR)pCreateStruct->lpCreateParams);
    }
    occ_viewer *viewer = (occ_viewer *)GetWindowLongPtrA(hwnd, int(GWLP_USERDATA));
    if(viewer != NULL) {
        return viewer->wnd_proc(hwnd, message, wparam, lparam);
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

//////////////////////////////////////////////////////////////////////

LRESULT occ_viewer::wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if(view.IsNull()) {
        return DefWindowProc(hwnd, message, wparam, lparam);
    }

    switch(message) {
    case WM_PAINT: {
        PAINTSTRUCT aPaint;
        BeginPaint(hwnd, &aPaint);
        EndPaint(hwnd, &aPaint);
        viewer_interactor->ProcessExpose();
        break;
    }
    case WM_SIZE: {
        viewer_interactor->ProcessConfigure(Standard_True);
        break;
    }
    case WM_MOVE:
    case WM_MOVING:
    case WM_SIZING: {
        switch(view->RenderingParams().StereoMode) {
        case Graphic3d_StereoMode_RowInterlaced:
        case Graphic3d_StereoMode_ColumnInterlaced:
        case Graphic3d_StereoMode_ChessBoard: {
            // track window moves to reverse stereo pair
            view->MustBeResized();
            view->Update();
            break;
        }
        default:
            break;
        }
        break;
    }
    case WM_KEYUP:
    case WM_KEYDOWN: {
        const Aspect_VKey vkey = WNT_Window::VirtualKeyFromNative((int)wparam);

        if(vkey != Aspect_VKey_UNKNOWN) {
            const double timeStamp = viewer_interactor->EventTime();

            if(message == WM_KEYDOWN) {
                viewer_interactor->KeyDown(vkey, timeStamp);
            } else {
                viewer_interactor->KeyUp(vkey, timeStamp);
            }
        }
        break;
    }
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        const Graphic3d_Vec2i pos(LOWORD(lparam), HIWORD(lparam));
        const Aspect_VKeyFlags flags = WNT_Window::MouseKeyFlagsFromEvent(wparam);
        Aspect_VKeyMouse button = Aspect_VKeyMouse_NONE;
        //
        switch(message) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
            button = Aspect_VKeyMouse_LeftButton;
            break;
        case WM_MBUTTONUP:
        case WM_MBUTTONDOWN:
            button = Aspect_VKeyMouse_RightButton;
            break;
        case WM_RBUTTONUP:
        case WM_RBUTTONDOWN:
            button = Aspect_VKeyMouse_MiddleButton;
            break;
        }
        if(message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN) {
            SetFocus(hwnd);
            SetCapture(hwnd);

            if(!viewer_interactor.IsNull()) {
                viewer_interactor->PressMouseButton(pos, button, flags, false);
            }

        } else {

            ReleaseCapture();

            if(!viewer_interactor.IsNull()) {
                viewer_interactor->ReleaseMouseButton(pos, button, flags, false);
            }
        }
        viewer_interactor->FlushViewEvents(interactive_context, view, true);
        break;
    }
    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
        const double deltaF = double(delta) / double(WHEEL_DELTA);

        const Aspect_VKeyFlags flags = WNT_Window::MouseKeyFlagsFromEvent(wparam);

        Graphic3d_Vec2i pos(int(short(LOWORD(lparam))), int(short(HIWORD(lparam))));
        POINT cursorPnt = { pos.x(), pos.y() };
        if(ScreenToClient(hwnd, &cursorPnt)) {
            pos.SetValues(cursorPnt.x, cursorPnt.y);
        }

        if(!viewer_interactor.IsNull()) {
            viewer_interactor->UpdateMouseScroll(Aspect_ScrollDelta(pos, deltaF, flags));
            viewer_interactor->FlushViewEvents(interactive_context, view, true);
        }
        break;
    }
    case WM_MOUSEMOVE: {
        Graphic3d_Vec2i pos(LOWORD(lparam), HIWORD(lparam));
        Aspect_VKeyMouse buttons = WNT_Window::MouseButtonsFromEvent(wparam);
        Aspect_VKeyFlags flags = WNT_Window::MouseKeyFlagsFromEvent(wparam);

        // don't make a slide-show from input events - fetch the actual mouse cursor position
        CURSORINFO cursor{};
        cursor.cbSize = sizeof(cursor);
        if(::GetCursorInfo(&cursor) != FALSE) {
            POINT cursorPnt = { cursor.ptScreenPos.x, cursor.ptScreenPos.y };
            if(ScreenToClient(hwnd, &cursorPnt)) {
                // as we override mouse position, we need overriding also mouse state
                pos.SetValues(cursorPnt.x, cursorPnt.y);
                buttons = WNT_Window::MouseButtonsAsync();
                flags = WNT_Window::MouseKeyFlagsAsync();
            }
        }

        if(wnt_window.IsNull() || (HWND)wnt_window->HWindow() != hwnd) {
            // mouse move events come also for inactive windows
            break;
        }

        if(!viewer_interactor.IsNull()) {
            viewer_interactor->UpdateMousePosition(pos, buttons, flags, false);
            viewer_interactor->FlushViewEvents(interactive_context, view, true);
        }
        break;
    }
    default: {
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}
