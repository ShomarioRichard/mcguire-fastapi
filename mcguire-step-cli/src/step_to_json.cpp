// src/step_to_json.cpp
#include "step_to_json.h"
#include <STEPControl_Reader.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopoDS.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Pnt.hxx>
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>

using json = nlohmann::json;

void convertStepToJson(const std::string& inputPath, const std::string& outputPath) {
    STEPControl_Reader reader;
    if (reader.ReadFile(inputPath.c_str()) != IFSelect_RetDone)
        throw std::runtime_error("Failed to read STEP file");

    reader.TransferRoots();
    TopoDS_Shape shape = reader.OneShape();

    BRepMesh_IncrementalMesh mesher(shape, 0.1);

    json j;
    j["vertices"] = json::array();
    j["faces"] = json::array();

    std::map<gp_Pnt, int, bool(*)(const gp_Pnt&, const gp_Pnt&)> vertexMap(
        [](const gp_Pnt& a, const gp_Pnt& b) {
            return a.X() < b.X() || (a.X() == b.X() && a.Y() < b.Y()) || (a.X() == b.X() && a.Y() == b.Y() && a.Z() < b.Z());
        }
    );

    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        TopoDS_Face face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, loc);
        if (triangulation.IsNull()) continue;

        int nbNodes = triangulation->NbNodes();
        int nbTriangles = triangulation->NbTriangles();

        std::vector<int> indexMap(nbNodes + 1);  // OCC is 1-based

        for (int i = 1; i <= nbNodes; ++i) {
            gp_Pnt pt = triangulation->Node(i).Transformed(loc.Transformation());

            if (vertexMap.find(pt) == vertexMap.end()) {
                int id = vertexMap.size();
                vertexMap[pt] = id;
                j["vertices"].push_back({ pt.X(), pt.Y(), pt.Z() });
            }

            indexMap[i] = vertexMap[pt];
        }

        for (int i = 1; i <= nbTriangles; ++i) {
            Poly_Triangle t = triangulation->Triangle(i);
            int n1, n2, n3;
            t.Get(n1, n2, n3);
            j["faces"].push_back({ indexMap[n1], indexMap[n2], indexMap[n3] });
        }
    }

    std::ofstream out(outputPath);
    out << j.dump(2);
}
