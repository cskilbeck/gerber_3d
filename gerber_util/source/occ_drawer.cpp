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
#include <ShapeFix_Shape.hxx>

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
        ShapeFix_Shape fixer(tool_face);
        fixer.Perform();

        BOPAlgo_BOP builder;
        builder.SetOperation(operation);
        builder.AddArgument(final_face);
        builder.AddTool(fixer.Shape());
        builder.Perform();

        ShapeUpgrade_UnifySameDomain unify_final;
        unify_final.Initialize(builder.Shape());
        unify_final.Build();
        final_face = unify_final.Shape();
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

    void occ_drawer::set_gerber(gerber *g)
    {
        g->draw(*this);

        // deal with the last current_face which might be dangling

        if(main_face.IsNull()) {
            main_face = current_face;
        } else if(!current_face.IsNull()) {
            // add or remove it to/from main_face
            if(previous_fill) {
                add_face(current_face, main_face);
            } else {
                remove_face(current_face, main_face);
            }
        }
        if(!main_face.IsNull()) {

            double const depth = 0.5;
            // double const depth = 1.0;

            gerber_timer t;

            LOG_DEBUG("BRepPrimAPI_MakePrism begins");
            t.reset();
            BRepPrimAPI_MakePrism prism(main_face, gp_Vec(0, 0, depth));
            prism.Build();

            vout.add_shape(prism.Shape());
            LOG_DEBUG("BRepPrimAPI_MakePrism complete, took {:7.2} seconds", t.elapsed_seconds());
            gerber_file = g;
            previous_fill = false;
            current_fill = false;
            current_face.Nullify();
            main_face.Nullify();
        }
    }

    //////////////////////////////////////////////////////////////////////

    void occ_drawer::on_gerber_finished()
    {
        vout.add_shapes_to_scene();
        InvalidateRect(vout.hwnd, nullptr, false);
    }

    //////////////////////////////////////////////////////////////////////

    void occ_drawer::fill_elements(gerber_draw_element const *elements, size_t num_elements, gerber_polarity polarity, int entity_id)
    {
        LOG_CONTEXT("fill_elements", info);

        BRepBuilderAPI_MakeWire wire;

        LOG_DEBUG("FILL: entity {}, {} elements, polarity {}", entity_id, num_elements, polarity);

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
                if(end < start) {
                    std::swap(start, end);
                }
                gp_Circ circle;
                circle.SetLocation(gp_Pnt(e.arc_center.x, e.arc_center.y, 0));
                circle.SetRadius(e.radius);
                wire.Add(BRepBuilderAPI_MakeEdge(circle, start, end));
            } break;
            }
        }
        wire.Build();
        if(wire.IsDone()) {

            // build up a face while the polarity is the same

            BRepBuilderAPI_MakeFace wire_face(wire);
            TopoDS_Face new_face = wire_face.Face();

            current_fill = polarity == polarity_dark || polarity == polarity_positive;

            if(current_face.IsNull()) {
                current_face = new_face;

            } else if(current_fill != previous_fill) {

                // add or remove it to/from main_face
                if(previous_fill) {
                    if(main_face.IsNull()) {
                        main_face = current_face;
                    } else {
                        add_face(current_face, main_face);
                    }
                } else {
                    remove_face(current_face, main_face);
                }
                // now the current face is what we just filled
                current_face = new_face;
            } else {
                add_face(new_face, current_face);
            }
            previous_fill = current_fill;
        }
    }

}    // namespace gerber_3d
