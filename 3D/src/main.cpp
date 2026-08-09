#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>


#pragma pack(push, 1)
struct Vertex
{
    float x, y, z;
    float r, g, b;
};
#pragma pack(pop)


#pragma pack(push, 1)
struct VertexCompressed
{
    int16_t x, y, z;
    uint8_t r, g, b;
};
#pragma pack(pop)


// ============================================================
// Generate point cloud from color + depth images
// ============================================================

void generateCompressedCloudFromImages(
    std::vector<VertexCompressed>& cloud,
    const char* colorFile,
    const char* depthFile)
{
    int colorW, colorH, colorC;

    unsigned char* colorImg =
        stbi_load(
            colorFile,
            &colorW,
            &colorH,
            &colorC,
            3
        );

    int depthW, depthH, depthC;

    unsigned char* depthImg =
        stbi_load(
            depthFile,
            &depthW,
            &depthH,
            &depthC,
            1
        );

    if (!colorImg || !depthImg)
    {
        std::cout << "Failed to load images\n";

        if (colorImg)
            stbi_image_free(colorImg);

        if (depthImg)
            stbi_image_free(depthImg);

        return;
    }

    if (colorW != depthW || colorH != depthH)
    {
        std::cout << "Image sizes do not match\n";

        stbi_image_free(colorImg);
        stbi_image_free(depthImg);

        return;
    }


    cloud.clear();

    cloud.reserve(colorW * colorH);


    const float depthScale = 5.0f;


    // Downsample image 2x2
    for (int y = 0; y < depthH; y += 1)
    {
        for (int x = 0; x < depthW; x += 1)
        {
            int idxDepth =
                y * depthW + x;


            unsigned char depthValue =
                depthImg[idxDepth];


            // Ignore invalid depth
            //if (depthValue == 0)
            //    continue;


            // -----------------------------------------
            // Position
            // -----------------------------------------

            float z =
                (depthValue / 255.0f) * depthScale;


            float px =
                (float)x / depthW - 0.5f;


            float py =
                -(float)y / depthH + 0.5f;


            // -----------------------------------------
            // Color
            // -----------------------------------------

            int idxColor =
                (y * colorW + x) * 3;


            uint8_t r =
                colorImg[idxColor + 0];

            uint8_t g =
                colorImg[idxColor + 1];

            uint8_t b =
                colorImg[idxColor + 2];


            // -----------------------------------------
            // Compress position
            //
            // x,y: [-0.5,0.5] -> int16
            // z:   [0,5]      -> int16
            // -----------------------------------------

            int16_t cX =
                static_cast<int16_t>(
                    px * 32767.0f
                    );


            int16_t cY =
                static_cast<int16_t>(
                    py * 32767.0f
                    );


            int16_t cZ =
                static_cast<int16_t>(
                    (z / depthScale) * 32767.0f
                    );


            cloud.push_back(
                {
                    cX,
                    cY,
                    cZ,
                    r,
                    g,
                    b
                });
        }
    }


    stbi_image_free(colorImg);
    stbi_image_free(depthImg);


    std::cout
        << "Loaded compressed point cloud: "
        << cloud.size()
        << " points\n";
}


// ============================================================
// Convert compressed vertex back to normal coordinates
// ============================================================

Vertex decompressVertex(
    const VertexCompressed& v)
{
    Vertex result{};

    result.x =
        static_cast<float>(v.x) / 32767.0f;

    result.y =
        static_cast<float>(v.y) / 32767.0f;

    result.z =
        static_cast<float>(v.z) / 32767.0f;

    result.r =
        static_cast<float>(v.r) / 255.0f;

    result.g =
        static_cast<float>(v.g) / 255.0f;

    result.b =
        static_cast<float>(v.b) / 255.0f;

    return result;
}


// ============================================================
// Draw point cloud
// ============================================================

void drawPointCloud(
    const std::vector<VertexCompressed>& cloud)
{
    glPointSize(3.0f);

    glBegin(GL_POINTS);

    for (const auto& compressed : cloud)
    {
        Vertex v =
            decompressVertex(compressed);


        glColor3f(
            v.r,
            v.g,
            v.b
        );


        glVertex3f(
            v.x,
            v.y,
            v.z
        );
    }

    glEnd();
}


bool rotating = false;
bool panning = false;

double lastMouseX = 0.0;
double lastMouseY = 0.0;

float panX = 0.0f;
float panY = 0.0f;

float yaw = 0.0f;
float pitch = 0.0f;

float rotateSensitivity = 0.3f;
float panSensitivity = 0.005f;




void mouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int mods)
{
    if (action == GLFW_PRESS)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            rotating = true;

            glfwGetCursorPos(
                window,
                &lastMouseX,
                &lastMouseY
            );
        }

        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        {
            panning = true;

            glfwGetCursorPos(
                window,
                &lastMouseX,
                &lastMouseY
            );
        }
    }

    if (action == GLFW_RELEASE)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
            rotating = false;

        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
            panning = false;
    }
}

void cursorPositionCallback(
    GLFWwindow* window,
    double xpos,
    double ypos)
{
    double deltaX =
        xpos - lastMouseX;

    double deltaY =
        ypos - lastMouseY;

    lastMouseX = xpos;
    lastMouseY = ypos;

    // ============================================
    // Rotation
    // ============================================

    if (rotating)
    {
        yaw +=
            static_cast<float>(deltaX)
            * rotateSensitivity;

        pitch +=
            static_cast<float>(deltaY)
            * rotateSensitivity;

        // Prevent flipping upside down
        pitch = std::clamp(
            pitch,
            -89.0f,
            89.0f
        );
    }

    // ============================================
    // Pan
    // ============================================

    if (panning)
    {
        panX +=
            static_cast<float>(deltaX)
            * panSensitivity;

        panY -=
            static_cast<float>(deltaY)
            * panSensitivity;
    }
}

// ============================================================
// Main
// ============================================================

int main()
{
    // --------------------------------------------------------
    // GLFW
    // --------------------------------------------------------

    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW\n";
        return -1;
    }


    GLFWwindow* window =
        glfwCreateWindow(
            1280,
            720,
            "Point Cloud Viewer",
            nullptr,
            nullptr
        );




    if (!window)
    {
        std::cout << "Failed to create GLFW window\n";

        glfwTerminate();

        return -1;
    }


    glfwMakeContextCurrent(window);

    glfwSetMouseButtonCallback(
        window,
        mouseButtonCallback
    );

    glfwSetCursorPosCallback(
        window,
        cursorPositionCallback
    );

    glfwSwapInterval(1);


    // --------------------------------------------------------
    // OpenGL settings
    // --------------------------------------------------------

    glEnable(GL_DEPTH_TEST);


    glClearColor(
        0.02f,
        0.02f,
        0.02f,
        1.0f
    );


    // --------------------------------------------------------
    // Load point cloud
    // --------------------------------------------------------

    std::vector<VertexCompressed> cloud;


    generateCompressedCloudFromImages(
        cloud,
        "assets/color.jpeg",
        "assets/depth.png"
    );


    std::cout
        << "Point cloud size: "
        << cloud.size()
        << "\n";


    // --------------------------------------------------------
    // Main loop
    // --------------------------------------------------------

    while (!glfwWindowShouldClose(window))
    {
        // ----------------------------------------------------
        // Clear
        // ----------------------------------------------------

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );


        // ----------------------------------------------------
        // Projection
        // ----------------------------------------------------

        int width;
        int height;

        glfwGetFramebufferSize(
            window,
            &width,
            &height
        );


        glViewport(
            0,
            0,
            width,
            height
        );


        float aspect =
            static_cast<float>(width) /
            static_cast<float>(height);


        glMatrixMode(GL_PROJECTION);

        glLoadIdentity();


        // Simple orthographic projection

        if (aspect >= 1.0f)
        {
            glOrtho(
                -0.7f * aspect,
                0.7f * aspect,

                -0.7f,
                0.7f,

                -10.0f,
                10.0f
            );
        }
        else
        {
            glOrtho(
                -0.7f,
                0.7f,

                -0.7f / aspect,
                0.7f / aspect,

                -10.0f,
                10.0f
            );
        }


        // ----------------------------------------------------
        // Camera
        // ----------------------------------------------------

        glMatrixMode(GL_MODELVIEW);

        glLoadIdentity();


        // Move cloud in front of camera.
        //
        // OpenGL camera looks towards -Z.
        // Our depth is positive, therefore move the
        // entire cloud backwards.

        glTranslatef(
            panX,
            panY,
            -6.0f
        );

        glRotatef(
            pitch,
            1.0f,
            0.0f,
            0.0f
        );

        glRotatef(
            yaw,
            0.0f,
            1.0f,
            0.0f
        );


        // ----------------------------------------------------
        // Draw
        // ----------------------------------------------------

        drawPointCloud(cloud);


        // ----------------------------------------------------
        // Present
        // ----------------------------------------------------

        glfwSwapBuffers(window);

        glfwPollEvents();
    }


    // --------------------------------------------------------
    // Cleanup
    // --------------------------------------------------------

    glfwDestroyWindow(window);

    glfwTerminate();


    return 0;
}