#include "Application.h"
#include <iostream>
#include <exception>

#ifndef TESSERACT_ANDROID
int main() {
    Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Blad krytyczny: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    #endif

    return EXIT_SUCCESS;
}