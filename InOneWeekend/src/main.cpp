#include "raylib.h"
#include "vec3.h"
#include "color.h"
#include "ray.h"
#include "hittable.h"
#include "sphere.h"
#include "hittable_list.h"
#include "camera.h"
#include "material.h"



vec3 ray_color(const ray& r, const hittable& world, int depth) {
    hit_record rec;
    if (depth <= 0) return vec3(0,0,0);

    if (world.hit(r, 0.001, infinity, rec)) {
        ray scattered;
        vec3 attenuation;
    if (rec.mat_ptr->scatter(r, rec, attenuation, scattered))
        return attenuation * ray_color(scattered, world, depth-1);
            return vec3(0,0,0);
    }

    vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5*(unit_direction.y + 1.0);
        return (1.0 - t)*vec3(1.0,1.0,1.0) + t*vec3(0.5,0.7,1.0);
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
        auto material_ground = make_shared<lambertian>(vec3(0.8, 0.8, 0.0));
        auto material_center = make_shared<lambertian>(vec3(0.1, 0.2, 0.5));
    world.add(make_shared<sphere>(vec3(0,-100.5,-1), 100, material_ground));
    world.add(make_shared<sphere>(vec3(0,0,-1), 0.5, material_center));

    camera cam(vec3(0,0,1), vec3(0,0,-1), vec3(0,1,0), 90, double(image_width)/image_height);

    for (int j = 0; j < image_height; ++j) {
        for (int i = 0; i < image_width; ++i) {
            vec3 pixel_color(0,0,0);

            for (int s = 0; s < samples_per_pixel; ++s) {
                auto u = (i + random_double()) / (image_width-1);
                auto v = (j + random_double()) / (image_height-1);
                ray r = cam.get_ray(u,v);
                pixel_color += ray_color(r, world, max_depth);
            }
            auto r = sqrt(pixel_color.x / samples_per_pixel);
            auto g = sqrt(pixel_color.y / samples_per_pixel);
            auto b = sqrt(pixel_color.z / samples_per_pixel);
            pixels[j * image_width + i] = (Color) {
                (unsigned char)(255.99*r),
                (unsigned char)(255.99*g),
                (unsigned char)(255.99*b), 255 };
        }

        UpdateTexture(texture, pixels);

        BeginDrawing();

        ClearBackground(BLACK);

        DrawTexture(texture, 0, 0, WHITE);

        EndDrawing();
    }

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
}
