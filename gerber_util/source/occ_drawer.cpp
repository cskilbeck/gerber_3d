#include "occ_drawer.h"

#include "gerber_lib.h"

#include <AIS_Shape.hxx>
#include <gp.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>
#include <gp_Circ.hxx>
#include <TopoDS.hxx>
#include <BRepTools.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <BOPAlgo_BOP.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepAlgoAPI_Fuse.hxx>

LOG_CONTEXT("OCC", debug);

namespace
{
    //////////////////////////////////////////////////////////////////////

    void dump_alerts(char const *banner, BOPAlgo_Options const &builder)
    {
        auto dump = [&](int x, char const *severity) {
            Message_ListOfAlert const &alerts = builder.GetReport()->GetAlerts(static_cast<Message_Gravity>(x));
            if(alerts.Size() != 0) {
                LOG_DEBUG(" {:10s}: {} alerts for {}", severity, alerts.Size(), banner);
                for(Message_ListOfAlert::Iterator anIt(alerts); anIt.More(); anIt.Next()) {
                    Message_Alert *m = anIt.Value().get();
                    auto y = m->GetMessageKey();
                    LOG_DEBUG("----> {}", y);
                }
            }
        };

        dump(Message_Gravity::Message_Trace, "Trace");
        dump(Message_Gravity::Message_Info, "Info");
        dump(Message_Gravity::Message_Warning, "Warning");
        dump(Message_Gravity::Message_Alarm, "Alarm");
        dump(Message_Gravity::Message_Fail, "Fail");
    }

    //////////////////////////////////////////////////////////////////////

    void dump_shape(TopoDS_Shape const &shape, int indent)
    {
        std::string indent_str(indent, ' ');
        LOG_INFO("{}Shape is a {}", indent_str, (int)shape.ShapeType());
        int index = 0;
        for(TopoDS_Iterator face_iterator(shape); face_iterator.More(); face_iterator.Next()) {
            LOG_INFO("{}Subshape {} is a {}", indent_str, index, (int)(face_iterator.Value().ShapeType()));
            index += 1;
            dump_shape(face_iterator.Value(), indent + 4);
        }
    }

    //////////////////////////////////////////////////////////////////////

    void boolean_face(TopoDS_Shape const &tool_face, TopoDS_Shape &final_face, BOPAlgo_Operation operation)
    {
        BOPAlgo_BOP builder;
        builder.SetOperation(operation);
        builder.AddArgument(final_face);
        builder.AddTool(tool_face);
        builder.Perform();
        final_face = builder.Shape();
        // dump_shape(final_face, 0);
    }

    //////////////////////////////////////////////////////////////////////
    // Merge a face into another face

    void add_face(TopoDS_Shape const &face_to_add, TopoDS_Shape &final_face)
    {
        if(final_face.IsNull()) {
            final_face = face_to_add;
            return;
        }
        boolean_face(face_to_add, final_face, BOPAlgo_FUSE);
    }

    //////////////////////////////////////////////////////////////////////
    // remove one face from another

    void remove_face(TopoDS_Shape const &face_to_remove, TopoDS_Shape &final_face)
    {
        if(final_face.IsNull()) {
            return;
        }
        boolean_face(face_to_remove, final_face, BOPAlgo_CUT);
    }

}    // namespace

namespace gerber_3d
{
    using namespace gerber_lib;

    //////////////////////////////////////////////////////////////////////

    void occ_drawer::create_window(int x, int y, int w, int h)
    {
        vout.create_window(x, y, w, h);
    }

    //////////////////////////////////////////////////////////////////////

    void occ_drawer::set_gerber(gerber *g, int hide_elements)
    {
        elements_to_hide = hide_elements;

        g->draw(*this, elements_to_hide);

        double const depth = 0.5;
        // double const depth = 1.0;

        if(!main_face.IsNull()) {

            ShapeUpgrade_UnifySameDomain unify;
            unify.Initialize(main_face);
            unify.Build();
            main_face = unify.Shape();

            BRepPrimAPI_MakePrism prism(main_face, gp_Vec(0, 0, depth));
            prism.Build();

            vout.add_shape(prism.Shape());
            gerber_file = g;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void occ_drawer::on_gerber_finished()
    {
        vout.add_shapes_to_scene();
        InvalidateRect(vout.hwnd, nullptr, false);
    }

    //////////////////////////////////////////////////////////////////////

    void occ_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity)
    {
        LOG_CONTEXT("fill_elements", none);

        BRepBuilderAPI_MakeWire wire;

        LOG_DEBUG("FILL: net {}, {} elements, polarity {}", current_net_id, num_elements, polarity);

        gp_Ax2 up_axis2;

        for(size_t i = 0; i < num_elements; ++i) {

            gerber_draw_element const &e = elements[i];
            LOG_DEBUG("[{}]={}", i, e);

            switch(e.draw_element_type) {

            case draw_element_line: {

                BRepBuilderAPI_MakeEdge edge(gp_Pnt(e.line_end.x, e.line_end.y, 0), gp_Pnt(e.line_start.x, e.line_start.y, 0));
                wire.Add(edge);

            } break;

            case draw_element_arc: {
                double start = deg_2_rad(e.start_degrees);
                double end = deg_2_rad(e.end_degrees);
                gp_Circ circle;
                circle.SetLocation(gp_Pnt(e.arc_center.x, e.arc_center.y, 0));
                circle.SetRadius(e.radius);
                wire.Add(BRepBuilderAPI_MakeEdge(circle, start, end));
            } break;
            }
        }
        wire.Build();
        if(wire.IsDone()) {

            BRepBuilderAPI_MakeFace wire_face(wire);
            TopoDS_Face the_face = wire_face.Face();

            switch(polarity) {

            case polarity_negative:
            case polarity_clear:
                remove_face(the_face, main_face);
                break;

            case polarity_dark:
            case polarity_positive:
                add_face(the_face, main_face);
                break;
            }
        }
    }

}    // namespace gerber_3d
