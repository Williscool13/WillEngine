#include <libraryTests.h>
#include <willEngine.h>

const char* testImagePath = "assets\\textures\\bouncing.png";
std::filesystem::path testModelPath = "assets\\models\\box\\Box.gltf";

void testGLM() {
    // Test vector creation and arithmetic
    glm::vec3 v1(1.0f, 2.0f, 3.0f);
    glm::vec3 v2(4.0f, 5.0f, 6.0f);
    glm::vec3 v3 = v1 + v2;

    std::cout << "Vector v3 (v1 + v2): ("
        << v3.x << ", " << v3.y << ", " << v3.z << ")\n";

    // Test matrix creation and transformation
    glm::mat4 identity = glm::mat4(1.0f); // Identity matrix
    glm::mat4 translation = glm::translate(identity, glm::vec3(10.0f, 20.0f, 30.0f));

    glm::vec4 position = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
    glm::vec4 transformedPosition = translation * position;

    std::cout << "Transformed position: ("
        << transformedPosition.x << ", "
        << transformedPosition.y << ", "
        << transformedPosition.z << ", "
        << transformedPosition.w << ")\n";

    // Test matrix multiplication
    glm::mat4 rotation = glm::rotate(identity, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 transform = translation * rotation;
    glm::vec4 transformedPosition2 = transform * position;

    std::cout << "Transformed position with rotation: ("
        << transformedPosition2.x << ", "
        << transformedPosition2.y << ", "
        << transformedPosition2.z << ", "
        << transformedPosition2.w << ")\n";
}

void sdlTestWindow() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        // Handle initialization error
        return;
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow("SDL Test Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        // Handle window creation error
        SDL_Quit();
        return;
    }

    // Main loop
    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
					quit = true;
				}
			}
        }
    }

    // Cleanup SDL
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void sdlTestDrawing() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        // Handle initialization error
        return;
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow("SDL Test Drawing",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        // Handle window creation error
        SDL_Quit();
        return;
    }

    // Get the window's surface
    SDL_Surface* surface = SDL_GetWindowSurface(window);

    // Fill the surface with a red rectangle
    SDL_Rect rect = { 100, 100, 200, 200 };
    SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 255, 0, 0));

    // Update the window surface
    SDL_UpdateWindowSurface(window);

    // Main loop
    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
            }
        }
    }

    // Cleanup SDL
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void sdlTestSTBImage() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        // Handle initialization error
        return;
    }

    // Create an SDL window
    SDL_Window* window = SDL_CreateWindow("SDL Test STB Image",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        400, 225,
        SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        // Handle window creation error
        SDL_Quit();
        return;
    }

    // Create an SDL renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        // Handle renderer creation error
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Load the image using STB Image
    int width, height, channels;
    unsigned char* image_data = stbi_load(testImagePath, &width, &height, &channels, STBI_rgb_alpha);
    if (image_data == nullptr) {
        // Handle image loading error
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Create an SDL texture from the loaded image data
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(image_data, width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        // Handle surface creation error
        stbi_image_free(image_data);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture == nullptr) {
        // Handle texture creation error
        stbi_image_free(image_data);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Clear the renderer
    SDL_RenderClear(renderer);

    // Render the texture onto the renderer
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);

    // Update the renderer
    SDL_RenderPresent(renderer);

    // Main loop
    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
            }
        }
    }

    // Cleanup SDL
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    stbi_image_free(image_data);
    SDL_Quit();
}

void testFormatString() {
    // Basic string formatting
    std::string formatted = fmt::format("Hello, {}!", "world");
    fmt::print("{}\n", formatted);
    // expects "Hello, world!\n"
}

void testFormatNumbers() {
    // Number formatting
    int num1 = 42;
    double num2 = 3.14159;
    fmt::print("Integer: {:04d}, Floating point: {:.2f}\n", num1, num2);
    // expects "Integer: 0042, Floating point: 3.14\n"
}

void testFormatCustomTypes() {
    // Custom type formatting
    struct Point {
        int x;
        int y;
    };
    Point p{ 10, 20 };
    fmt::print("Point: ({}, {})\n", p.x, p.y);
    // expects "Point: (10, 20)\n"
}

void testAlignmentPadding() {
    // Alignment and padding
    std::string name = "John";
    int age = 30;
    fmt::print("{:<10}: {:>5}\n", name, age);
    // expects "John      :    30\n"
}

void testFMT() {
    testFormatString();
	testFormatNumbers();
	testFormatCustomTypes();
	testAlignmentPadding();
}

void testGLTF() {
    if (!std::filesystem::exists(testModelPath)) {
        std::cout << "Failed to find " << testModelPath << '\n';
        return;
    }

    fastgltf::Parser parser;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(testModelPath);
    if (!bool(gltfFile)) {
        std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
        return;
    }

    auto asset = parser.loadGltf(gltfFile.get(), testModelPath.parent_path(), fastgltf::Options::None);
    if (asset.error() != fastgltf::Error::None) {
        std::cerr << "Failed to load glTF: " << fastgltf::getErrorMessage(asset.error()) << '\n';
        return;
    }

    fastgltf::Buffer buffer = asset.get().buffers[0];
    fmt::print("Buffer size: {}\n", buffer.byteLength);

    fmt::print("Accessor 0 count: {}\n", asset.get().accessors[0].count);
}