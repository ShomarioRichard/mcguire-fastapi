#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

#include "step_to_json.h"
#include "stl_to_json.h"
#include "obj_to_json.h"

std::string toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lowerStr;
}

std::string getExtension(const std::string& filename) {
    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos) return "";
    return toLower(filename.substr(dot));
}

bool isJsonFileValid(const std::string& path) {
    try {
        std::ifstream in(path);
        if (!in) return false;
        nlohmann::json j;
        in >> j;
        return j.contains("vertices") && (j.contains("tetrahedra") || j.contains("faces"));
    } catch (...) {
        return false;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: mcguire_step_cli <input_file.step|.stl|.obj|.json> <output_file.json>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    std::string ext = getExtension(inputPath);

    try {
        if (ext == ".step" || ext == ".stp") {
            convertStepToJson(inputPath, outputPath);
        } else if (ext == ".stl") {
            convertStlToJson(inputPath, outputPath);
        } else if (ext == ".obj") {
            convertObjToJson(inputPath, outputPath);
        } else if (ext == ".json") {
            if (isJsonFileValid(inputPath)) {
                std::ifstream in(inputPath);
                std::ofstream out(outputPath);
                if (!out) throw std::runtime_error("Failed to open output file for writing");
                out << in.rdbuf();
                std::cout << "✅ Passed through valid .json file as mesh output" << std::endl;
            } else {
                throw std::runtime_error("Invalid or unsupported .json format: must contain 'vertices' and either 'faces' or 'tetrahedra'");
            }
        } else {
            std::cerr << "❌ Unsupported file extension: " << ext << std::endl;
            return 2;
        }

        std::cout << "✅ Conversion completed: " << outputPath << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error during conversion: " << e.what() << std::endl;
        return 3;
    }
}
