#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>      // for std::exit
#include <Eigen/Core>
#include <SDL.h>

#include "refract.h"

// The refractive index used to generate the lens
static const double REFRACTIVE_INDEX = 1.457;

// Window dimensions (default)
static int WINDOW_WIDTH  = 256;
static int WINDOW_HEIGHT = 256;

/**
 * \brief Clears the renderer to black and draws all intersection points onto the screen.
 * \param renderer      The SDL renderer to draw to.
 * \param intersections A list of 2D points indicating intersection positions.
 */
static void DrawIntersections(SDL_Renderer* renderer,
                              const std::vector<Eigen::Vector2d>& intersections)
{
    const float scaleX = float(WINDOW_WIDTH)  / 256.0f;
    const float scaleY = float(WINDOW_HEIGHT) / 256.0f;

    // Clear screen with black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw intersection points in white
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (const auto& pt : intersections)
    {
        // Scale up to match the resizing of our display
        SDL_RenderDrawPointF(renderer, float(pt.x() * scaleX), float(pt.y() * scaleY));
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char** argv)
{
    // Basic input validation
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <path/to/lens.obj> <initial_distance>\n";
        return EXIT_FAILURE;
    }

    // Attempt to parse user arguments
    const std::string objFilePath = argv[1];
    double receiverPlaneZ = 0.0;
    try
    {
        receiverPlaneZ = std::stod(argv[2]);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to parse receiver-plane distance: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // Load data from OBJ
    std::vector<Eigen::Vector3d> vertices;
    std::vector<Eigen::Vector3d> normals;
    if (!ParseOBJ(objFilePath.c_str(), &vertices, &normals))
    {
        std::cerr << "Failed to parse OBJ file: " << objFilePath << "\n";
        return EXIT_FAILURE;
    }

    // Refract: find the refracted directions for each vertex point
    std::vector<Eigen::Vector3d> refractedDirs;
    if (!Refract(normals, &refractedDirs, REFRACTIVE_INDEX))
    {
        std::cerr << "Error in Refract() routine.\n";
        return EXIT_FAILURE;
    }

    // Calculate intersection points on a plane located at receiverPlaneZ.
    std::vector<Eigen::Vector2d> intersections;
    if (!CalculateIntersections(vertices, refractedDirs, &intersections, receiverPlaneZ))
    {
        std::cerr << "Error in CalculateIntersections() routine.\n";
        return EXIT_FAILURE;
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }

    // Create a window and its renderer
    SDL_Window* window = SDL_CreateWindow("Caustics Image",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WINDOW_WIDTH,
                                          WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        std::cerr << "Failed to create SDL renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Initial draw
    DrawIntersections(renderer, intersections);

    // Main loop
    bool quit = false;
    while (!quit)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e.type == SDL_WINDOWEVENT &&
                     e.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                WINDOW_WIDTH  = e.window.data1;
                WINDOW_HEIGHT = e.window.data2;
                DrawIntersections(renderer, intersections);
            }
            else if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                    case SDLK_w: // Move receiver plane away
                        receiverPlaneZ += 0.1;
                        CalculateIntersections(vertices, refractedDirs, &intersections, receiverPlaneZ);
                        DrawIntersections(renderer, intersections);
                        break;
                    case SDLK_s: // Move receiver plane closer
                        receiverPlaneZ -= 0.1;
                        CalculateIntersections(vertices, refractedDirs, &intersections, receiverPlaneZ);
                        DrawIntersections(renderer, intersections);
                        break;
                    case SDLK_q: // Print current distance
                        std::cout << "Current distance between wall and lens: "
                                  << receiverPlaneZ << "\n";
                        break;
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    default:
                        break;
                }
            }
        }
        // We could do other processing here if desired
    }

    // Clean up and exit
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
