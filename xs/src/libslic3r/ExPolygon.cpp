#include "BoundingBox.hpp"
#include "ExPolygon.hpp"
#include "Geometry.hpp"
#include "Polygon.hpp"
#include "Line.hpp"
#include "ClipperUtils.hpp"
#include "SVG.hpp"
#include "polypartition.h"
#include "poly2tri/poly2tri.h"
#include <algorithm>
#include <cassert>
#include <list>

namespace Slic3r {

ExPolygon::operator Points() const
{
    Points points;
    Polygons pp = *this;
    for (Polygons::const_iterator poly = pp.begin(); poly != pp.end(); ++poly) {
        for (Points::const_iterator point = poly->points.begin(); point != poly->points.end(); ++point)
            points.push_back(*point);
    }
    return points;
}

ExPolygon::operator Polygons() const
{
    return to_polygons(*this);
}

ExPolygon::operator Polylines() const
{
    return to_polylines(*this);
}

void
ExPolygon::scale(double factor)
{
    contour.scale(factor);
    for (Polygons::iterator it = holes.begin(); it != holes.end(); ++it) {
        (*it).scale(factor);
    }
}

void
ExPolygon::translate(double x, double y)
{
    contour.translate(x, y);
    for (Polygons::iterator it = holes.begin(); it != holes.end(); ++it) {
        (*it).translate(x, y);
    }
}

void
ExPolygon::rotate(double angle)
{
    contour.rotate(angle);
    for (Polygons::iterator it = holes.begin(); it != holes.end(); ++it) {
        (*it).rotate(angle);
    }
}

void
ExPolygon::rotate(double angle, const Point &center)
{
    contour.rotate(angle, center);
    for (Polygons::iterator it = holes.begin(); it != holes.end(); ++it) {
        (*it).rotate(angle, center);
    }
}

double
ExPolygon::area() const
{
    double a = this->contour.area();
    for (Polygons::const_iterator it = this->holes.begin(); it != this->holes.end(); ++it) {
        a -= -(*it).area();  // holes have negative area
    }
    return a;
}

bool
ExPolygon::is_valid() const
{
    if (!this->contour.is_valid() || !this->contour.is_counter_clockwise()) return false;
    for (Polygons::const_iterator it = this->holes.begin(); it != this->holes.end(); ++it) {
        if (!(*it).is_valid() || (*it).is_counter_clockwise()) return false;
    }
    return true;
}

bool
ExPolygon::contains(const Line &line) const
{
    return this->contains((Polyline)line);
}

bool
ExPolygon::contains(const Polyline &polyline) const
{
    return diff_pl((Polylines)polyline, *this).empty();
}

bool
ExPolygon::contains(const Polylines &polylines) const
{
    #if 0
    BoundingBox bbox = get_extents(polylines);
    bbox.merge(get_extents(*this));
    SVG svg(debug_out_path("ExPolygon_contains.svg"), bbox);
    svg.draw(*this);
    svg.draw_outline(*this);
    svg.draw(polylines, "blue");
    #endif
    Polylines pl_out = diff_pl(polylines, *this);
    #if 0
    svg.draw(pl_out, "red");
    #endif
    return pl_out.empty();
}

bool
ExPolygon::contains(const Point &point) const
{
    if (!this->contour.contains(point)) return false;
    for (Polygons::const_iterator it = this->holes.begin(); it != this->holes.end(); ++it) {
        if (it->contains(point)) return false;
    }
    return true;
}

// inclusive version of contains() that also checks whether point is on boundaries
bool
ExPolygon::contains_b(const Point &point) const
{
    return this->contains(point) || this->has_boundary_point(point);
}

bool
ExPolygon::has_boundary_point(const Point &point) const
{
    if (this->contour.has_boundary_point(point)) return true;
    for (Polygons::const_iterator h = this->holes.begin(); h != this->holes.end(); ++h) {
        if (h->has_boundary_point(point)) return true;
    }
    return false;
}

bool
ExPolygon::overlaps(const ExPolygon &other) const
{
    #if 0
    BoundingBox bbox = get_extents(other);
    bbox.merge(get_extents(*this));
    static int iRun = 0;
    SVG svg(debug_out_path("ExPolygon_overlaps-%d.svg", iRun ++), bbox);
    svg.draw(*this);
    svg.draw_outline(*this);
    svg.draw_outline(other, "blue");
    #endif
    Polylines pl_out = intersection_pl((Polylines)other, *this);
    #if 0
    svg.draw(pl_out, "red");
    #endif
    if (! pl_out.empty())
        return true; 
    return ! other.contour.points.empty() && this->contains_b(other.contour.points.front());
}

void ExPolygon::simplify_p(double tolerance, Polygons* polygons) const
{
    Polygons pp = this->simplify_p(tolerance);
    polygons->insert(polygons->end(), pp.begin(), pp.end());
}

Polygons ExPolygon::simplify_p(double tolerance) const
{
    Polygons pp;
    pp.reserve(this->holes.size() + 1);
    // contour
    {
        Polygon p = this->contour;
        p.points.push_back(p.points.front());
        p.points = MultiPoint::_douglas_peucker(p.points, tolerance);
        p.points.pop_back();
        pp.emplace_back(std::move(p));
    }
    // holes
    for (Polygon p : this->holes) {
        p.points.push_back(p.points.front());
        p.points = MultiPoint::_douglas_peucker(p.points, tolerance);
        p.points.pop_back();
        pp.emplace_back(std::move(p));
    }
    return simplify_polygons(pp);
}

ExPolygons ExPolygon::simplify(double tolerance) const
{
    return union_ex(this->simplify_p(tolerance));
}

void ExPolygon::simplify(double tolerance, ExPolygons* expolygons) const
{
    append(*expolygons, this->simplify(tolerance));
}

void
ExPolygon::medial_axis(const ExPolygon &bounds, double max_width, double min_width, ThickPolylines* polylines) const
{
    // init helper object
    Slic3r::Geometry::MedialAxis ma(max_width, min_width, this);
    ma.lines = this->lines();
    
    // compute the Voronoi diagram and extract medial axis polylines
    ThickPolylines pp;
    ma.build(&pp);
    


    for (Lines lines : ma.lines){
        cout << "lines " << lines.size() << ": ";
        for (Line line : lines){
            cout << ",  " << unscale(line.a.x) << ":" << unscale(line.a.y) << "->" << unscale(line.b.x) << ":" << unscale(line.b.y);
        }
    }
    cout << "\n";
    /*
    SVG svg("medial_axis.svg");
    svg.draw(*this);
    svg.draw(pp);
    svg.Close();
    */
    
    /* Find the maximum width returned; we're going to use this for validating and 
       filtering the output segments. */
    double max_w = 0;
    for (ThickPolylines::const_iterator it = pp.begin(); it != pp.end(); ++it)
        max_w = fmaxf(max_w, *std::max_element(it->width.begin(), it->width.end()));

    /*  Aligned fusion: Fusion the bits at the end of lines by "increasing thikness"
    *   For that, we have to find other lines,
    *   and with a next point no more distant than the max width.
    *   Then, we can merge the bit from the first point to the second by following the mean.
    */
    bool changes = true;
    while (changes) {
        changes = false;
        for (size_t i = 0; i < pp.size(); ++i) {
            ThickPolyline& polyline = pp[i];
            if (polyline.endpoints.first && polyline.endpoints.second) continue; // optimization

            ThickPolyline* best_candidate = nullptr;
            float best_dot = -1;
            int best_idx = 0;

            // find another polyline starting here
            for (size_t j = i + 1; j < pp.size(); ++j) {
                ThickPolyline& other = pp[j];
                if (polyline.last_point().coincides_with(other.last_point())) {
                    std::cout << "VOROFUSION av " << unscale(polyline.first_point().x) << ":" << unscale(polyline.first_point().y) << "\n";
                    polyline.reverse();
                    other.reverse();
                }
                else if (polyline.first_point().coincides_with(other.last_point())) {
                    std::cout << "VOROFUSION ov " << unscale(polyline.last_point().x) << ":" << unscale(polyline.last_point().y) << "\n";
                    other.reverse();
                }
                else if (polyline.first_point().coincides_with(other.first_point())) {
                    std::cout << "VOROFUSION ok " << unscale(polyline.last_point().x) << ":" << unscale(polyline.last_point().y) << "\n";
                }
                else if (polyline.last_point().coincides_with(other.first_point())) {
                    std::cout << "VOROFUSION mv " << unscale(polyline.first_point().x) << ":" << unscale(polyline.first_point().y) << "\n";
                    polyline.reverse();
                } else {
                    continue;
                }

                //only consider the other if the next point is near us
                if (polyline.points.size() < 2 && other.points.size() < 2) continue;
                std::cout << "ok to move? : !" << polyline.endpoints.second << " && !" << other.endpoints.second << "\n";
                if (!polyline.endpoints.second || !other.endpoints.second) continue;
                if (polyline.points.back().distance_to(other.points.back()) > max_width) {
                    std::cout << "too far apart : " << polyline.points.back().distance_to(other.points.back())<< " > "<< max_width << "\n";
                    continue;
                } else {
                    std::cout << "dist ok : " << polyline.points.back().distance_to(other.points.back()) << " < " << max_width << "\n";
                }
                if (polyline.points.size() != other.points.size()) {
                    std::cout << "not same nbPoints : " << polyline.points.size() << " != " << other.points.size() <<"\n";
                    continue;
                }

                std::cout << "VOROFUSION (1) ThickPolyline " << polyline.points.size() << ": ";
                for (unsigned int i = 0; i < polyline.points.size() && i < 10; i++) {
                    std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
                }
                std::cout << "\n";
                std::cout << "VOROFUSION (2) ThickPolyline " << other.points.size() << ": ";
                for (unsigned int i = 0; i < other.points.size() && i < 10; i++) {
                    std::cout << "->" << unscale(other.points[i].x) << ":" << unscale(other.points[i].y);
                }
                std::cout << "\n";

                Pointf v_poly(polyline.lines().front().vector().x, polyline.lines().front().vector().y);
                std::cout << "my vect (before norm: " << (v_poly.x) << " : " << (v_poly.y) << "\n";
                std::cout << "my vect (before norm: " << unscale(v_poly.x) << " : " << unscale(v_poly.y) << "\n";
                std::cout << "my vect ( norm: " << std::sqrt(v_poly.x*v_poly.x + v_poly.y*v_poly.y) << "\n";
                v_poly.scale(1 / std::sqrt(v_poly.x*v_poly.x + v_poly.y*v_poly.y));
                std::cout << "my vect: " << v_poly.x << " : " << v_poly.y << "\n";
                Pointf v_other(other.lines().front().vector().x, other.lines().front().vector().y);
                std::cout << "his vect (before norm: " << v_other.x << " : " << v_other.y << "\n";
                std::cout << "his vect (before norm: " << unscale(v_other.x) << " : " << unscale(v_other.y) << "\n";
                std::cout << "his vect ( norm: " << std::sqrt(v_other.x*v_other.x + v_other.y*v_other.y) << "\n";
                v_other.scale(1 / std::sqrt(v_other.x*v_other.x + v_other.y*v_other.y));
                std::cout << "his vect: " << v_other.x << " : " << v_other.y << "\n";
                float other_dot = v_poly.x*v_other.x + v_poly.y*v_other.y;
                std::cout << "VOROFUSION dot " << other_dot << " >? " << best_dot << "\n";
                if (other_dot > best_dot) {
                    std::cout << "VOROFUSION WIN: \n";
                    best_candidate = &other;
                    best_idx = j;
                    best_dot = other_dot;
                }
            }
            std::cout << "VOROFUSION search ended: \n";
            if (best_candidate != nullptr) {
                std::cout << "VOROFUSION launched: \n";

                //TODO: witch if polyline.size > best_candidate->size
                //doesn't matter rright now because a if in the selection process prevent this.

                //iterate the points
                // as voronoi should create symetric thing, we can iterate synchonously
                unsigned int idx_point = 1;
                std::cout << "start fusion " << idx_point << " < " << polyline.points.size() << " & " << best_candidate->points.size() << " \n";
                std::cout << "start fusion " << idx_point << " < " << polyline.width.size() << " & " << best_candidate->width.size() << " \n";
                while (idx_point < polyline.points.size() && polyline.points[idx_point].distance_to(best_candidate->points[idx_point]) < max_width) {
                    std::cout << "fusion " << idx_point << " < " << polyline.points.size() << " & " << best_candidate->points.size()<<" \n";
                    //fusion
                    polyline.points[idx_point].x += best_candidate->points[idx_point].x;
                    polyline.points[idx_point].x /= 2;
                    polyline.points[idx_point].y += best_candidate->points[idx_point].y;
                    polyline.points[idx_point].y /= 2;
                    polyline.width[idx_point] += best_candidate->width[idx_point];
                    ++idx_point;
                }
                std::cout << "fusion finishing: \n";
                if (idx_point < best_candidate->points.size()) {
                    if (idx_point + 1 < best_candidate->points.size()) {
                        std::cout << "fusion create new poly: \n";
                        //create a new polyline
                        pp.emplace_back();
                        pp.back().endpoints.first = true;
                        pp.back().endpoints.second = best_candidate->endpoints.second;
                        for (int idx_point_new_line = idx_point; idx_point_new_line < best_candidate->points.size(); ++idx_point_new_line) {
                            pp.back().points.push_back(best_candidate->points[idx_point_new_line]);
                            pp.back().width.push_back(best_candidate->width[idx_point_new_line]);
                        }
                        std::cout << "VOROFUSION (pp) " << pp.back().points.size() << ": ";
                        for (unsigned int i = 0; i < pp.back().points.size() && i < 10; i++) {
                            std::cout << "->" << unscale(pp.back().points[i].x) << ":" << unscale(pp.back().points[i].y);
                        }
                    } else {
                        std::cout << "fusion add last point: \n";
                        //Add last point
                        polyline.points.push_back(best_candidate->points[idx_point]);
                        polyline.width.push_back(best_candidate->width[idx_point]);
                        //select if an end opccur
                        polyline.endpoints.second &= best_candidate->endpoints.second;
                    }

                } else {
                    //select if an end opccur
                    polyline.endpoints.second &= best_candidate->endpoints.second;
                }

                //remove points that are the same or too close each other, ie simplify
                for (unsigned int idx_point = 1; idx_point < polyline.points.size(); ++idx_point) {
                    //distance of 1 is on the sclaed coordinates, so it correspond to SCALE_FACTOR, so it's very small
                    if (polyline.points[idx_point - 1].distance_to(polyline.points[idx_point]) < 1) {
                        std::cout << "fusion erase duplicate@" << idx_point<< " \n";
                        if (idx_point < polyline.points.size() -1) {
                            polyline.points.erase(polyline.points.begin() + idx_point);
                        } else {
                            polyline.points.erase(polyline.points.begin() + idx_point -1);
                        }
                        --idx_point;
                    }
                }
                //remove points that are outside of the geometry
                for (unsigned int idx_point = 0; idx_point < polyline.points.size(); ++idx_point) {
                    //distance of 1 is on the sclaed coordinates, so it correspond to SCALE_FACTOR, so it's very small
                    if (!bounds.contains_b(polyline.points[idx_point])) {
                        std::cout << "erase outside point@" << idx_point << " \n";
                        polyline.points.erase(polyline.points.begin() + idx_point);
                        --idx_point;
                    }
                }
                if (polyline.points.size() < 2) {
                    //remove self
                    pp.erase(pp.begin() + i);
                    --i;
                    --best_idx;
                }


                std::cout << "fusion erase second line: \n";
                pp.erase(pp.begin() + best_idx);

                std::cout << "VOROFUSION (res) ThickPolyline " << polyline.points.size() << ": ";
                for (int i = 0; i < polyline.points.size() && i < 10; i++) {
                    std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
                }
                std::cout << "\n";
                changes = true;
            }
        }
    }


    /* Loop through all returned polylines in order to extend their endpoints to the 
       expolygon boundaries */
    bool removed = false;
    for (size_t i = 0; i < pp.size(); ++i) {
        ThickPolyline& polyline = pp[i];
        std::cout << "ThickPolyline " << polyline.points.size() << ": ";
        for (int i = 0; i < polyline.points.size() && i < 10; i++){
            std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
        }
        std::cout << "\n";

        // extend initial and final segments of each polyline if they're actual endpoints
        /* We assign new endpoints to temporary variables because in case of a single-line
           polyline, after we extend the start point it will be caught by the intersection()
           call, so we keep the inner point until we perform the second intersection() as well */
        Point new_front = polyline.points.front();
        Point new_back = polyline.points.back();
        std::cout << "ThickPolyline start" << polyline.endpoints.first << " && !"<<bounds.has_boundary_point(new_front) << "?\n";
        if (polyline.endpoints.first && !bounds.has_boundary_point(new_front)) {
            std::cout << "ThickPolyline extend(1) " << bounds.contains(new_front) << "?\n";
            Line line(polyline.points.front(), polyline.points[1]);

            // prevent the line from touching on the other side, otherwise intersection() might return that solution
            if (polyline.points.size() == 2) line.b = line.midpoint();

            line.extend_start(max_width);
            (void)bounds.contour.intersection(line, &new_front);
            std::cout << "NEW 1 ThickPolyline " << polyline.points.size() << ": ";
            for (int i = 0; i < polyline.points.size() && i < 10; i++){
                std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
            }
            std::cout << "\n";
        }
        std::cout << "ThickPolyline end " << polyline.endpoints.second << " && !" << bounds.has_boundary_point(new_back) << "?\n";
        if (polyline.endpoints.second && !bounds.has_boundary_point(new_back)) {
            std::cout << "ThickPolyline extend(1)" << bounds.contains(new_back) << "?\n";
            Line line(
                *(polyline.points.end() - 2),
                polyline.points.back()
                );

            // prevent the line from touching on the other side, otherwise intersection() might return that solution
            if (polyline.points.size() == 2) line.a = line.midpoint();
            line.extend_end(max_width);

            (void)bounds.contour.intersection(line, &new_back);
            std::cout << "NEW 2 ThickPolyline " << polyline.points.size() << ": ";
            for (int i = 0; i < polyline.points.size() && i < 10; i++){
                std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
            }
            std::cout << "\n";
        }
        polyline.points.front() = new_front;
        polyline.points.back() = new_back;

    }



    /*  If we removed any short polylines we now try to connect consecutive polylines
        in order to allow loop detection. Note that this algorithm is greedier than 
        MedialAxis::process_edge_neighbors() as it will connect random pairs of 
        polylines even when more than two start from the same point. This has no 
        drawbacks since we optimize later using nearest-neighbor which would do the 
        same, but should we use a more sophisticated optimization algorithm we should
        not connect polylines when more than two meet.  */
    if (true || removed) {
        for (size_t i = 0; i < pp.size(); ++i) {
            ThickPolyline& polyline = pp[i];
            if (polyline.endpoints.first && polyline.endpoints.second) continue; // optimization
            
            ThickPolyline* best_candidate = nullptr;
            float best_dot = -1;
            int best_idx = 0;

            // find another polyline starting here
            for (size_t j = i + 1; j < pp.size(); ++j) {
                ThickPolyline& other = pp[j];
                if (polyline.last_point().coincides_with(other.last_point())) {
                    other.reverse();
                }
                else if (polyline.first_point().coincides_with(other.last_point())) {
                    polyline.reverse();
                    other.reverse();
                }
                else if (polyline.first_point().coincides_with(other.first_point())) {
                    polyline.reverse();
                }
                else if (!polyline.last_point().coincides_with(other.first_point())) {
                    continue;
                }
                std::cout << "FUSION (1) ThickPolyline " << polyline.points.size() << ": ";
                for (int i = 0; i < polyline.points.size() && i < 10; i++){
                    std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
                }
                std::cout << "\n";
                std::cout << "FUSION (2) ThickPolyline " << other.points.size() << ": ";
                for (int i = 0; i < other.points.size() && i < 10; i++){
                    std::cout << "->" << unscale(other.points[i].x) << ":" << unscale(other.points[i].y);
                }
                std::cout << "\n";
                std::cout << "FUSION (1 again) ThickPolyline " << polyline.points.size() << ": ";
                for (int i = 0; i < polyline.points.size() && i < 10; i++) {
                    std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
                }
                std::cout << "\n";

                Pointf v_poly(polyline.lines().back().vector().x, polyline.lines().back().vector().y);
                std::cout << "my vect (before norm: " << (v_poly.x) << " : " << (v_poly.y) << "\n";
                std::cout << "my vect ( norm: " << std::sqrt(v_poly.x*v_poly.x + v_poly.y*v_poly.y) << "\n";
                v_poly.scale(1 / std::sqrt(v_poly.x*v_poly.x + v_poly.y*v_poly.y));
                std::cout << "his vect: " << v_poly.x << " : " << v_poly.y << "\n";
                Pointf v_other(other.lines().front().vector().x, other.lines().front().vector().y);
                std::cout << "his vect (before norm: " << v_other.x << " : " << v_other.y << "\n";
                std::cout << "his vect ( norm: " << std::sqrt(v_other.x*v_other.x + v_other.y*v_other.y) << "\n";
                v_other.scale(1 / std::sqrt(v_other.x*v_other.x + v_other.y*v_other.y));
                std::cout << "his vect: " << v_other.x << " : " << v_other.y << "\n";
                float other_dot = v_poly.x*v_other.x + v_poly.y*v_other.y;
                std::cout << "FUSION dot " << other_dot << " >? " << best_dot << "\n";
                if (other_dot > best_dot) {
                    best_candidate = &other;
                    best_idx = j;
                    best_dot = other_dot;
                }
            }
            if (best_candidate != nullptr) {

                polyline.points.insert(polyline.points.end(), best_candidate->points.begin() + 1, best_candidate->points.end());
                polyline.width.insert(polyline.width.end(), best_candidate->width.begin(), best_candidate->width.end());
                polyline.endpoints.second = best_candidate->endpoints.second;
                assert(polyline.width.size() == polyline.points.size()*2 - 2);
                
                pp.erase(pp.begin () + best_idx);
                std::cout << "FUSION (res) ThickPolyline " << polyline.points.size() << ": ";
                for (int i = 0; i < polyline.points.size() && i < 10; i++) {
                    std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
                }
                std::cout << "\n";
            }
        }
    }

    for (size_t i = 0; i < pp.size(); ++i) {
        ThickPolyline& polyline = pp[i];

        std::cout << "END ThickPolyline " << polyline.points.size() << ": ";
        for (int i = 0; i < polyline.points.size() && i < 10; i++){
            std::cout << "->" << unscale(polyline.points[i].x) << ":" << unscale(polyline.points[i].y);
        }
        std::cout << "\n";

        /*  remove too short polylines
        (we can't do this check before endpoints extension and clipping because we don't
        know how long will the endpoints be extended since it depends on polygon thickness
        which is variable - extension will be <= max_width/2 on each side)  */
        //FIXME: try to merge it with an other thick polyline before erasing it.
        if ((polyline.endpoints.first || polyline.endpoints.second)
            && polyline.length() < max_w * 2) {
            pp.erase(pp.begin() + i);
            --i;
            removed = true;
            std::cout << "ThickPolyline DELETED!!\n";
            continue;
        }

    }

    
    polylines->insert(polylines->end(), pp.begin(), pp.end());
}

void
ExPolygon::medial_axis(double max_width, double min_width, Polylines* polylines) const
{
    ThickPolylines tp;
    this->medial_axis(*this, max_width, min_width, &tp);
    polylines->insert(polylines->end(), tp.begin(), tp.end());
}

void
ExPolygon::get_trapezoids(Polygons* polygons) const
{
    ExPolygons expp;
    expp.push_back(*this);
    boost::polygon::get_trapezoids(*polygons, expp);
}

void
ExPolygon::get_trapezoids(Polygons* polygons, double angle) const
{
    ExPolygon clone = *this;
    clone.rotate(PI/2 - angle, Point(0,0));
    clone.get_trapezoids(polygons);
    for (Polygons::iterator polygon = polygons->begin(); polygon != polygons->end(); ++polygon)
        polygon->rotate(-(PI/2 - angle), Point(0,0));
}

// This algorithm may return more trapezoids than necessary
// (i.e. it may break a single trapezoid in several because
// other parts of the object have x coordinates in the middle)
void
ExPolygon::get_trapezoids2(Polygons* polygons) const
{
    // get all points of this ExPolygon
    Points pp = *this;
    
    // build our bounding box
    BoundingBox bb(pp);
    
    // get all x coordinates
    std::vector<coord_t> xx;
    xx.reserve(pp.size());
    for (Points::const_iterator p = pp.begin(); p != pp.end(); ++p)
        xx.push_back(p->x);
    std::sort(xx.begin(), xx.end());
    
    // find trapezoids by looping from first to next-to-last coordinate
    for (std::vector<coord_t>::const_iterator x = xx.begin(); x != xx.end()-1; ++x) {
        coord_t next_x = *(x + 1);
        if (*x == next_x) continue;
        
        // build rectangle
        Polygon poly;
        poly.points.resize(4);
        poly[0].x = *x;
        poly[0].y = bb.min.y;
        poly[1].x = next_x;
        poly[1].y = bb.min.y;
        poly[2].x = next_x;
        poly[2].y = bb.max.y;
        poly[3].x = *x;
        poly[3].y = bb.max.y;
        
        // intersect with this expolygon
        // append results to return value
        polygons_append(*polygons, intersection(poly, to_polygons(*this)));
    }
}

void
ExPolygon::get_trapezoids2(Polygons* polygons, double angle) const
{
    ExPolygon clone = *this;
    clone.rotate(PI/2 - angle, Point(0,0));
    clone.get_trapezoids2(polygons);
    for (Polygons::iterator polygon = polygons->begin(); polygon != polygons->end(); ++polygon)
        polygon->rotate(-(PI/2 - angle), Point(0,0));
}

// While this triangulates successfully, it's NOT a constrained triangulation
// as it will create more vertices on the boundaries than the ones supplied.
void
ExPolygon::triangulate(Polygons* polygons) const
{
    // first make trapezoids
    Polygons trapezoids;
    this->get_trapezoids2(&trapezoids);
    
    // then triangulate each trapezoid
    for (Polygons::iterator polygon = trapezoids.begin(); polygon != trapezoids.end(); ++polygon)
        polygon->triangulate_convex(polygons);
}

void
ExPolygon::triangulate_pp(Polygons* polygons) const
{
    // convert polygons
    std::list<TPPLPoly> input;
    
    ExPolygons expp = union_ex(simplify_polygons(to_polygons(*this), true));
    
    for (ExPolygons::const_iterator ex = expp.begin(); ex != expp.end(); ++ex) {
        // contour
        {
            TPPLPoly p;
            p.Init(int(ex->contour.points.size()));
            //printf(PRINTF_ZU "\n0\n", ex->contour.points.size());
            for (Points::const_iterator point = ex->contour.points.begin(); point != ex->contour.points.end(); ++point) {
                p[ point-ex->contour.points.begin() ].x = point->x;
                p[ point-ex->contour.points.begin() ].y = point->y;
                //printf("%ld %ld\n", point->x, point->y);
            }
            p.SetHole(false);
            input.push_back(p);
        }
    
        // holes
        for (Polygons::const_iterator hole = ex->holes.begin(); hole != ex->holes.end(); ++hole) {
            TPPLPoly p;
            p.Init(hole->points.size());
            //printf(PRINTF_ZU "\n1\n", hole->points.size());
            for (Points::const_iterator point = hole->points.begin(); point != hole->points.end(); ++point) {
                p[ point-hole->points.begin() ].x = point->x;
                p[ point-hole->points.begin() ].y = point->y;
                //printf("%ld %ld\n", point->x, point->y);
            }
            p.SetHole(true);
            input.push_back(p);
        }
    }
    
    // perform triangulation
    std::list<TPPLPoly> output;
    int res = TPPLPartition().Triangulate_MONO(&input, &output);
    if (res != 1) CONFESS("Triangulation failed");
    
    // convert output polygons
    for (std::list<TPPLPoly>::iterator poly = output.begin(); poly != output.end(); ++poly) {
        long num_points = poly->GetNumPoints();
        Polygon p;
        p.points.resize(num_points);
        for (long i = 0; i < num_points; ++i) {
            p.points[i].x = coord_t((*poly)[i].x);
            p.points[i].y = coord_t((*poly)[i].y);
        }
        polygons->push_back(p);
    }
}

void
ExPolygon::triangulate_p2t(Polygons* polygons) const
{
    ExPolygons expp = simplify_polygons_ex(*this, true);
    
    for (ExPolygons::const_iterator ex = expp.begin(); ex != expp.end(); ++ex) {
        // TODO: prevent duplicate points

        // contour
        std::vector<p2t::Point*> ContourPoints;
        for (Points::const_iterator point = ex->contour.points.begin(); point != ex->contour.points.end(); ++point) {
            // We should delete each p2t::Point object
            ContourPoints.push_back(new p2t::Point(point->x, point->y));
        }
        p2t::CDT cdt(ContourPoints);

        // holes
        for (Polygons::const_iterator hole = ex->holes.begin(); hole != ex->holes.end(); ++hole) {
            std::vector<p2t::Point*> points;
            for (Points::const_iterator point = hole->points.begin(); point != hole->points.end(); ++point) {
                // will be destructed in SweepContext::~SweepContext
                points.push_back(new p2t::Point(point->x, point->y));
            }
            cdt.AddHole(points);
        }
        
        // perform triangulation
        cdt.Triangulate();
        std::vector<p2t::Triangle*> triangles = cdt.GetTriangles();
        
        for (std::vector<p2t::Triangle*>::const_iterator triangle = triangles.begin(); triangle != triangles.end(); ++triangle) {
            Polygon p;
            for (int i = 0; i <= 2; ++i) {
                p2t::Point* point = (*triangle)->GetPoint(i);
                p.points.push_back(Point(point->x, point->y));
            }
            polygons->push_back(p);
        }

        for(std::vector<p2t::Point*>::iterator it = ContourPoints.begin(); it != ContourPoints.end(); ++it) {
            delete *it;
        }
    }
}

Lines
ExPolygon::lines() const
{
    Lines lines = this->contour.lines();
    for (Polygons::const_iterator h = this->holes.begin(); h != this->holes.end(); ++h) {
        Lines hole_lines = h->lines();
        lines.insert(lines.end(), hole_lines.begin(), hole_lines.end());
    }
    return lines;
}

std::string
ExPolygon::dump_perl() const
{
    std::ostringstream ret;
    ret << "[" << this->contour.dump_perl();
    for (Polygons::const_iterator h = this->holes.begin(); h != this->holes.end(); ++h)
        ret << "," << h->dump_perl();
    ret << "]";
    return ret.str();
}

BoundingBox get_extents(const ExPolygon &expolygon)
{
    return get_extents(expolygon.contour);
}

BoundingBox get_extents(const ExPolygons &expolygons)
{
    BoundingBox bbox;
    if (! expolygons.empty()) {
        for (size_t i = 0; i < expolygons.size(); ++ i)
			if (! expolygons[i].contour.points.empty())
				bbox.merge(get_extents(expolygons[i]));
    }
    return bbox;
}

BoundingBox get_extents_rotated(const ExPolygon &expolygon, double angle)
{
    return get_extents_rotated(expolygon.contour, angle);
}

BoundingBox get_extents_rotated(const ExPolygons &expolygons, double angle)
{
    BoundingBox bbox;
    if (! expolygons.empty()) {
        bbox = get_extents_rotated(expolygons.front().contour, angle);
        for (size_t i = 1; i < expolygons.size(); ++ i)
            bbox.merge(get_extents_rotated(expolygons[i].contour, angle));
    }
    return bbox;
}

extern std::vector<BoundingBox> get_extents_vector(const ExPolygons &polygons)
{
    std::vector<BoundingBox> out;
    out.reserve(polygons.size());
    for (ExPolygons::const_iterator it = polygons.begin(); it != polygons.end(); ++ it)
        out.push_back(get_extents(*it));
    return out;
}

bool remove_sticks(ExPolygon &poly)
{
    return remove_sticks(poly.contour) || remove_sticks(poly.holes);
}

} // namespace Slic3r
