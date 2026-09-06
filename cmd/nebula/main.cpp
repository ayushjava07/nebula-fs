#include "nebula/cli/CommandLine.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    nebula::cli::CommandLineHandler handler;
    auto options = handler.parse(argc, argv);
    auto result = handler.execute(options);

    if (!result.message.empty()) {
        if (result.success) {
            std::cout << result.message << std::endl;
        } else {
            std::cerr << result.message << std::endl;
        }
    }

    return result.exitCode;
}
