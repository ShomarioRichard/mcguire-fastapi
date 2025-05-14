#include <iostream>
#include <string>
#include <algorithm>
#include "step_to_json.h"
#include "stl_to_json.h"

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

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: mcguire_step_cli <input_file.step|.stl> <output_file.json>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    std::string ext = getExtension(inputPath);

    try {
        if (ext == ".step" || ext == ".stp") {
            convertStepToJson(inputPath, outputPath);  // uses OpenCASCADE
        } else if (ext == ".stl") {
            convertStlToJson(inputPath, outputPath);   // uses custom STL parser
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

