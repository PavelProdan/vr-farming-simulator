#include "raylib.h"
#include "rcamera.h"
#include "resource_dir.h"
#include "math.h"
#include <stdlib.h>
#define MAX_COLUMNS 20

int main(void)
{
    const int screenWidth = 1920;
    const int screenHeight = 1080;

    InitWindow(screenWidth, screenHeight, "VR Farming Simulator");

    // Define the camera
    Camera camera = {0};
    camera.position = (Vector3){0.0f, 5.0f, 10.0f}; // Raise camera slightly
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int cameraMode = CAMERA_FIRST_PERSON;

    // Update the texture loading part of your code
    SearchAndSetResourceDir("resources");
    // Change the filename to match your downloaded texture
    Texture2D terrainTexture = LoadTexture("textures/rocky_terrain_02_diff_8k.jpg");

    // Optimize texture quality
    SetTextureFilter(terrainTexture, TEXTURE_FILTER_ANISOTROPIC_16X);
    SetTextureWrap(terrainTexture, TEXTURE_WRAP_REPEAT);

    // Generate a more detailed mesh for the terrain
    Mesh terrainMesh = GenMeshPlane(32.0f, 32.0f, 64, 64);

    // Adjust texture tiling for higher detail
    float textureTilingX = 4.0f;
    float textureTilingZ = 4.0f;

    // Update UV mapping for better texture repetition
    for (int i = 0; i < terrainMesh.vertexCount; i++)
    {
        terrainMesh.texcoords[i * 2] *= textureTilingX;
        terrainMesh.texcoords[i * 2 + 1] *= textureTilingZ;
    }

    Model terrainModel = LoadModelFromMesh(terrainMesh);
    terrainModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = terrainTexture;
    // Add this at the end of your initialization and before the game loop
    SetMaterialTexture(&terrainModel.materials[0], MATERIAL_MAP_DIFFUSE, terrainTexture);


    // Load GLB model
    Model myModel = LoadModel("humans/walking_character.glb");  // Change path to your model file
    ModelAnimation* animations = NULL;
    unsigned int animCount = 0;
    int animFrameCounter = 0;
    int currentAnimation = 0;
    // Load animations from the same file
    animations = LoadModelAnimations("humans/walking_character.glb", &animCount);
    if (animCount > 0) {
        TraceLog(LOG_INFO, "Model has %d animations", animCount);
    } else {
        TraceLog(LOG_WARNING, "Model has no animations");
    }// Horse movement variables
Vector3 modelPosition = (Vector3){ 0.0f, 0.0f, 0.0f };  // Position in world
float modelScale = 1.0f;                               // Adjust scale as needed
Vector3 moveDirection = (Vector3){ 0.0f, 0.0f, 0.0f }; // Movement direction
float moveSpeed = 0.01f;                              // Movement speed
float moveTimer = 0.0f;                               // Timer for direction changes
float moveInterval = 3.0f;                            // Seconds between direction changes
bool isMoving = false;                                // Whether horse is moving
float rotationAngle = 0.0f;                           // Rotation angle for the model



// Adjust scale as needed

    // Generate random columns (your existing code)
    float heights[MAX_COLUMNS] = {0};
    Vector3 positions[MAX_COLUMNS] = {0};
    Color colors[MAX_COLUMNS] = {0};

    for (int i = 0; i < MAX_COLUMNS; i++)
    {
        heights[i] = (float)GetRandomValue(1, 12);
        positions[i] = (Vector3){(float)GetRandomValue(-15, 15), heights[i] / 2.0f, (float)GetRandomValue(-15, 15)};
        colors[i] = (Color){GetRandomValue(20, 255), GetRandomValue(10, 55), 30, 255};
    }

    DisableCursor();
    SetTargetFPS(90);

    while (!WindowShouldClose())
    {
        // Update camera mode (your existing code remains the same)
        if (IsKeyPressed(KEY_ONE))
            cameraMode = CAMERA_FREE;
        if (IsKeyPressed(KEY_TWO))
            cameraMode = CAMERA_FIRST_PERSON;
        if (IsKeyPressed(KEY_THREE))
            cameraMode = CAMERA_THIRD_PERSON;
        if (IsKeyPressed(KEY_FOUR))
            cameraMode = CAMERA_ORBITAL;      

        UpdateCamera(&camera, cameraMode);

        // movement for the horse
        // Update horse movement
moveTimer += GetFrameTime();
        
// Change direction or state every few seconds
if (moveTimer >= moveInterval) {
    moveTimer = 0.0f;
    
    // 70% chance to move, 30% chance to stay idle
    if (GetRandomValue(0, 100) < 70) {
        isMoving = true;
        // Random direction in X and Z (keep Y at 0 for ground movement)
        moveDirection.x = GetRandomValue(-10, 10) / 10.0f;
        moveDirection.z = GetRandomValue(-10, 10) / 10.0f;
        
        // Normalize direction
        float length = sqrtf(moveDirection.x*moveDirection.x + moveDirection.z*moveDirection.z);
        if (length > 0) {
            moveDirection.x /= length;
            moveDirection.z /= length;
            
            // Calculate rotation angle based on direction
            rotationAngle = atan2f(moveDirection.x, moveDirection.z) * RAD2DEG;
        }
    } else {
        isMoving = false;
    }
    
    // Randomize next interval (1-3 seconds)
    moveInterval = 1.0f + GetRandomValue(0, 20) / 10.0f;
}

// Update position if moving
if (isMoving) {
    modelPosition.x += moveDirection.x * moveSpeed;
    modelPosition.z += moveDirection.z * moveSpeed;
    
    // Keep within boundaries (adjust based on your terrain size)
    if (modelPosition.x < -15.0f) modelPosition.x = -15.0f;
    if (modelPosition.x > 15.0f) modelPosition.x = 15.0f;
    if (modelPosition.z < -15.0f) modelPosition.z = -15.0f;
    if (modelPosition.z > 15.0f) modelPosition.z = 15.0f;
}


        // Draw
        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(camera);

        // DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 32.0f, 32.0f }, LIGHTGRAY); // Draw ground
        DrawModel(terrainModel, (Vector3){0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

        DrawCube((Vector3){-16.0f, 2.5f, 0.0f}, 1.0f, 5.0f, 32.0f, BLUE); // Draw a blue wall
        DrawCube((Vector3){16.0f, 2.5f, 0.0f}, 1.0f, 5.0f, 32.0f, LIME);  // Draw a green wall
        DrawCube((Vector3){0.0f, 2.5f, 16.0f}, 32.0f, 5.0f, 1.0f, GOLD);  // Draw a yellow wall

        // Update animation
if (animCount > 0) {
    // Update animation frame counter
    animFrameCounter++;
    
    // Update model animation
    UpdateModelAnimation(myModel, animations[currentAnimation], animFrameCounter);
    
    // Loop animation when it reaches the end
    if (animFrameCounter >= animations[currentAnimation].frameCount) {
        animFrameCounter = 0;
    }
}

// Draw the horse model with rotation
DrawModelEx(myModel, 
    modelPosition, 
    (Vector3){0.0f, 1.0f, 0.0f},  // Rotation axis (Y-axis)
    rotationAngle,                // Rotation angle
    (Vector3){modelScale, modelScale, modelScale}, 
    WHITE);

        // Draw some cubes around
        for (int i = 0; i < MAX_COLUMNS; i++)
        {
            DrawCube(positions[i], 2.0f, heights[i], 2.0f, colors[i]);
            DrawCubeWires(positions[i], 2.0f, heights[i], 2.0f, MAROON);
        }

        // Draw player cube
        if (cameraMode == CAMERA_THIRD_PERSON)
        {
            DrawCube(camera.target, 0.5f, 0.5f, 0.5f, PURPLE);
            DrawCubeWires(camera.target, 0.5f, 0.5f, 0.5f, DARKPURPLE);
        }

        EndMode3D();

        // Draw info boxes
        DrawRectangle(5, 5, 330, 100, Fade(SKYBLUE, 0.5f));
        DrawRectangleLines(5, 5, 330, 100, BLUE);

        DrawText("Camera controls:", 15, 15, 10, BLACK);
        DrawText("- Move keys: W, A, S, D, Space, Left-Ctrl", 15, 30, 10, BLACK);
        DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);
        DrawText("- Camera mode keys: 1, 2, 3, 4", 15, 60, 10, BLACK);
        DrawText("- Zoom keys: num-plus, num-minus or mouse scroll", 15, 75, 10, BLACK);
        DrawText("- Camera projection key: P", 15, 90, 10, BLACK);

        DrawRectangle(600, 5, 195, 100, Fade(SKYBLUE, 0.5f));
        DrawRectangleLines(600, 5, 195, 100, BLUE);

        DrawText("Camera status:", 610, 15, 10, BLACK);
        DrawText(TextFormat("- Mode: %s", (cameraMode == CAMERA_FREE) ? "FREE" : (cameraMode == CAMERA_FIRST_PERSON) ? "FIRST_PERSON"
                                                                             : (cameraMode == CAMERA_THIRD_PERSON)   ? "THIRD_PERSON"
                                                                             : (cameraMode == CAMERA_ORBITAL)        ? "ORBITAL"
                                                                                                                     : "CUSTOM"),
                 610, 30, 10, BLACK);
        DrawText(TextFormat("- Projection: %s", (camera.projection == CAMERA_PERSPECTIVE) ? "PERSPECTIVE" : (camera.projection == CAMERA_ORTHOGRAPHIC) ? "ORTHOGRAPHIC"
                                                                                                                                                       : "CUSTOM"),
                 610, 45, 10, BLACK);
        DrawText(TextFormat("- Position: (%06.3f, %06.3f, %06.3f)", camera.position.x, camera.position.y, camera.position.z), 610, 60, 10, BLACK);
        DrawText(TextFormat("- Target: (%06.3f, %06.3f, %06.3f)", camera.target.x, camera.target.y, camera.target.z), 610, 75, 10, BLACK);
        DrawText(TextFormat("- Up: (%06.3f, %06.3f, %06.3f)", camera.up.x, camera.up.y, camera.up.z), 610, 90, 10, BLACK);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    UnloadTexture(terrainTexture);
    UnloadModel(terrainModel);
    UnloadModel(myModel);
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}