#include <iostream>              // ⬅️ Fix: Needed for std::cout, std::cerr
#include "step_to_json.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: mcguire_step_cli <input_step_file> <output_json_file>" << std::endl;
        return 1;
    }

    std::string stepPath = argv[1];
    std::string jsonPath = argv[2];

    convertStepToJson(stepPath, jsonPath);
    std::cout << "✅ Conversion completed: " << jsonPath << std::endl;

    return 0;
}
