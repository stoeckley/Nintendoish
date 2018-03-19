#pragma once

#include "glfw3.h"

class GlassDisplay {
    float scale;
    int width;
    int height;
public:
    GlassDisplay(float scale, int width, int  height);
    void plotPixel(int x, int y, float r, float g, float b);
    void initialize(int argc, char **argv);
    GLFWwindow* window;
    ~GlassDisplay();
};
