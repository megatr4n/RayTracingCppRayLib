#include "rtweekend.h"
#include "vec3.h"
#include "color.h" 
#include "ray.h"
#include "hittable.h"
#include "sphere.h"
#include "hittable_list.h"
#include "camera.h"
#include "material.h"
#include "interval.h"
#include "raylib.h"
#include <cmath>
#include <memory>
#include <thread>
#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex>

using std::make_shared;
using std::shared_ptr;

std::atomic<int> completed_rows(0);
std::mutex texture_mutex;

rt::vec3 ray_color(const rt::ray& r, const rt::hittable& world, int depth) {
    if (depth <= 0) return rt::vec3(0,0,0);

    rt::hit_record rec;

    if (world.hit(r, rt::interval(0.001, rt::infinity), rec)) {
        rt::ray scattered;
        rt::vec3 attenuation;
        if (rec.mat->scatter(r, rec, attenuation, scattered))
            return attenuation * ray_color(scattered, world, depth-1);
        return rt::vec3(0,0,0);
    }

    rt::vec3 unit_direction = rt::unit_vector(r.direction());
    auto t = 0.5*(unit_direction.y() + 1.0);
    return (1.0 - t)*rt::vec3(1.0,1.0,1.0) + t*rt::vec3(0.5,0.7,1.0);
}

void render_block(int start_row, int end_row, int width, int height, int samples, int depth,
                  rt::camera& cam, const rt::hittable_list& world, Color* pixels)
{
    for (int j = start_row; j < end_row; ++j) {
        for (int i = 0; i < width; ++i) {
            rt::vec3 pixel_color(0,0,0);
            for (int s = 0; s < samples; ++s) {
                rt::ray r = cam.get_ray(i, j);
                pixel_color += ray_color(r, world, depth);
            }
            auto scale = 1.0 / samples;
            auto r_col = sqrt(scale * pixel_color.x());
            auto g_col = sqrt(scale * pixel_color.y());
            auto b_col = sqrt(scale * pixel_color.z());

            pixels[j * width + i] = (Color) {
                (unsigned char)(256 * std::clamp(r_col, 0.0, 0.999)),
                (unsigned char)(256 * std::clamp(g_col, 0.0, 0.999)),
                (unsigned char)(256 * std::clamp(b_col, 0.0, 0.999)), 
                255
            };
        }
        completed_rows.fetch_add(1);
    }
}

int main() {
    const int image_width = 800;
    const int image_height = 450;
    const int samples_per_pixel = 50;
    const int max_depth = 10;
    
    const int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) {
        std::cout << "Could not detect number of threads, using 4" << std::endl;
    }
    const int actual_threads = (num_threads == 0) ? 4 : num_threads;
    
    std::cout << "Using " << actual_threads << " threads for rendering" << std::endl;

    SetConfigFlags(FLAG_VSYNC_HINT);
    
    InitWindow(image_width, image_height, "Multithreaded RayTracing - Raylib");
    SetTargetFPS(60);

    Color* pixels = (Color*)MemAlloc(image_width * image_height * sizeof(Color));
    
    for (int i = 0; i < image_width * image_height; i++) {
        pixels[i] = BLACK;
    }

    Image img = GenImageColor(image_width, image_height, BLACK);
    Texture2D texture = LoadTextureFromImage(img);

    rt::hittable_list world;

    auto ground_material = make_shared<rt::lambertian>(rt::vec3(0.5, 0.5, 0.5));
    world.add(make_shared<rt::sphere>(rt::vec3(0,-1000,0), 1000, ground_material));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = rt::random_double();
            rt::vec3 center(a + 0.9*rt::random_double(), 0.2, b + 0.9*rt::random_double());

            if ((center - rt::vec3(4, 0.2, 0)).length() > 0.9) {
                shared_ptr<rt::material> sphere_material;

                if (choose_mat < 0.8) {
                    auto albedo = rt::vec3::random() * rt::vec3::random();
                    sphere_material = make_shared<rt::lambertian>(albedo);
                } else if (choose_mat < 0.95) {
                    auto albedo = rt::vec3::random(0.5, 1);
                    auto fuzz = rt::random_double(0, 0.5);
                    sphere_material = make_shared<rt::metal>(albedo, fuzz);
                } else {
                    sphere_material = make_shared<rt::dielectric>(1.5);
                }

                world.add(make_shared<rt::sphere>(center, 0.2, sphere_material));
            }
        }
    }

    auto material1 = make_shared<rt::dielectric>(1.5);
    world.add(make_shared<rt::sphere>(rt::vec3(0, 1, 0), 1.0, material1));

    auto material2 = make_shared<rt::lambertian>(rt::vec3(0.4, 0.2, 0.1));
    world.add(make_shared<rt::sphere>(rt::vec3(-4, 1, 0), 1.0, material2));

    auto material3 = make_shared<rt::metal>(rt::vec3(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<rt::sphere>(rt::vec3(4, 1, 0), 1.0, material3));

    rt::camera cam;
    cam.aspect_ratio = double(image_width) / image_height;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;

    cam.vfov = 20;
    cam.lookfrom = rt::vec3(13,2,3);
    cam.lookat = rt::vec3(0,0,0);
    cam.vup = rt::vec3(0,1,0);
    cam.initialize();

    bool rendering = false;
    bool rendered = false;
    std::vector<std::thread> threads;
    
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (!rendering && !rendered && IsKeyPressed(KEY_SPACE)) {
            rendering = true;
            completed_rows.store(0);
            
            int rows_per_thread = image_height / actual_threads;
            int remaining_rows = image_height % actual_threads;
            
            threads.clear();
            threads.reserve(actual_threads);
            
            for (int t = 0; t < actual_threads; t++) {
                int start_row = t * rows_per_thread;
                int end_row = start_row + rows_per_thread;
                
                if (t == actual_threads - 1) {
                    end_row += remaining_rows;
                }
                
                threads.emplace_back(render_block, start_row, end_row, image_width, 
                                   image_height, samples_per_pixel, max_depth, 
                                   std::ref(cam), std::cref(world), pixels);
            }
            
            std::cout << "Started rendering with " << actual_threads << " threads..." << std::endl;
        }
        
        if (rendering && !rendered) {
            int current_completed = completed_rows.load();
            
            bool all_finished = true;
            for (auto& thread : threads) {
                if (thread.joinable()) {
                    all_finished = false;
                    break;
                }
            }
            
            if (current_completed >= image_height || all_finished) {
                for (auto& thread : threads) {
                    if (thread.joinable()) {
                        thread.join();
                    }
                }
                
                UpdateTexture(texture, pixels);
                rendering = false;
                rendered = true;
                std::cout << "\nRendering completed!" << std::endl;
            } else {
                static int last_update = 0;
                if (current_completed - last_update >= 10) { 
                    UpdateTexture(texture, pixels);
                    last_update = current_completed;
                }
                
                float progress = (float)current_completed / image_height;
                DrawText(TextFormat("Rendering: %.1f%% (%d/%d threads active)", 
                        progress * 100, actual_threads, actual_threads), 10, 10, 20, BLACK);
                DrawRectangle(10, 40, (int)(400 * progress), 20, GREEN);
                DrawRectangleLines(10, 40, 400, 20, BLACK);
            }
        }

        DrawTexture(texture, 0, 0, WHITE);
        
        if (!rendering && !rendered) {
            DrawText("Press SPACE to start multithreaded rendering", 10, 10, 20, BLACK);
            DrawText(TextFormat("Will use %d threads", actual_threads), 10, 35, 16, DARKGRAY);
        } else if (rendered) {
            DrawText("Rendering complete! Press R to render again, ESC to exit", 10, 10, 20, BLACK);
            DrawText(TextFormat("Rendered with %d threads", actual_threads), 10, 35, 16, DARKGREEN);
            
            if (IsKeyPressed(KEY_R)) {
                rendered = false;
                for (int i = 0; i < image_width * image_height; i++) {
                    pixels[i] = BLACK;
                }
                UpdateTexture(texture, pixels);
            }
        }

        EndDrawing();
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    UnloadTexture(texture);
    
    MemFree(pixels);
    
    UnloadImage(img);
    
    CloseWindow();

    return 0;
}
