#include "Application.h"
#include <iostream>
#include <exception>

int main() {
    Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Blad krytyczny: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}