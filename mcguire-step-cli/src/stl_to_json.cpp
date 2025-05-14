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

bool isAsciiStl(const std::string& path) {
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    return line.find("solid") != std::string::npos;
}

void parseAsciiStl(std::ifstream& in, std::vector<Vec3>& vertices, std::vector<std::array<int, 3>>& faces) {
    std::map<Vec3, int> vertexMap;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "vertex") {
            Vec3 v;
            iss >> v.x >> v.y >> v.z;
            auto [it, inserted] = vertexMap.emplace(v, vertexMap.size());
            if (inserted) {
                vertices.push_back(v);
            }
        }

        if (line.find("endloop") != std::string::npos) {
            int n = vertices.size();
            faces.push_back({n - 3, n - 2, n - 1});
        }
    }
}

void parseBinaryStl(std::ifstream& in, std::vector<Vec3>& vertices, std::vector<std::array<int, 3>>& faces) {
    char header[80];
    in.read(header, 80);

    uint32_t numTriangles;
    in.read(reinterpret_cast<char*>(&numTriangles), 4);

    std::map<Vec3, int> vertexMap;
    for (uint32_t i = 0; i < numTriangles; ++i) {
        in.ignore(12); // skip normal vector
        std::array<int, 3> faceIdx;

        for (int j = 0; j < 3; ++j) {
            Vec3 v;
            in.read(reinterpret_cast<char*>(&v), sizeof(Vec3));
            auto [it, inserted] = vertexMap.emplace(v, vertexMap.size());
            if (inserted) {
                vertices.push_back(v);
            }
            faceIdx[j] = vertexMap[v];
        }

        faces.push_back(faceIdx);
        in.ignore(2); // skip attribute byte count
    }
}

void convertStlToJson(const std::string& inputPath, const std::string& outputPath) {
    std::vector<Vec3> vertices;
    std::vector<std::array<int, 3>> faces;

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Could not open STL file");

    if (isAsciiStl(inputPath)) {
        in.close();
        in.open(inputPath); // reopen as text
        parseAsciiStl(in, vertices, faces);
    } else {
        parseBinaryStl(in, vertices, faces);
    }

    json j;
    j["vertices"] = json::array();
    j["faces"] = json::array();

    for (const auto& v : vertices)
        j["vertices"].push_back({v.x, v.y, v.z});

    for (const auto& f : faces)
        j["faces"].push_back({f[0], f[1], f[2]});

    std::ofstream out(outputPath);
    out << j.dump(2);
}
