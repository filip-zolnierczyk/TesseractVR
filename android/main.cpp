#include "Application.h"

#include <android_native_app_glue.h>

void android_main(android_app* app) {
    app_dummy();

    while (app->window == nullptr && !app->destroyRequested) {
        int events;
        android_poll_source* source;
        if (ALooper_pollAll(-1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0 &&
            source != nullptr) {
            source->process(app, source);
        }
    }

    if (app->destroyRequested) {
        return;
    }

    Application application;
    application.runAndroid(app);
}
