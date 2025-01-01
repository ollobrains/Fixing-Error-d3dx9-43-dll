#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>            // for FPS measurement
#include <cstdlib>          // for std::exit, EXIT_FAILURE

#include "Eigen/Core"
#include "SDL.h"
#include "refract.h" // Ensure this header declares ParseOBJ, Refract, CalculateIntersections, etc.

// Global refractive index
constexpr double ETA = 1.457; // Refractive index used to generate the lens
// Initial window dimensions
int windowWidth  = 256;
int windowHeight = 256;

/**
 * @brief Draws intersection points into an SDL renderer.
 * 
 * @param renderer        SDL renderer
 * @param intersections   2D intersection points (x,y)
 */
void DrawIntersections(SDL_Renderer* renderer,
                       const std::vector<Eigen::Vector2d>& intersections)
{
    // Clear the screen with black
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(renderer);

    // We'll scale up from the nominal 256x256 coordinate system to our window size
    float scaleX = static_cast<float>(windowWidth)  / 256.0f;
    float scaleY = static_cast<float>(windowHeight) / 256.0f;

    // Optionally, set each point color individually
    // Here we use a simple gradient approach as an example
    // If you want them all white, just remove the color logic below.
    int numPoints = static_cast<int>(intersections.size());
    for(int i = 0; i < numPoints; ++i)
    {
        // Some color variation based on x or y (simple demonstration)
        // Make sure to clamp color components to [0,255]
        Uint8 r = static_cast<Uint8>( (intersections[i].x() / 256.0) * 255.0 );
        Uint8 g = static_cast<Uint8>( (intersections[i].y() / 256.0) * 255.0 );
        Uint8 b = static_cast<Uint8>( (i             / double(numPoints)) * 255.0 );

        SDL_SetRenderDrawColor(renderer, r, g, b, 0xFF);

        float drawX = intersections[i].x() * scaleX;
        float drawY = intersections[i].y() * scaleY;
        SDL_RenderDrawPointF(renderer, drawX, drawY);
    }

    // Present the updated frame
    SDL_RenderPresent(renderer);
}

/**
 * @brief Saves the current intersection "caustics" image to a simple PPM file.
 * 
 * This example draws each intersection as a pixel in a 256x256 array 
 * (so it might look sparse if you only have a few points).
 * You might want to adapt to a better accumulation buffer for production use.
 *
 * @param filename        Name of the output file (e.g. "output.ppm")
 * @param intersections   Points to draw
 */
void SaveCausticsPPM(const std::string& filename, const std::vector<Eigen::Vector2d>& intersections)
{
    // We'll create a 256x256 buffer and fill it with black, then set white 
    // for every intersection.  This is quite naive, but is just a demonstration.
    constexpr int IMG_SIZE = 256;
    std::vector<unsigned char> buffer(IMG_SIZE * IMG_SIZE * 3, 0u);

    // Fill in white for every intersection in the 256 x 256 space
    for(const auto& pt : intersections)
    {
        int x = static_cast<int>(pt.x());
        int y = static_cast<int>(pt.y());
        if(x >= 0 && x < IMG_SIZE && y >= 0 && y < IMG_SIZE)
        {
            int idx = (y * IMG_SIZE + x) * 3;
            // Let’s do white for each intersection
            buffer[idx + 0] = 255; // R
            buffer[idx + 1] = 255; // G
            buffer[idx + 2] = 255; // B
        }
    }

    std::ofstream ofs(filename, std::ios::binary);
    if(!ofs)
    {
        std::cerr << "Failed to open " << filename << " for writing.\n";
        return;
    }

    ofs << "P6\n" << IMG_SIZE << " " << IMG_SIZE << "\n255\n";
    ofs.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    ofs.close();

    std::cout << "Saved " << filename << " (simple PPM format)\n";
}

/**
 * @brief Main entry point. 
 * Takes arguments: 
 *   1) path to OBJ file 
 *   2) distance to the receiver plane (z-direction).
 */
int main(int argc, char** argv)
{
    // Basic usage check
    if(argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <path_to_obj> <distance_to_receiver_plane>\n"
                  << "Example: " << argv[0] << " lens.obj 10.0\n";
        return EXIT_FAILURE;
    }

    // Attempt to parse user arguments
    std::string objPath   = argv[1];
    double receiverPlane  = 0.0;
    try 
    {
        receiverPlane = std::stod(argv[2]);
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error parsing receiver plane distance: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // Read OBJ data
    std::vector<Eigen::Vector3d> vertices;
    std::vector<Eigen::Vector3d> normals;

    if(!ParseOBJ(objPath, &vertices, &normals))
    {
        std::cerr << "ParseOBJ failed. Check if file path is valid: " << objPath << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "Successfully parsed OBJ: " << objPath
              << " with " << vertices.size() << " vertices.\n";

    // Refract
    std::vector<Eigen::Vector3d> refracteds; // normalized directions from each point
    Refract(normals, &refracteds, ETA);

    // Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::cerr << "SDL_Init error: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }

    // Create window & renderer
    SDL_Window* window = SDL_CreateWindow(
        "Caustics Simulation", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if(!window)
    {
        std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << "\n";
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if(!renderer)
    {
        std::cerr << "SDL_CreateRenderer error: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Calculate initial intersections
    std::vector<Eigen::Vector2d> intersections;
    CalculateIntersections(vertices, refracteds, &intersections, receiverPlane);
    DrawIntersections(renderer, intersections);

    // For basic FPS measurement
    auto lastTime = std::chrono::high_resolution_clock::now();
    int framesCount = 0;

    bool quit = false;
    SDL_Event e;
    while (!quit)
    {
        while (SDL_PollEvent(&e))
        {
            switch(e.type)
            {
            case SDL_QUIT:
                quit = true;
                break;

            case SDL_WINDOWEVENT:
                if(e.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    windowWidth  = e.window.data1;
                    windowHeight = e.window.data2;
                    DrawIntersections(renderer, intersections);
                }
                break;

            case SDL_KEYDOWN:
            {
                // We can make these adjustable step increments for convenience
                double bigStep = 1.0;
                double smallStep = 0.1;

                switch(e.key.keysym.sym)
                {
                case SDLK_w:
                    receiverPlane += smallStep;
                    CalculateIntersections(vertices, refracteds, &intersections, receiverPlane);
                    DrawIntersections(renderer, intersections);
                    break;
                case SDLK_s:
                    receiverPlane -= smallStep;
                    CalculateIntersections(vertices, refracteds, &intersections, receiverPlane);
                    DrawIntersections(renderer, intersections);
                    break;
                case SDLK_e:
                    receiverPlane += bigStep;
                    CalculateIntersections(vertices, refracteds, &intersections, receiverPlane);
                    DrawIntersections(renderer, intersections);
                    break;
                case SDLK_d:
                    receiverPlane -= bigStep;
                    CalculateIntersections(vertices, refracteds, &intersections, receiverPlane);
                    DrawIntersections(renderer, intersections);
                    break;
                case SDLK_q:
                    std::cout << "Current distance between lens and receiver plane: "
                              << receiverPlane << "\n";
                    break;
                case SDLK_p:
                    // Example: Save to a PPM file
                    SaveCausticsPPM("caustics.ppm", intersections);
                    break;
                case SDLK_ESCAPE:
                    quit = true;
                    break;
                default:
                    break;
                }
            }
            break;

            default:
                break;
            }
        }

        // FPS measurement
        ++framesCount;
        auto now = std::chrono::high_resolution_clock::now();
        auto durationSec =
          std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
        if(durationSec >= 1) // every 1 second
        {
            // Update window title with FPS
            double fps = static_cast<double>(framesCount) / durationSec;
            framesCount = 0;
            lastTime = now;

            // Construct new title
            std::string title = "Caustics Simulation - FPS: " + std::to_string(fps);
            SDL_SetWindowTitle(window, title.c_str());
        }

        // Optional: If you want to do incremental animations or real-time updates, 
        // you can recalc or re-draw here. 
        // For now, the code only updates when user changes distance or resizes window.
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

