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


using std::make_shared;


vec3 ray_color(const ray& r, const hittable& world, int depth) {
    if (depth <= 0) return vec3(0,0,0);

    hit_record rec;

    if (world.hit(r, interval(0.001, infinity), rec)) {
        ray scattered;
        vec3 attenuation;
        if (rec.mat->scatter(r, rec, attenuation, scattered))
            return attenuation * ray_color(scattered, world, depth-1);
        return vec3(0,0,0);
    }

    vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5*(unit_direction.y() + 1.0);
        return (1.0 - t)*vec3(1.0,1.0,1.0) + t*vec3(0.5,0.7,1.0);
}

void render_block(int start_row, int end_row, int width, int height, int samples, int depth,
                  camera& cam, hittable_list& world, Color* pixels)
{
    for (int j = start_row; j < end_row; ++j) {
        for (int i = 0; i < width; ++i) {
            vec3 pixel_color(0,0,0);
            for (int s = 0; s < samples; ++s) {
                ray r = cam.get_ray(i, j);
                pixel_color += ray_color(r, world, depth);
            }
            auto r_col = sqrt(pixel_color.x()/samples);
            auto g_col = sqrt(pixel_color.y()/samples);
            auto b_col = sqrt(pixel_color.z()/samples);

            pixels[j * width + i] = (Color) {
                (unsigned char)(255.99*r_col),
                (unsigned char)(255.99*g_col),
                (unsigned char)(255.99*b_col), 255
            };
        }
    }
}

int main() {
    const int image_width = 800;
    const int image_height = 450;
    const int samples_per_pixel = 50;
    const int max_depth = 10;

    InitWindow(image_width, image_height, "RayTracing in One Weekend - Raylib");
    SetTargetFPS(60);

    Image img = GenImageColor(image_width, image_height, BLACK);
    Texture2D texture = LoadTextureFromImage(img);
    Color* pixels = (Color*)MemAlloc(image_width * image_height * sizeof(Color));

    hittable_list world;
        auto material_ground = make_shared<lambertian>(vec3(0.8,0.8,0.0));
        auto material_center = make_shared<lambertian>(vec3(0.1,0.2,0.5));
    world.add(make_shared<sphere>(vec3(0,-100.5,-1), 100, material_ground));
    world.add(make_shared<sphere>(vec3(0,0,-1), 0.5, material_center));

    camera cam;
    cam.lookfrom = vec3(0,0,1);
    cam.lookat   = vec3(0,0,-1);
    cam.vup      = vec3(0,1,0);
    cam.vfov     = 90;
    cam.aspect_ratio = double(image_width)/image_height;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.initialize();

    int num_threads = std::thread::hardware_concurrency();
        if(num_threads == 0) num_threads = 4;
        std::vector<std::thread> threads;
    int rows_per_thread = image_height / num_threads;

    for (int t = 0; t < num_threads; ++t) {
        int start_row = t * rows_per_thread;
        int end_row = (t == num_threads-1) ? image_height : start_row + rows_per_thread;
        threads.emplace_back(render_block, start_row, end_row, image_width, image_height,
                             samples_per_pixel, max_depth, std::ref(cam), std::ref(world), pixels);
    }

    for (auto& th : threads) th.join();

    UpdateTexture(texture, pixels);

    while (!WindowShouldClose()) {
        BeginDrawing();
        
        ClearBackground(BLACK);
        
        DrawTexture(texture, 0, 0, WHITE);
        
        EndDrawing();
    }

    UnloadTexture(texture);
    
    MemFree(pixels);
    
    UnloadImage(img);
    
    CloseWindow();
    
    return 0;
}
