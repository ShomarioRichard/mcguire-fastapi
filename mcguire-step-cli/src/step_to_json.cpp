// src/step_to_json.cpp
#include "step_to_json.h"
#include <STEPControl_Reader.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopoDS.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Pnt.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_Label.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFApp_Application.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDF_LabelSequence.hxx>
#include <TCollection_ExtendedString.hxx>
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>
#include <vector>
#include <string>

using json = nlohmann::json;

struct PointComparator {
    bool operator()(const gp_Pnt& a, const gp_Pnt& b) const {
        const double tolerance = 1e-9;
        if (std::abs(a.X() - b.X()) > tolerance) return a.X() < b.X();
        if (std::abs(a.Y() - b.Y()) > tolerance) return a.Y() < b.Y();
        return a.Z() < b.Z() - tolerance;
    }
};

struct Mesh {
    std::string name;
    std::vector<std::array<double, 3>> vertices;
    std::vector<std::array<int, 3>> faces;
    std::map<gp_Pnt, int, PointComparator> vertexMap;
    
    void addVertex(const gp_Pnt& pt) {
        if (vertexMap.find(pt) == vertexMap.end()) {
            int id = vertices.size();
            vertexMap[pt] = id;
            vertices.push_back({pt.X(), pt.Y(), pt.Z()});
        }
    }
    
    void addFace(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3) {
        addVertex(p1);
        addVertex(p2);
        addVertex(p3);
        faces.push_back({vertexMap[p1], vertexMap[p2], vertexMap[p3]});
    }
    
    bool isEmpty() const {
        return vertices.empty() && faces.empty();
    }
};

std::string getShapeName(const TDF_Label& label, int defaultIndex) {
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        TCollection_ExtendedString extName = nameAttr->Get();
        TCollection_AsciiString asciiName(extName);
        Standard_CString cstr = asciiName.ToCString();
        if (cstr && strlen(cstr) > 0) {
            return std::string(cstr);
        }
    }
    return "body_" + std::to_string(defaultIndex);
}

void meshShape(const TopoDS_Shape& shape, Mesh& mesh, double deflection = 0.1) {
    // Create mesh for the shape
    BRepMesh_IncrementalMesh mesher(shape, deflection);
    
    // Extract triangulation from all faces
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        TopoDS_Face face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, loc);
        
        if (triangulation.IsNull()) continue;
        
        int nbNodes = triangulation->NbNodes();
        int nbTriangles = triangulation->NbTriangles();
        
        // Create mapping from local triangle indices to global vertices
        std::vector<gp_Pnt> localVertices(nbNodes + 1); // OCC is 1-based
        for (int i = 1; i <= nbNodes; ++i) {
            localVertices[i] = triangulation->Node(i).Transformed(loc.Transformation());
        }
        
        // Add triangles
        for (int i = 1; i <= nbTriangles; ++i) {
            Poly_Triangle t = triangulation->Triangle(i);
            int n1, n2, n3;
            t.Get(n1, n2, n3);
            
            // Handle face orientation
            if (face.Orientation() == TopAbs_REVERSED) {
                mesh.addFace(localVertices[n1], localVertices[n3], localVertices[n2]);
            } else {
                mesh.addFace(localVertices[n1], localVertices[n2], localVertices[n3]);
            }
        }
    }
}

std::vector<Mesh> extractMeshesWithCAF(const std::string& inputPath, double deflection = 0.1) {
    std::vector<Mesh> meshes;
    
    // Try to read with CAF (Component Application Framework) for better component separation
    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    app->NewDocument("MDTV-XCAF", doc);
    
    STEPCAFControl_Reader reader;
    if (reader.ReadFile(inputPath.c_str()) != IFSelect_RetDone) {
        throw std::runtime_error("Failed to read STEP file with CAF reader");
    }
    
    if (!reader.Transfer(doc)) {
        throw std::runtime_error("Failed to transfer STEP data");
    }
    
    Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    TDF_LabelSequence topLevelShapes;
    shapeTool->GetFreeShapes(topLevelShapes);
    
    // If we have multiple top-level shapes, treat each as a separate mesh
    if (topLevelShapes.Length() > 1) {
        for (int i = 1; i <= topLevelShapes.Length(); ++i) {
            TDF_Label label = topLevelShapes.Value(i);
            TopoDS_Shape shape;
            if (shapeTool->GetShape(label, shape)) {
                Mesh mesh;
                mesh.name = getShapeName(label, i - 1);
                meshShape(shape, mesh, deflection);
                if (!mesh.isEmpty()) {
                    meshes.push_back(std::move(mesh));
                }
            }
        }
    } else if (topLevelShapes.Length() == 1) {
        // Single top-level shape - check if it contains multiple solids
        TDF_Label rootLabel = topLevelShapes.Value(1);
        TopoDS_Shape rootShape;
        shapeTool->GetShape(rootLabel, rootShape);
        
        // Count solids in the root shape
        int solidCount = 0;
        for (TopExp_Explorer exp(rootShape, TopAbs_SOLID); exp.More(); exp.Next()) {
            solidCount++;
        }
        
        if (solidCount > 1) {
            // Multiple solids - create separate mesh for each
            int solidIndex = 0;
            for (TopExp_Explorer exp(rootShape, TopAbs_SOLID); exp.More(); exp.Next()) {
                TopoDS_Solid solid = TopoDS::Solid(exp.Current());
                Mesh mesh;
                mesh.name = "solid_" + std::to_string(solidIndex);
                meshShape(solid, mesh, deflection);
                if (!mesh.isEmpty()) {
                    meshes.push_back(std::move(mesh));
                }
                solidIndex++;
            }
        } else {
            // Single solid or no solids - treat as single mesh
            Mesh mesh;
            mesh.name = getShapeName(rootLabel, 0);
            meshShape(rootShape, mesh, deflection);
            if (!mesh.isEmpty()) {
                meshes.push_back(std::move(mesh));
            }
        }
    }
    
    return meshes;
}

std::vector<Mesh> extractMeshesBasic(const std::string& inputPath, double deflection = 0.1) {
    std::vector<Mesh> meshes;
    
    STEPControl_Reader reader;
    if (reader.ReadFile(inputPath.c_str()) != IFSelect_RetDone) {
        throw std::runtime_error("Failed to read STEP file");
    }
    
    reader.TransferRoots();
    TopoDS_Shape shape = reader.OneShape();
    
    // Check if we have multiple solids
    int solidCount = 0;
    for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
        solidCount++;
    }
    
    if (solidCount > 1) {
        // Multiple solids - create separate mesh for each
        int solidIndex = 0;
        for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
            TopoDS_Solid solid = TopoDS::Solid(exp.Current());
            Mesh mesh;
            mesh.name = "solid_" + std::to_string(solidIndex);
            meshShape(solid, mesh, deflection);
            if (!mesh.isEmpty()) {
                meshes.push_back(std::move(mesh));
            }
            solidIndex++;
        }
    } else {
        // Single solid or complex shape - treat as single mesh
        Mesh mesh;
        mesh.name = "shape_0";
        meshShape(shape, mesh, deflection);
        if (!mesh.isEmpty()) {
            meshes.push_back(std::move(mesh));
        }
    }
    
    return meshes;
}

void convertStepToJson(const std::string& inputPath, const std::string& outputPath, double deflection = 0.1) {
    std::vector<Mesh> meshes;
    
    // Try CAF reader first for better component separation
    try {
        meshes = extractMeshesWithCAF(inputPath, deflection);
    } catch (const std::exception&) {
        // Fall back to basic reader
        try {
            meshes = extractMeshesBasic(inputPath, deflection);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to read STEP file with both CAF and basic readers: " + std::string(e.what()));
        }
    }
    
    if (meshes.empty()) {
        throw std::runtime_error("No valid geometry found in STEP file");
    }
    
    json j;
    
    // Check if we have multiple meshes
    if (meshes.size() == 1) {
        // Single mesh - maintain backward compatibility
        j["vertices"] = json::array();
        j["faces"] = json::array();
        
        for (const auto& v : meshes[0].vertices) {
            j["vertices"].push_back({v[0], v[1], v[2]});
        }
        
        for (const auto& f : meshes[0].faces) {
            j["faces"].push_back({f[0], f[1], f[2]});
        }
        
        // Include mesh name if meaningful
        if (!meshes[0].name.empty() && meshes[0].name != "shape_0") {
            j["name"] = meshes[0].name;
        }
    } else {
        // Multiple meshes - use new multi-body format
        j["meshes"] = json::array();
        
        for (const auto& mesh : meshes) {
            json meshJson;
            meshJson["name"] = mesh.name;
            meshJson["vertices"] = json::array();
            meshJson["faces"] = json::array();
            
            for (const auto& v : mesh.vertices) {
                meshJson["vertices"].push_back({v[0], v[1], v[2]});
            }
            
            for (const auto& f : mesh.faces) {
                meshJson["faces"].push_back({f[0], f[1], f[2]});
            }
            
            j["meshes"].push_back(meshJson);
        }
        
        // Add metadata
        j["mesh_count"] = meshes.size();
    }
    
    // Add processing metadata
    j["deflection"] = deflection;
    
    std::ofstream out(outputPath);
    out << j.dump(2);
}

// Overloaded version to maintain backward compatibility
void convertStepToJson(const std::string& inputPath, const std::string& outputPath) {
    convertStepToJson(inputPath, outputPath, 0.1);
}