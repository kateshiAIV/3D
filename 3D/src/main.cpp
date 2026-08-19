#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <psapi.h>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


// ============================================================
// VERTEX
// ============================================================

#pragma pack(push, 1)

struct Vertex
{
    float x, y, z;
    float r, g, b;
};

#pragma pack(pop)


// ============================================================
// COMPRESSED VERTEX
// ============================================================

#pragma pack(push, 1)

struct VertexCompressed
{
    int16_t x, y, z;
    uint8_t r, g, b;
};

#pragma pack(pop)


// ============================================================
// LOD METHOD
// ============================================================

enum class LODMethod
{
    None,
    Random,
    Uniform,
    Grid,
    Distance
};


// ============================================================
// LOD SETTINGS
// ============================================================

struct LODSettings
{
    LODMethod method = LODMethod::Random;

    float nearDistance = 3.0f;
    float farDistance = 12.0f;

    // Random sampling
    float nearDensity = 1.0f;
    float farDensity = 0.1f;

    // Uniform / Every-N
    int nearStep = 1;
    int farStep = 10;

    // Grid
    int nearGridSize = 1;
    int farGridSize = 8;

    // Distance based
    float nearSpacing = 1.0f;
    float farSpacing = 8.0f;
};


// ============================================================
// CAMERA
// ============================================================

bool rotating = false;
bool panning = false;

double lastMouseX = 0.0;
double lastMouseY = 0.0;

float panX = 0.0f;
float panY = 0.0f;

float yaw = 0.0f;
float pitch = 0.0f;


// Camera distance

float cameraDistance = 6.0f;

float minCameraDistance = 3.0f;
float maxCameraDistance = 30.0f;

float zoomSensitivity = 0.1f;


// Mouse sensitivity

float rotateSensitivity = 0.3f;
float panSensitivity = 0.005f;


// ============================================================
// IMAGE / CLOUD DIMENSIONS
// ============================================================

int cloudWidth = 0;
int cloudHeight = 0;


// ============================================================
// PERFORMANCE METRICS
// ============================================================

double currentFPS = 0.0;
double currentFrameTimeMs = 0.0;
double currentCPUTimeMs = 0.0;

std::ofstream logFile(
    "log.txt",
    std::ios::out | std::ios::trunc
);


// FPS measurement

static auto fpsTimer =
std::chrono::high_resolution_clock::now();

static int fpsFrameCount = 0;


// ============================================================
// PROCESS MEMORY
// ============================================================

size_t getProcessMemoryUsage()
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};

    pmc.cb = sizeof(pmc);

    if (GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
        sizeof(pmc)))
    {
        return pmc.WorkingSetSize;
    }

    return 0;
}


// ============================================================
// UPDATE FPS
// ============================================================

void updateFPS()
{
    fpsFrameCount++;

    auto now =
        std::chrono::high_resolution_clock::now();

    double elapsed =
        std::chrono::duration<double>(
            now - fpsTimer
        ).count();

    if (elapsed >= 1.0)
    {
        currentFPS =
            static_cast<double>(
                fpsFrameCount
                ) / elapsed;

        fpsFrameCount = 0;

        fpsTimer = now;
    }

    if (currentFPS > 0.0)
    {
        currentFrameTimeMs =
            1000.0 / currentFPS;
    }
}


// ============================================================
// LOAD RGB + DEPTH
// ============================================================

void generateCompressedCloudFromImages(
    std::vector<VertexCompressed>& cloud,
    const char* colorFile,
    const char* depthFile,
    int& outputWidth,
    int& outputHeight)
{
    int colorW;
    int colorH;
    int colorC;

    unsigned char* colorImg =
        stbi_load(
            colorFile,
            &colorW,
            &colorH,
            &colorC,
            3
        );


    int depthW;
    int depthH;
    int depthC;

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
        std::cout
            << "Failed to load images\n";

        if (colorImg)
            stbi_image_free(colorImg);

        if (depthImg)
            stbi_image_free(depthImg);

        return;
    }


    if (colorW != depthW ||
        colorH != depthH)
    {
        std::cout
            << "Image sizes do not match\n";

        stbi_image_free(colorImg);
        stbi_image_free(depthImg);

        return;
    }


    outputWidth = colorW;
    outputHeight = colorH;


    cloud.clear();

    cloud.reserve(
        colorW * colorH
    );


    const float depthScale = 5.0f;


    // ========================================================
    // Generate point cloud
    // ========================================================

    for (int y = 0; y < depthH; y++)
    {
        for (int x = 0; x < depthW; x++)
        {
            int idxDepth =
                y * depthW + x;


            unsigned char depthValue =
                depthImg[idxDepth];


            float z =
                (depthValue / 255.0f)
                * depthScale;


            float px =
                static_cast<float>(x)
                / depthW
                - 0.5f;


            float py =
                -static_cast<float>(y)
                / depthH
                + 0.5f;


            int idxColor =
                (y * colorW + x) * 3;


            uint8_t r =
                colorImg[idxColor + 0];

            uint8_t g =
                colorImg[idxColor + 1];

            uint8_t b =
                colorImg[idxColor + 2];


            // =================================================
            // Compress position
            // =================================================

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
                    (z / depthScale)
                    * 32767.0f
                    );


            cloud.push_back(
                {
                    cX,
                    cY,
                    cZ,
                    r,
                    g,
                    b
                }
            );
        }
    }


    stbi_image_free(colorImg);
    stbi_image_free(depthImg);


    std::cout
        << "Loaded compressed point cloud: "
        << cloud.size()
        << " points\n";

    std::cout
        << "Cloud dimensions: "
        << outputWidth
        << " x "
        << outputHeight
        << "\n";
}


// ============================================================
// DECOMPRESS VERTEX
// ============================================================

Vertex decompressVertex(
    const VertexCompressed& v)
{
    Vertex result{};

    result.x =
        static_cast<float>(v.x)
        / 32767.0f;

    result.y =
        static_cast<float>(v.y)
        / 32767.0f;

    result.z =
        static_cast<float>(v.z)
        / 32767.0f;

    result.r =
        static_cast<float>(v.r)
        / 255.0f;

    result.g =
        static_cast<float>(v.g)
        / 255.0f;

    result.b =
        static_cast<float>(v.b)
        / 255.0f;

    return result;
}


// ============================================================
// METHOD NAME
// ============================================================

const char* getLODMethodName(
    LODMethod method)
{
    switch (method)
    {
    case LODMethod::None:
        return "No LOD";

    case LODMethod::Random:
        return "Random Sampling";

    case LODMethod::Uniform:
        return "Uniform / Every-N";

    case LODMethod::Grid:
        return "Grid Sampling";

    case LODMethod::Distance:
        return "Distance-Based";

    default:
        return "Unknown";
    }
}


// ============================================================
// DETERMINISTIC RANDOM VALUE
// ============================================================

float randomValueForPoint(
    size_t index)
{
    uint32_t value =
        static_cast<uint32_t>(index);

    value ^= value >> 16;

    value *= 0x45d9f3b;

    value ^= value >> 16;

    value *= 0x45d9f3b;

    value ^= value >> 16;

    return
        static_cast<float>(value)
        /
        static_cast<float>(UINT32_MAX);
}


// ============================================================
// RANDOM LOD DENSITY
// ============================================================

float calculateLODDensity(
    float cameraDistance,
    const LODSettings& settings)
{
    if (cameraDistance <=
        settings.nearDistance)
    {
        return settings.nearDensity;
    }


    if (cameraDistance >=
        settings.farDistance)
    {
        return settings.farDensity;
    }


    float t =
        (cameraDistance -
            settings.nearDistance)
        /
        (settings.farDistance -
            settings.nearDistance);


    float density =
        settings.nearDensity +
        t *
        (
            settings.farDensity -
            settings.nearDensity
            );


    return
        std::clamp(
            density,
            0.0f,
            1.0f
        );
}


// ============================================================
// RANDOM SAMPLING
// ============================================================

bool shouldRenderRandom(
    size_t index,
    float density)
{
    if (density >= 1.0f)
        return true;

    if (density <= 0.0f)
        return false;

    return
        randomValueForPoint(index)
        < density;
}


// ============================================================
// UNIFORM / EVERY-N
// ============================================================

int calculateUniformStep(
    float cameraDistance,
    const LODSettings& settings)
{
    if (cameraDistance <=
        settings.nearDistance)
    {
        return settings.nearStep;
    }


    if (cameraDistance >=
        settings.farDistance)
    {
        return settings.farStep;
    }


    float t =
        (cameraDistance -
            settings.nearDistance)
        /
        (settings.farDistance -
            settings.nearDistance);


    float step =
        settings.nearStep +
        t *
        (
            settings.farStep -
            settings.nearStep
            );


    return max(
        1,
        static_cast<int>(
            std::round(step)
            )
    );
}


bool shouldRenderUniform(
    size_t index,
    int step)
{
    if (step <= 1)
        return true;

    return
        (index % step) == 0;
}


// ============================================================
// GRID SAMPLING
// ============================================================

int calculateGridSize(
    float cameraDistance,
    const LODSettings& settings)
{
    if (cameraDistance <=
        settings.nearDistance)
    {
        return settings.nearGridSize;
    }


    if (cameraDistance >=
        settings.farDistance)
    {
        return settings.farGridSize;
    }


    float t =
        (cameraDistance -
            settings.nearDistance)
        /
        (settings.farDistance -
            settings.nearDistance);


    float gridSize =
        settings.nearGridSize +
        t *
        (
            settings.farGridSize -
            settings.nearGridSize
            );


    return max(
        1,
        static_cast<int>(
            std::round(gridSize)
            )
    );
}


bool shouldRenderGrid(
    size_t index,
    int gridSize,
    int imageWidth)
{
    if (gridSize <= 1)
        return true;


    if (imageWidth <= 0)
        return true;


    int x =
        static_cast<int>(
            index % imageWidth
            );


    int y =
        static_cast<int>(
            index / imageWidth
            );


    return
        (x % gridSize == 0) &&
        (y % gridSize == 0);
}


// ============================================================
// DISTANCE-BASED SAMPLING
// ============================================================

float calculateDistanceSpacing(
    float cameraDistance,
    const LODSettings& settings)
{
    if (cameraDistance <=
        settings.nearDistance)
    {
        return settings.nearSpacing;
    }


    if (cameraDistance >=
        settings.farDistance)
    {
        return settings.farSpacing;
    }


    float t =
        (cameraDistance -
            settings.nearDistance)
        /
        (settings.farDistance -
            settings.nearDistance);


    return
        settings.nearSpacing +
        t *
        (
            settings.farSpacing -
            settings.nearSpacing
            );
}


bool shouldRenderDistance(
    const VertexCompressed& point,
    float spacing)
{
    if (spacing <= 1.0f)
        return true;


    int step =
        max(
            1,
            static_cast<int>(
                std::round(spacing)
                )
        );


    int x =
        static_cast<int>(point.x);

    int y =
        static_cast<int>(point.y);


    return
        (std::abs(x) % step == 0) &&
        (std::abs(y) % step == 0);
}


// ============================================================
// DRAW POINT CLOUD
// ============================================================

void drawPointCloud(
    const std::vector<VertexCompressed>& cloud,
    const LODSettings& lodSettings,
    int imageWidth)
{
    // ========================================================
    // CPU TIMER
    // ========================================================

    auto cpuStart =
        std::chrono::high_resolution_clock::now();


    // ========================================================
    // CALCULATE PARAMETERS
    // ========================================================

    float density =
        calculateLODDensity(
            cameraDistance,
            lodSettings
        );


    int uniformStep =
        calculateUniformStep(
            cameraDistance,
            lodSettings
        );


    int gridSize =
        calculateGridSize(
            cameraDistance,
            lodSettings
        );


    float distanceSpacing =
        calculateDistanceSpacing(
            cameraDistance,
            lodSettings
        );


    size_t renderedPoints = 0;


    glPointSize(2.0f);


    // ========================================================
    // DRAW
    // ========================================================

    glBegin(GL_POINTS);


    for (size_t i = 0;
        i < cloud.size();
        ++i)
    {
        bool render = true;


        // ====================================================
        // SELECT LOD METHOD
        // ====================================================

        switch (lodSettings.method)
        {
            // ----------------------------------------------------
            // NO LOD
            // ----------------------------------------------------

        case LODMethod::None:

            render = true;

            break;


            // ----------------------------------------------------
            // RANDOM
            // ----------------------------------------------------

        case LODMethod::Random:

            render =
                shouldRenderRandom(
                    i,
                    density
                );

            break;


            // ----------------------------------------------------
            // UNIFORM
            // ----------------------------------------------------

        case LODMethod::Uniform:

            render =
                shouldRenderUniform(
                    i,
                    uniformStep
                );

            break;


            // ----------------------------------------------------
            // GRID
            // ----------------------------------------------------

        case LODMethod::Grid:

            render =
                shouldRenderGrid(
                    i,
                    gridSize,
                    imageWidth
                );

            break;


            // ----------------------------------------------------
            // DISTANCE
            // ----------------------------------------------------

        case LODMethod::Distance:

            render =
                shouldRenderDistance(
                    cloud[i],
                    distanceSpacing
                );

            break;
        }


        if (!render)
            continue;


        // ====================================================
        // DECOMPRESS
        // ====================================================

        Vertex v =
            decompressVertex(
                cloud[i]
            );


        // ====================================================
        // COLOR
        // ====================================================

        glColor3f(
            v.r,
            v.g,
            v.b
        );


        // ====================================================
        // POSITION
        // ====================================================

        glVertex3f(
            v.x,
            v.y,
            v.z
        );


        renderedPoints++;
    }


    glEnd();


    // ========================================================
    // CPU TIME
    // ========================================================

    auto cpuEnd =
        std::chrono::high_resolution_clock::now();


    currentCPUTimeMs =
        std::chrono::duration<double, std::milli>(
            cpuEnd - cpuStart
        ).count();


    // ========================================================
    // STATISTICS
    // ========================================================

    static int frameCounter = 0;

    frameCounter++;


    if (frameCounter % 120 == 0)
    {
        // ====================================================
        // MEMORY
        // ====================================================

        size_t cloudMemory =
            cloud.size() *
            sizeof(VertexCompressed);


        size_t effectiveLODData =
            renderedPoints *
            sizeof(VertexCompressed);


        size_t processMemory =
            getProcessMemoryUsage();


        double cloudMB =
            cloudMemory /
            (1024.0 * 1024.0);


        double effectiveLODMB =
            effectiveLODData /
            (1024.0 * 1024.0);


        double processMB =
            processMemory /
            (1024.0 * 1024.0);


        // ====================================================
        // POINT STATISTICS
        // ====================================================

        size_t totalPoints =
            cloud.size();


        double renderedPercent =
            0.0;


        double reductionPercent =
            0.0;


        if (totalPoints > 0)
        {
            renderedPercent =
                (
                    static_cast<double>(
                        renderedPoints
                        )
                    /
                    static_cast<double>(
                        totalPoints
                        )
                    )
                * 100.0;


            reductionPercent =
                100.0 -
                renderedPercent;
        }


        // ====================================================
        // THROUGHPUT
        // ====================================================

        double pointsPerSecond =
            renderedPoints *
            currentFPS;


        double pointsPerSecondMillion =
            pointsPerSecond /
            1'000'000.0;


        // ====================================================
        // CONSOLE OUTPUT
        // ====================================================

        std::cout
            << "\rMethod: "
            << getLODMethodName(
                lodSettings.method
            )

            << " | Distance: "
            << std::fixed
            << std::setprecision(2)
            << cameraDistance

            << " | Points: "
            << renderedPoints
            << " / "
            << totalPoints

            << " ("
            << std::setprecision(1)
            << renderedPercent
            << "%)"

            << " | FPS: "
            << std::setprecision(1)
            << currentFPS

            << " | CPU: "
            << std::setprecision(2)
            << currentCPUTimeMs
            << " ms"

            << "          "
            << std::flush;


        // ====================================================
        // LOG FILE
        // ====================================================

        logFile

            << "Method: "
            << getLODMethodName(
                lodSettings.method
            )

            << " | Distance: "
            << std::fixed
            << std::setprecision(2)
            << cameraDistance

            << " | Density: "
            << std::setprecision(3)
            << density

            << " | UniformStep: "
            << uniformStep

            << " | GridSize: "
            << gridSize

            << " | DistanceSpacing: "
            << distanceSpacing

            << " | Points: "
            << renderedPoints
            << " / "
            << totalPoints

            << " ("
            << std::setprecision(2)
            << renderedPercent
            << "%)"

            << " | Reduction: "
            << reductionPercent
            << "%"

            << " | FPS: "
            << std::setprecision(1)
            << currentFPS

            << " | Frame: "
            << std::setprecision(2)
            << currentFrameTimeMs
            << " ms"

            << " | CPU: "
            << currentCPUTimeMs
            << " ms"

            << " | Cloud: "
            << cloudMB
            << " MB"

            << " | Effective LOD: "
            << effectiveLODMB
            << " MB"

            << " | RAM: "
            << processMB
            << " MB"

            << " | Draw calls: 1"

            << " | Throughput: "
            << std::setprecision(2)
            << pointsPerSecondMillion
            << " M pts/s"

            << '\n';


        logFile.flush();
    }
}


// ============================================================
// MOUSE BUTTON CALLBACK
// ============================================================

void mouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int mods)
{
    if (action == GLFW_PRESS)
    {
        // ====================================================
        // LEFT MOUSE = ROTATE
        // ====================================================

        if (button ==
            GLFW_MOUSE_BUTTON_LEFT)
        {
            rotating = true;

            glfwGetCursorPos(
                window,
                &lastMouseX,
                &lastMouseY
            );
        }


        // ====================================================
        // MIDDLE MOUSE = PAN
        // ====================================================

        if (button ==
            GLFW_MOUSE_BUTTON_MIDDLE)
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
        if (button ==
            GLFW_MOUSE_BUTTON_LEFT)
        {
            rotating = false;
        }


        if (button ==
            GLFW_MOUSE_BUTTON_MIDDLE)
        {
            panning = false;
        }
    }
}


// ============================================================
// MOUSE MOVE CALLBACK
// ============================================================

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


    // ========================================================
    // ROTATION
    // ========================================================

    if (rotating)
    {
        yaw +=
            static_cast<float>(
                deltaX
                )
            * rotateSensitivity;


        pitch +=
            static_cast<float>(
                deltaY
                )
            * rotateSensitivity;


        pitch =
            std::clamp(
                pitch,
                -89.0f,
                89.0f
            );
    }


    // ========================================================
    // PAN
    // ========================================================

    if (panning)
    {
        panX +=
            static_cast<float>(
                deltaX
                )
            * panSensitivity;


        panY -=
            static_cast<float>(
                deltaY
                )
            * panSensitivity;
    }
}


// ============================================================
// MOUSE SCROLL CALLBACK
// ============================================================

void scrollCallback(
    GLFWwindow* window,
    double xoffset,
    double yoffset)
{
    cameraDistance -=
        static_cast<float>(
            yoffset
            )
        * zoomSensitivity;


    cameraDistance =
        std::clamp(
            cameraDistance,
            minCameraDistance,
            maxCameraDistance
        );
}


// ============================================================
// PRINT LOD CONFIGURATION
// ============================================================

void printLODConfiguration(
    const LODSettings& settings)
{
    std::cout
        << "\n========================================\n";

    std::cout
        << "           LOD CONFIGURATION\n";

    std::cout
        << "========================================\n";


    std::cout
        << "Method: "
        << getLODMethodName(
            settings.method
        )
        << "\n";


    std::cout
        << "Near distance: "
        << settings.nearDistance
        << "\n";


    std::cout
        << "Far distance: "
        << settings.farDistance
        << "\n";


    if (settings.method ==
        LODMethod::Random)
    {
        std::cout
            << "Near density: "
            << settings.nearDensity
            << "\n";

        std::cout
            << "Far density: "
            << settings.farDensity
            << "\n";
    }


    if (settings.method ==
        LODMethod::Uniform)
    {
        std::cout
            << "Near step: "
            << settings.nearStep
            << "\n";

        std::cout
            << "Far step: "
            << settings.farStep
            << "\n";
    }


    if (settings.method ==
        LODMethod::Grid)
    {
        std::cout
            << "Near grid size: "
            << settings.nearGridSize
            << "\n";

        std::cout
            << "Far grid size: "
            << settings.farGridSize
            << "\n";
    }


    if (settings.method ==
        LODMethod::Distance)
    {
        std::cout
            << "Near spacing: "
            << settings.nearSpacing
            << "\n";

        std::cout
            << "Far spacing: "
            << settings.farSpacing
            << "\n";
    }


    std::cout
        << "Initial camera distance: "
        << cameraDistance
        << "\n";


    std::cout
        << "========================================\n";
}


// ============================================================
// CONSOLE MENU
// ============================================================

LODSettings selectLODMethod()
{
    LODSettings settings;


    std::cout
        << "\n========================================\n";

    std::cout
        << "          POINT CLOUD RENDERER\n";

    std::cout
        << "========================================\n";


    std::cout
        << "1. No LOD\n";

    std::cout
        << "2. Random Sampling LOD\n";

    std::cout
        << "3. Uniform / Every-N LOD\n";

    std::cout
        << "4. Grid Sampling LOD\n";

    std::cout
        << "5. Distance-Based LOD\n";

    std::cout
        << "6. Exit\n";


    std::cout
        << "\nSelect method: ";


    int choice;

    std::cin >> choice;


    switch (choice)
    {
        // ========================================================
        // NO LOD
        // ========================================================

    case 1:

        settings.method =
            LODMethod::None;


        std::cout
            << "Selected: No LOD\n";

        break;


        // ========================================================
        // RANDOM
        // ========================================================

    case 2:

        settings.method =
            LODMethod::Random;


        std::cout
            << "\nRandom Sampling LOD\n";


        std::cout
            << "Near camera distance: ";

        std::cin >>
            settings.nearDistance;


        std::cout
            << "Far camera distance: ";

        std::cin >>
            settings.farDistance;


        std::cout
            << "Near density (0.0 - 1.0): ";

        std::cin >>
            settings.nearDensity;


        std::cout
            << "Far density (0.0 - 1.0): ";

        std::cin >>
            settings.farDensity;


        settings.nearDensity =
            std::clamp(
                settings.nearDensity,
                0.0f,
                1.0f
            );


        settings.farDensity =
            std::clamp(
                settings.farDensity,
                0.0f,
                1.0f
            );


        if (settings.farDistance <=
            settings.nearDistance)
        {
            settings.farDistance =
                settings.nearDistance + 1.0f;
        }


        std::cout
            << "Selected: Random Sampling LOD\n";

        break;


        // ========================================================
        // UNIFORM
        // ========================================================

    case 3:

        settings.method =
            LODMethod::Uniform;


        std::cout
            << "\nUniform / Every-N LOD\n";


        std::cout
            << "Near camera distance: ";

        std::cin >>
            settings.nearDistance;


        std::cout
            << "Far camera distance: ";

        std::cin >>
            settings.farDistance;


        std::cout
            << "Near step (1 = all points): ";

        std::cin >>
            settings.nearStep;


        std::cout
            << "Far step: ";

        std::cin >>
            settings.farStep;


        settings.nearStep =
            max(
                1,
                settings.nearStep
            );


        settings.farStep =
            max(
                settings.nearStep,
                settings.farStep
            );


        if (settings.farDistance <=
            settings.nearDistance)
        {
            settings.farDistance =
                settings.nearDistance + 1.0f;
        }


        std::cout
            << "Selected: Uniform / Every-N LOD\n";

        break;


        // ========================================================
        // GRID
        // ========================================================

    case 4:

        settings.method =
            LODMethod::Grid;


        std::cout
            << "\nGrid Sampling LOD\n";


        std::cout
            << "Near camera distance: ";

        std::cin >>
            settings.nearDistance;


        std::cout
            << "Far camera distance: ";

        std::cin >>
            settings.farDistance;


        std::cout
            << "Near grid size: ";

        std::cin >>
            settings.nearGridSize;


        std::cout
            << "Far grid size: ";

        std::cin >>
            settings.farGridSize;


        settings.nearGridSize =
            max(
                1,
                settings.nearGridSize
            );


        settings.farGridSize =
            max(
                settings.nearGridSize,
                settings.farGridSize
            );


        if (settings.farDistance <=
            settings.nearDistance)
        {
            settings.farDistance =
                settings.nearDistance + 1.0f;
        }


        std::cout
            << "Selected: Grid Sampling LOD\n";

        break;


        // ========================================================
        // DISTANCE
        // ========================================================

    case 5:

        settings.method =
            LODMethod::Distance;


        std::cout
            << "\nDistance-Based LOD\n";


        std::cout
            << "Near camera distance: ";

        std::cin >>
            settings.nearDistance;


        std::cout
            << "Far camera distance: ";

        std::cin >>
            settings.farDistance;


        std::cout
            << "Near spacing: ";

        std::cin >>
            settings.nearSpacing;


        std::cout
            << "Far spacing: ";

        std::cin >>
            settings.farSpacing;


        settings.nearSpacing =
            max(
                1.0f,
                settings.nearSpacing
            );


        settings.farSpacing =
            max(
                settings.nearSpacing,
                settings.farSpacing
            );


        if (settings.farDistance <=
            settings.nearDistance)
        {
            settings.farDistance =
                settings.nearDistance + 1.0f;
        }


        std::cout
            << "Selected: Distance-Based LOD\n";

        break;


        // ========================================================
        // EXIT
        // ========================================================

    case 6:

        std::cout
            << "Exiting...\n";

        std::exit(0);


        // ========================================================
        // INVALID
        // ========================================================

    default:

        std::cout
            << "Invalid option. Using No LOD.\n";


        settings.method =
            LODMethod::None;

        break;
    }


    return settings;
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    // ========================================================
    // SELECT LOD
    // ========================================================

    LODSettings lodSettings =
        selectLODMethod();


    // ========================================================
    // INITIALIZE GLFW
    // ========================================================

    if (!glfwInit())
    {
        std::cout
            << "Failed to initialize GLFW\n";

        return -1;
    }


    // ========================================================
    // CREATE WINDOW
    // ========================================================

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
        std::cout
            << "Failed to create GLFW window\n";

        glfwTerminate();

        return -1;
    }


    glfwMakeContextCurrent(
        window
    );


    // ========================================================
    // CALLBACKS
    // ========================================================

    glfwSetMouseButtonCallback(
        window,
        mouseButtonCallback
    );


    glfwSetCursorPosCallback(
        window,
        cursorPositionCallback
    );


    glfwSetScrollCallback(
        window,
        scrollCallback
    );


    // ========================================================
    // VSYNC
    // ========================================================

    glfwSwapInterval(1);


    // ========================================================
    // OPENGL
    // ========================================================

    glEnable(
        GL_DEPTH_TEST
    );


    glClearColor(
        0.02f,
        0.02f,
        0.02f,
        1.0f
    );


    // ========================================================
    // LOAD POINT CLOUD
    // ========================================================

    std::vector<VertexCompressed> cloud;


    generateCompressedCloudFromImages(
        cloud,
        "assets/color.jpeg",
        "assets/depth.png",
        cloudWidth,
        cloudHeight
    );


    if (cloud.empty())
    {
        std::cout
            << "Point cloud is empty.\n";

        glfwDestroyWindow(window);

        glfwTerminate();

        return -1;
    }


    std::cout
        << "Point cloud size: "
        << cloud.size()
        << "\n";


    std::cout
        << "VertexCompressed size: "
        << sizeof(VertexCompressed)
        << " bytes\n";


    std::cout
        << "Cloud memory: "
        << (
            cloud.size() *
            sizeof(VertexCompressed)
            /
            (1024.0 * 1024.0)
            )
        << " MB\n";


    printLODConfiguration(
        lodSettings
    );


    std::cout
        << "\nControls:\n";

    std::cout
        << "Left Mouse  = Rotate\n";

    std::cout
        << "Middle Mouse = Pan\n";

    std::cout
        << "Mouse Wheel = Zoom\n\n";


    // ========================================================
    // INITIALIZE FPS TIMER
    // ========================================================

    fpsTimer =
        std::chrono::high_resolution_clock::now();


    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (!glfwWindowShouldClose(window))
    {
        // ====================================================
        // FRAME START
        // ====================================================

        auto frameStart =
            std::chrono::high_resolution_clock::now();


        // ====================================================
        // UPDATE FPS
        // ====================================================

        updateFPS();


        // ====================================================
        // CLEAR
        // ====================================================

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );


        // ====================================================
        // FRAMEBUFFER SIZE
        // ====================================================

        int width;
        int height;


        glfwGetFramebufferSize(
            window,
            &width,
            &height
        );


        if (height == 0)
            height = 1;


        glViewport(
            0,
            0,
            width,
            height
        );


        // ====================================================
        // ASPECT RATIO
        // ====================================================

        float aspect =
            static_cast<float>(width)
            /
            static_cast<float>(height);


        // ====================================================
        // PROJECTION
        // ====================================================

        glMatrixMode(
            GL_PROJECTION
        );


        glLoadIdentity();


        float orthoSize =
            0.7f *
            (6.0f / 6.0f);


        if (aspect >= 1.0f)
        {
            glOrtho(
                -orthoSize * aspect,
                orthoSize * aspect,

                -orthoSize,
                orthoSize,

                -30.0f,
                30.0f
            );
        }
        else
        {
            glOrtho(
                -orthoSize,
                orthoSize,

                -orthoSize / aspect,
                orthoSize / aspect,

                -30.0f,
                30.0f
            );
        }


        // ====================================================
        // MODEL VIEW
        // ====================================================

        glMatrixMode(
            GL_MODELVIEW
        );


        glLoadIdentity();


        glTranslatef(
            panX,
            panY,
            -cameraDistance
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


        // ====================================================
        // DRAW
        // ====================================================

        drawPointCloud(
            cloud,
            lodSettings,
            cloudWidth
        );


        // ====================================================
        // PRESENT
        // ====================================================

        glfwSwapBuffers(
            window
        );


        // ====================================================
        // EVENTS
        // ====================================================

        glfwPollEvents();


        // ====================================================
        // FRAME TIME
        // ====================================================

        auto frameEnd =
            std::chrono::high_resolution_clock::now();


        double totalFrameTime =
            std::chrono::duration<double, std::milli>(
                frameEnd - frameStart
            ).count();


        if (currentFPS > 0.0)
        {
            currentFrameTimeMs =
                totalFrameTime;
        }
    }


    // ========================================================
    // CLEANUP
    // ========================================================

    glfwDestroyWindow(
        window
    );


    glfwTerminate();


    return 0;
}