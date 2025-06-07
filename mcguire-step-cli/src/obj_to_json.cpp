#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct Mesh {
    std::string name;
    std::vector<std::vector<float>> vertices;
    std::vector<std::vector<int>> faces;
    std::map<int, int> globalToLocalVertexMap; // Maps global vertex index to local mesh index
    
    void addVertex(int globalIndex, const std::vector<float>& vertex) {
        if (globalToLocalVertexMap.find(globalIndex) == globalToLocalVertexMap.end()) {
            int localIndex = vertices.size();
            globalToLocalVertexMap[globalIndex] = localIndex;
            vertices.push_back(vertex);
        }
    }
    
    void addFace(const std::vector<int>& globalFace) {
        std::vector<int> localFace;
        for (int globalIdx : globalFace) {
            localFace.push_back(globalToLocalVertexMap[globalIdx]);
        }
        faces.push_back(localFace);
    }
    
    bool isEmpty() const {
        return vertices.empty() && faces.empty();
    }
};

std::vector<int> parseFaceIndices(const std::string& faceData) {
    std::vector<int> indices;
    std::istringstream faceStream(faceData);
    std::string index;
    
    while (faceStream >> index) {
        // Handle various OBJ face formats: v, v/vt, v/vt/vn, v//vn
        size_t slashPos = index.find('/');
        std::string vertexIndexStr;
        
        if (slashPos != std::string::npos) {
            vertexIndexStr = index.substr(0, slashPos);
        } else {
            vertexIndexStr = index;
        }
        
        if (!vertexIndexStr.empty()) {
            int vertexIndex = std::stoi(vertexIndexStr);
            // Handle negative indices (relative to current vertex count)
            if (vertexIndex < 0) {
                // This would need the current vertex count, but for simplicity we'll assume positive indices
                // In a full implementation, you'd pass the current vertex count here
            }
            indices.push_back(vertexIndex - 1); // Convert from 1-indexed to 0-indexed
        }
    }
    
    return indices;
}

void triangulate(const std::vector<int>& face, std::vector<std::vector<int>>& triangles) {
    if (face.size() < 3) return;
    
    // Simple fan triangulation for convex polygons
    for (size_t i = 1; i + 1 < face.size(); ++i) {
        triangles.push_back({face[0], face[i], face[i + 1]});
    }
}

void convertObjToJson(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath);
    if (!in.is_open()) throw std::runtime_error("Cannot open OBJ file");
    
    std::vector<std::vector<float>> globalVertices;
    std::vector<Mesh> meshes;
    std::string line;
    
    Mesh* currentMesh = nullptr;
    std::string currentObjectName = "default";
    int meshIndex = 0;
    
    // Create initial default mesh
    meshes.emplace_back();
    meshes.back().name = currentObjectName;
    currentMesh = &meshes.back();
    
    while (std::getline(in, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;
        
        if (prefix == "v") {
            // Vertex definition
            float x, y, z;
            ss >> x >> y >> z;
            globalVertices.push_back({x, y, z});
            
        } else if (prefix == "o" || prefix == "g") {
            // Object or group definition - start new mesh
            std::string objectName;
            std::getline(ss, objectName);
            
            // Trim leading/trailing whitespace
            objectName.erase(0, objectName.find_first_not_of(" \t"));
            objectName.erase(objectName.find_last_not_of(" \t") + 1);
            
            if (objectName.empty()) {
                objectName = (prefix == "o" ? "object_" : "group_") + std::to_string(meshIndex);
            }
            
            // Only create new mesh if current one has data or if this is not the first object/group
            if (!currentMesh->isEmpty() || meshes.size() > 1) {
                meshes.emplace_back();
                currentMesh = &meshes.back();
                meshIndex++;
            }
            
            currentMesh->name = objectName;
            
        } else if (prefix == "f") {
            // Face definition
            std::string faceData;
            std::getline(ss, faceData);
            
            std::vector<int> faceIndices = parseFaceIndices(faceData);
            
            if (faceIndices.size() >= 3) {
                // Add vertices used by this face to current mesh
                for (int globalIdx : faceIndices) {
                    if (globalIdx >= 0 && globalIdx < static_cast<int>(globalVertices.size())) {
                        currentMesh->addVertex(globalIdx, globalVertices[globalIdx]);
                    }
                }
                
                // Triangulate face if it has more than 3 vertices
                std::vector<std::vector<int>> triangles;
                triangulate(faceIndices, triangles);
                
                for (const auto& triangle : triangles) {
                    currentMesh->addFace(triangle);
                }
            }
        }
        // Ignore other OBJ elements like materials (mtllib, usemtl), texture coords (vt), normals (vn), etc.
    }
    
    // Remove empty meshes
    meshes.erase(std::remove_if(meshes.begin(), meshes.end(), 
                               [](const Mesh& mesh) { return mesh.isEmpty(); }), 
                 meshes.end());
    
    if (meshes.empty()) {
        throw std::runtime_error("No valid geometry found in OBJ file");
    }
    
    json j;
    
    // Check if we have multiple meshes
    if (meshes.size() == 1) {
        // Single mesh - maintain backward compatibility
        j["vertices"] = meshes[0].vertices;
        j["faces"] = meshes[0].faces;
        
        // Include mesh name if it's not the default
        if (!meshes[0].name.empty() && meshes[0].name != "default") {
            j["name"] = meshes[0].name;
        }
    } else {
        // Multiple meshes - use new multi-body format
        j["meshes"] = json::array();
        
        for (const auto& mesh : meshes) {
            json meshJson;
            meshJson["name"] = mesh.name;
            meshJson["vertices"] = mesh.vertices;
            meshJson["faces"] = mesh.faces;
            
            j["meshes"].push_back(meshJson);
        }
        
        // Add metadata
        j["mesh_count"] = meshes.size();
    }
    
    std::ofstream out(outputPath);
    out << j.dump(2);
}