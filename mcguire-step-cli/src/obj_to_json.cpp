#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void convertObjToJson(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath);
    if (!in.is_open()) throw std::runtime_error("Cannot open OBJ file");

    std::vector<std::vector<float>> vertices;
    std::vector<std::vector<int>> faces;

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            vertices.push_back({x, y, z});
        } else if (prefix == "f") {
            std::vector<int> face;
            std::string index;
            while (ss >> index) {
                int i = std::stoi(index.substr(0, index.find('/')));
                face.push_back(i - 1);  // OBJ is 1-indexed
            }
            if (face.size() >= 3)
                for (size_t i = 1; i + 1 < face.size(); ++i)
                    faces.push_back({face[0], face[i], face[i + 1]});
        }
    }

    json j;
    j["vertices"] = vertices;
    j["faces"] = faces;

    std::ofstream out(outputPath);
    out << j.dump();
}
