#include "stl_to_json.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct Vec3 {
    float x, y, z;
    bool operator<(const Vec3& other) const {
        return std::tie(x, y, z) < std::tie(other.x, other.y, other.z);
    }
};

struct Mesh {
    std::string name;
    std::vector<Vec3> vertices;
    std::vector<std::array<int, 3>> faces;
    std::map<Vec3, int> vertexMap;
    
    void addVertex(const Vec3& v, std::array<int, 3>& currentFace, int faceVertexIndex) {
        auto [it, inserted] = vertexMap.emplace(v, vertices.size());
        if (inserted) {
            vertices.push_back(v);
        }
        currentFace[faceVertexIndex] = vertexMap[v];
    }
    
    void addFace(const std::array<int, 3>& face) {
        faces.push_back(face);
    }
    
    void clear() {
        vertices.clear();
        faces.clear();
        vertexMap.clear();
    }
};

bool isAsciiStl(const std::string& path) {
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    return line.find("solid") != std::string::npos;
}

std::vector<Mesh> parseAsciiStl(std::ifstream& in) {
    std::vector<Mesh> meshes;
    std::string line;
    Mesh currentMesh;
    std::array<int, 3> currentFace;
    int vertexCount = 0;
    bool inSolid = false;
    
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;
        
        if (keyword == "solid") {
            if (inSolid && (!currentMesh.vertices.empty() || !currentMesh.faces.empty())) {
                // Save previous mesh if it has data
                meshes.push_back(std::move(currentMesh));
                currentMesh.clear();
            }
            
            // Extract solid name (everything after "solid")
            std::string solidName;
            std::getline(iss, solidName);
            if (!solidName.empty() && solidName[0] == ' ') {
                solidName = solidName.substr(1); // Remove leading space
            }
            if (solidName.empty()) {
                solidName = "mesh_" + std::to_string(meshes.size());
            }
            
            currentMesh.name = solidName;
            vertexCount = 0;
            inSolid = true;
        }
        else if (keyword == "endsolid") {
            if (inSolid) {
                meshes.push_back(std::move(currentMesh));
                currentMesh.clear();
                inSolid = false;
            }
        }
        else if (keyword == "vertex" && inSolid) {
            Vec3 v;
            iss >> v.x >> v.y >> v.z;
            currentMesh.addVertex(v, currentFace, vertexCount % 3);
            vertexCount++;
        }
        else if (line.find("endloop") != std::string::npos && inSolid) {
            if (vertexCount >= 3) {
                currentMesh.addFace(currentFace);
                vertexCount = 0;
            }
        }
    }
    
    // Handle case where file doesn't end with endsolid
    if (inSolid && (!currentMesh.vertices.empty() || !currentMesh.faces.empty())) {
        meshes.push_back(std::move(currentMesh));
    }
    
    return meshes;
}

std::vector<Mesh> parseBinaryStl(std::ifstream& in) {
    std::vector<Mesh> meshes;
    char header[80];
    in.read(header, 80);
    
    uint32_t numTriangles;
    in.read(reinterpret_cast<char*>(&numTriangles), 4);
    
    // For binary STL, we typically have one mesh, but we'll structure it consistently
    Mesh mesh;
    mesh.name = "mesh_0"; // Default name for binary STL
    
    for (uint32_t i = 0; i < numTriangles; ++i) {
        in.ignore(12); // skip normal vector
        std::array<int, 3> faceIdx;
        
        for (int j = 0; j < 3; ++j) {
            Vec3 v;
            in.read(reinterpret_cast<char*>(&v), sizeof(Vec3));
            mesh.addVertex(v, faceIdx, j);
        }
        
        mesh.addFace(faceIdx);
        in.ignore(2); // skip attribute byte count
    }
    
    meshes.push_back(std::move(mesh));
    return meshes;
}

void convertStlToJson(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Could not open STL file");
    
    std::vector<Mesh> meshes;
    
    if (isAsciiStl(inputPath)) {
        in.close();
        in.open(inputPath); // reopen as text
        meshes = parseAsciiStl(in);
    } else {
        meshes = parseBinaryStl(in);
    }
    
    json j;
    
    // Check if we have multiple meshes
    if (meshes.size() == 1) {
        // Single mesh - maintain backward compatibility with existing format
        j["vertices"] = json::array();
        j["faces"] = json::array();
        
        for (const auto& v : meshes[0].vertices) {
            j["vertices"].push_back({v.x, v.y, v.z});
        }
        
        for (const auto& f : meshes[0].faces) {
            j["faces"].push_back({f[0], f[1], f[2]});
        }
        
        // Optionally include mesh name
        if (!meshes[0].name.empty() && meshes[0].name != "mesh_0") {
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
                meshJson["vertices"].push_back({v.x, v.y, v.z});
            }
            
            for (const auto& f : mesh.faces) {
                meshJson["faces"].push_back({f[0], f[1], f[2]});
            }
            
            j["meshes"].push_back(meshJson);
        }
        
        // Add metadata
        j["mesh_count"] = meshes.size();
    }
    
    std::ofstream out(outputPath);
    out << j.dump(2);
}