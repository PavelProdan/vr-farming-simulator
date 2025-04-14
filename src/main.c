#include "raylib.h"
#include "rcamera.h"
#include "resource_dir.h"
#include "raymath.h"    // For Vector3Distance
#include "rlgl.h"       // For rl* functions
#include "math.h"
#include <stdlib.h>
#include <stdio.h>
#define MAX_COLUMNS 20
#define MAX_ANIMALS 100
#define MAX_TERRAIN_CHUNKS 9   // 3x3 grid of chunks
#define CHUNK_SIZE 64.0f      // Size of each terrain chunk
#define CAMERA_MOVE_SPEED 0.2f // Camera movement speed

// Define LIGHTGREEN color if it's not defined in raylib
#ifndef LIGHTGREEN
    #define LIGHTGREEN (Color){ 200, 255, 200, 255 }
#endif

// Terrain chunk structure
typedef struct {
    Model model;
    Vector2 position;    // Grid position (in chunk coordinates)
    Vector3 worldPos;    // World position
    bool active;
} TerrainChunk;

// Animal types enum
typedef enum {
    ANIMAL_HORSE,
    ANIMAL_CAT,
    ANIMAL_DOG,
    ANIMAL_COW,
    ANIMAL_CHICKEN,
    ANIMAL_PIG,
    ANIMAL_COUNT
} AnimalType;

// Animal struct to store per-animal data
typedef struct {
    AnimalType type;
    Model walkingModel;
    Model idleModel;
    ModelAnimation* walkingAnim;
    ModelAnimation* idleAnim;
    int walkingAnimCount;
    int idleAnimCount;
    int animFrameCounter;
    
    Vector3 position;
    Vector3 direction;
    float scale;
    float speed;
    float rotationAngle;
    float moveTimer;
    float moveInterval;
    bool isMoving;
    bool active;
} Animal;

// Global array of animals
Animal animals[MAX_ANIMALS];
int animalCount = 0;
int animalCountByType[ANIMAL_COUNT] = {0};

// Global array of terrain chunks
TerrainChunk terrainChunks[MAX_TERRAIN_CHUNKS];

// Function to initialize a new animal
void InitAnimal(Animal* animal, AnimalType type, Vector3 position) {
    animal->type = type;
    animal->position = position;
    animal->direction = (Vector3){0.0f, 0.0f, 0.0f};
    animal->animFrameCounter = 0;
    animal->moveTimer = 0.0f;
    animal->moveInterval = 1.0f + GetRandomValue(0, 20) / 10.0f; // 1-3 seconds
    animal->isMoving = false;
    animal->rotationAngle = 0.0f;
    animal->active = true;
    
    // Set type-specific properties
    switch(type) {
        case ANIMAL_HORSE:
            animal->walkingModel = LoadModel("animals/walking_horse.glb");
            animal->idleModel = LoadModel("animals/idle_horse.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_horse.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_horse.glb", &animal->idleAnimCount);
            animal->scale = 1.0f;
            animal->speed = 0.02f;
            break;
        case ANIMAL_CAT:
            animal->walkingModel = LoadModel("animals/walking_cat.glb");
            animal->idleModel = LoadModel("animals/idle_cat.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_cat.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_cat.glb", &animal->idleAnimCount);
            animal->scale = 0.5f;
            animal->speed = 0.03f;
            break;
        case ANIMAL_DOG:
            animal->walkingModel = LoadModel("animals/walking_dog.glb");
            animal->idleModel = LoadModel("animals/idle_dog.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_dog.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_dog.glb", &animal->idleAnimCount);
            animal->scale = 0.7f;
            animal->speed = 0.025f;
            break;
        case ANIMAL_COW:
            animal->walkingModel = LoadModel("animals/walking_cow.glb");
            animal->idleModel = LoadModel("animals/idle_cow.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_cow.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_cow.glb", &animal->idleAnimCount);
            animal->scale = 0.12f;  // Reduced from 1.2f by 10x
            animal->speed = 0.015f;
            break;
        case ANIMAL_CHICKEN:
            animal->walkingModel = LoadModel("animals/walking_chicken.glb");
            animal->idleModel = LoadModel("animals/idle_chicken.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_chicken.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_chicken.glb", &animal->idleAnimCount);
            animal->scale = 0.4f;
            animal->speed = 0.02f;
            break;
        case ANIMAL_PIG:
            animal->walkingModel = LoadModel("animals/walking_pig.glb");
            animal->idleModel = LoadModel("animals/idle_pig.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_pig.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_pig.glb", &animal->idleAnimCount);
            animal->scale = 0.16f;  // Reduced from 0.8f by 5x
            animal->speed = 0.018f;
            break;
        case ANIMAL_COUNT:
        default:
            // This is not a valid animal type
            TraceLog(LOG_ERROR, "Invalid animal type: %d", type);
            animal->walkingModel = (Model){0};
            animal->idleModel = (Model){0};
            animal->walkingAnim = NULL;
            animal->idleAnim = NULL;
            animal->walkingAnimCount = 0;
            animal->idleAnimCount = 0;
            animal->scale = 1.0f;
            animal->speed = 0.0f;
            animal->active = false;
            break;
    }
    
    // Log if animations loaded successfully
    if (animal->walkingAnimCount > 0) {
        TraceLog(LOG_INFO, "Walking animation loaded for animal type %d", type);
    } else {
        TraceLog(LOG_WARNING, "No walking animations found for animal type %d", type);
    }
    
    if (animal->idleAnimCount > 0) {
        TraceLog(LOG_INFO, "Idle animation loaded for animal type %d", type);
    } else {
        TraceLog(LOG_WARNING, "No idle animations found for animal type %d", type);
    }
}

// Function to update animal position and state
void UpdateAnimal(Animal* animal, float terrainSize) {
    // Update timer for state changes
    animal->moveTimer += GetFrameTime();
    
    // Change direction or state after interval expires
    if (animal->moveTimer >= animal->moveInterval) {
        animal->moveTimer = 0.0f;
        
        // 70% chance to move, 30% chance to stay idle
        if (GetRandomValue(0, 100) < 70) {
            animal->isMoving = true;
            
            // Random direction in X and Z (keep Y at 0 for ground movement)
            animal->direction.x = GetRandomValue(-10, 10) / 10.0f;
            animal->direction.z = GetRandomValue(-10, 10) / 10.0f;
            
            // Normalize direction
            float length = sqrtf(animal->direction.x * animal->direction.x + animal->direction.z * animal->direction.z);
            if (length > 0) {
                animal->direction.x /= length;
                animal->direction.z /= length;
                
                // Calculate rotation angle based on direction
                animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
            }
        } else {
            animal->isMoving = false;
        }
        
        // Randomize next interval (1-3 seconds)
        animal->moveInterval = 1.0f + GetRandomValue(0, 20) / 10.0f;
    }
    
    // Update position if animal is moving
    if (animal->isMoving) {
        animal->position.x += animal->direction.x * animal->speed;
        animal->position.z += animal->direction.z * animal->speed;
        
        // Keep within terrain boundaries
        float boundary = terrainSize/2.0f - 2.0f;
        if (animal->position.x < -boundary) animal->position.x = -boundary;
        if (animal->position.x > boundary) animal->position.x = boundary;
        if (animal->position.z < -boundary) animal->position.z = -boundary;
        if (animal->position.z > boundary) animal->position.z = boundary;
        
        // Check for collisions with other animals
        for (int i = 0; i < animalCount; i++) {
            if (!animals[i].active || &animals[i] == animal) continue;
            
            // Simple collision detection
            float distance = Vector3Distance(animal->position, animals[i].position);
            float minDistance = (animal->scale + animals[i].scale) * 0.8f;
            
            if (distance < minDistance) {
                // Move away from the other animal
                Vector3 awayDir = {
                    animal->position.x - animals[i].position.x,
                    0,
                    animal->position.z - animals[i].position.z
                };
                
                float len = sqrtf(awayDir.x * awayDir.x + awayDir.z * awayDir.z);
                if (len > 0) {
                    awayDir.x /= len;
                    awayDir.z /= len;
                    
                    animal->position.x += awayDir.x * animal->speed * 2.0f;
                    animal->position.z += awayDir.z * animal->speed * 2.0f;
                    
                    // Update direction and angle
                    animal->direction = awayDir;
                    animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
                }
                
                // Reset timer to make a new decision sooner
                animal->moveTimer = animal->moveInterval - 0.5f;
            }
        }
    }
    
    // Update animation
    animal->animFrameCounter++;
    
    // Update the appropriate animation based on movement state
    if (animal->isMoving && animal->walkingAnimCount > 0) {
        UpdateModelAnimation(animal->walkingModel, animal->walkingAnim[0], animal->animFrameCounter);
        if (animal->animFrameCounter >= animal->walkingAnim[0].frameCount) {
            animal->animFrameCounter = 0;
        }
    } else if (animal->idleAnimCount > 0) {
        UpdateModelAnimation(animal->idleModel, animal->idleAnim[0], animal->animFrameCounter);
        if (animal->animFrameCounter >= animal->idleAnim[0].frameCount) {
            animal->animFrameCounter = 0;
        }
    }
}

// Function to spawn an animal of specified type
void SpawnAnimal(AnimalType type, Vector3 position, float terrainSize) {
    if (animalCount >= MAX_ANIMALS) {
        TraceLog(LOG_WARNING, "Cannot spawn more animals - maximum limit reached.");
        return;
    }
    
    InitAnimal(&animals[animalCount], type, position);
    animalCountByType[type]++;
    animalCount++;
}

// Helper to generate a random position for spawning animals with collision detection
Vector3 GetRandomSpawnPosition(float terrainSize, Camera camera) {
    Vector3 position;
    bool validPosition = false;
    int attempts = 0;
    float minDistance = 2.0f;  // Minimum distance between animals
    
    while (!validPosition && attempts < 50) {
        // Generate position near the camera
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float distance = GetRandomValue(3, 20);  // 3-20 units from camera
        
        position.x = camera.position.x + cosf(angle) * distance;
        position.z = camera.position.z + sinf(angle) * distance;
        position.y = 0.0f;  // On the ground
        
        // Check if position is within terrain bounds
        float boundary = terrainSize/2.0f - 2.0f;
        if (position.x < -boundary || position.x > boundary || position.z < -boundary || position.z > boundary) {
            attempts++;
            continue;
        }
        
        // Check for collisions with other animals
        validPosition = true;
        for (int i = 0; i < animalCount; i++) {
            if (!animals[i].active) continue;
            
            float dist = Vector3Distance(position, animals[i].position);
            if (dist < minDistance) {
                validPosition = false;
                break;
            }
        }
        
        attempts++;
    }
    
    return position;
}

// Function to spawn multiple animals at once
void SpawnMultipleAnimals(AnimalType type, int count, float terrainSize, Camera camera) {
    for (int i = 0; i < count && animalCount < MAX_ANIMALS; i++) {
        Vector3 position = GetRandomSpawnPosition(terrainSize, camera);
        SpawnAnimal(type, position, terrainSize);
    }
}

// Draw all animals
void DrawAnimals(void) {
    for (int i = 0; i < animalCount; i++) {
        if (!animals[i].active) continue;
        
        Animal *animal = &animals[i];
        
        // Get the appropriate model based on state
        Model modelToDraw = (animal->isMoving) ? animal->walkingModel : animal->idleModel;
        
        // Draw the animal with its original texture
        DrawModelEx(modelToDraw,
                   animal->position,
                   (Vector3){0.0f, 1.0f, 0.0f},  // Rotation axis (Y-axis)
                   animal->rotationAngle,        // Rotation angle
                   (Vector3){animal->scale, animal->scale, animal->scale},
                   WHITE);
    }
}

// Unload all animal resources
void UnloadAnimalResources(void) {
    for (int i = 0; i < ANIMAL_COUNT; i++) {
        UnloadModel(animals[i].walkingModel);
        UnloadModel(animals[i].idleModel);
        
        if (animals[i].walkingAnim != NULL && animals[i].walkingAnimCount > 0) {
            UnloadModelAnimations(animals[i].walkingAnim, animals[i].walkingAnimCount);
        }
        
        if (animals[i].idleAnim != NULL && animals[i].idleAnimCount > 0) {
            UnloadModelAnimations(animals[i].idleAnim, animals[i].idleAnimCount);
        }
    }
}

// Function to initialize a terrain chunk
void InitTerrainChunk(TerrainChunk* chunk, Vector2 position, Texture2D terrainTexture) {
    chunk->position = position;
    chunk->worldPos = (Vector3){ position.x * CHUNK_SIZE, 0.0f, position.y * CHUNK_SIZE };
    chunk->active = true;

    // Generate a more detailed mesh for the terrain
    Mesh terrainMesh = GenMeshPlane(CHUNK_SIZE, CHUNK_SIZE, 128, 128);  // Increased mesh detail

    // Adjust texture tiling for higher detail
    float textureTilingX = 4.0f;
    float textureTilingZ = 4.0f;

    // Update UV mapping for better texture repetition
    for (int i = 0; i < terrainMesh.vertexCount; i++)
    {
        terrainMesh.texcoords[i * 2] *= textureTilingX;
        terrainMesh.texcoords[i * 2 + 1] *= textureTilingZ;
    }

    chunk->model = LoadModelFromMesh(terrainMesh);
    chunk->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = terrainTexture;
    SetMaterialTexture(&chunk->model.materials[0], MATERIAL_MAP_DIFFUSE, terrainTexture);
}

// Function to update terrain chunks around the player
void UpdateTerrainChunks(Vector3 playerPosition, Texture2D terrainTexture) {
    Vector2 playerChunkPos = { floorf(playerPosition.x / CHUNK_SIZE), floorf(playerPosition.z / CHUNK_SIZE) };

    int chunkIndex = 0;
    for (int z = -1; z <= 1; z++) {
        for (int x = -1; x <= 1; x++) {
            Vector2 chunkPos = { playerChunkPos.x + x, playerChunkPos.y + z };
            terrainChunks[chunkIndex].position = chunkPos;
            terrainChunks[chunkIndex].worldPos = (Vector3){ chunkPos.x * CHUNK_SIZE, 0.0f, chunkPos.y * CHUNK_SIZE };
            if (!terrainChunks[chunkIndex].active) {
                InitTerrainChunk(&terrainChunks[chunkIndex], chunkPos, terrainTexture);
            }
            chunkIndex++;
        }
    }
}

// Function to draw terrain chunks
void DrawTerrainChunks(void) {
    for (int i = 0; i < MAX_TERRAIN_CHUNKS; i++) {
        if (terrainChunks[i].active) {
            DrawModel(terrainChunks[i].model, terrainChunks[i].worldPos, 1.0f, WHITE);
        }
    }
}

// Custom camera update function to fix the W+D movement lag
void UpdateCameraCustom(Camera *camera, int mode)
{
    // Camera movement speed vectors
    Vector3 moveVec = { 0.0f, 0.0f, 0.0f };
    float speed = CAMERA_MOVE_SPEED;

    // Keyboard inputs for all directions simultaneously
    if (IsKeyDown(KEY_W)) moveVec.z -= 1.0f;
    if (IsKeyDown(KEY_S)) moveVec.z += 1.0f;
    if (IsKeyDown(KEY_A)) moveVec.x -= 1.0f;
    if (IsKeyDown(KEY_D)) moveVec.x += 1.0f;

    // Normalize movement vector if moving diagonally
    if ((moveVec.x != 0.0f) && (moveVec.z != 0.0f))
    {
        float len = sqrtf(moveVec.x*moveVec.x + moveVec.z*moveVec.z);
        if (len > 0.0f)
        {
            moveVec.x /= len;
            moveVec.z /= len;
        }
    }

    // Handle camera modes
    if (mode == CAMERA_FIRST_PERSON || mode == CAMERA_THIRD_PERSON)
    {
        // Get camera up vector
        Vector3 up = { 0.0f, 1.0f, 0.0f };

        // Calculate translation vector based on camera view direction
        Vector3 forward = Vector3Subtract(camera->target, camera->position);
        forward.y = 0.0f; // Remove vertical component for horizontal movement
        forward = Vector3Normalize(forward);

        // Calculate right vector as cross product of forward and up vectors
        Vector3 right = Vector3CrossProduct(forward, up);

        // Calculate target position based on input and view directions
        Vector3 translation = { 0 };
        translation = Vector3Add(translation, Vector3Scale(forward, -moveVec.z * speed));
        translation = Vector3Add(translation, Vector3Scale(right, moveVec.x * speed));

        // Apply movement
        camera->position = Vector3Add(camera->position, translation);
        camera->target = Vector3Add(camera->target, translation);
    }

    // Process mouse movement for camera look
    UpdateCameraPro(camera, 
        (Vector3){ 0.0f, 0.0f, 0.0f },
        (Vector3){ GetMouseDelta().x * 0.1f, GetMouseDelta().y * 0.1f, 0.0f }, 
        0.0f);
}

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

    // Initialize terrain chunks
    for (int i = 0; i < MAX_TERRAIN_CHUNKS; i++) {
        terrainChunks[i].active = false;
    }
    UpdateTerrainChunks(camera.position, terrainTexture);

    // Initialize the animals array
    for (int i = 0; i < MAX_ANIMALS; i++) {
        animals[i].active = false;
    }
    
    // Model skybox
    Model skybox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    Texture2D skyboxTexture = LoadTexture(TextFormat("%s/sky/BlueSkySkybox.png", GetResourceDir()));
    skybox.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyboxTexture;
    
    // Pre-spawn some animals
    Vector3 horsePosition = { 5.0f, 0.0f, 5.0f };
    SpawnAnimal(ANIMAL_HORSE, horsePosition, CHUNK_SIZE);
    
    Vector3 pigPosition = { -5.0f, 0.0f, 5.0f };
    SpawnAnimal(ANIMAL_PIG, pigPosition, CHUNK_SIZE);
    
    Vector3 catPosition = { 8.0f, 0.0f, -5.0f };
    SpawnAnimal(ANIMAL_CAT, catPosition, CHUNK_SIZE);
    
    Vector3 dogPosition = { -8.0f, 0.0f, -5.0f };
    SpawnAnimal(ANIMAL_DOG, dogPosition, CHUNK_SIZE);
    
    Vector3 cowPosition = { 0.0f, 0.0f, 8.0f };
    SpawnAnimal(ANIMAL_COW, cowPosition, CHUNK_SIZE);
    
    Vector3 chickenPosition = { 0.0f, 0.0f, -8.0f };
    SpawnAnimal(ANIMAL_CHICKEN, chickenPosition, CHUNK_SIZE);

    // Generate random columns for visualization
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
        // Update camera mode
        if (IsKeyPressed(KEY_ONE))
            cameraMode = CAMERA_FREE;
        if (IsKeyPressed(KEY_TWO))
            cameraMode = CAMERA_FIRST_PERSON;
        if (IsKeyPressed(KEY_THREE))
            cameraMode = CAMERA_THIRD_PERSON;
        if (IsKeyPressed(KEY_FOUR))
            cameraMode = CAMERA_ORBITAL;      

        UpdateCameraCustom(&camera, cameraMode);
        
        // Update terrain chunks around the player
        UpdateTerrainChunks(camera.position, terrainTexture);
        
        // Animal spawn controls with keyboard
        if (IsKeyPressed(KEY_H)) SpawnMultipleAnimals(ANIMAL_HORSE, 5, CHUNK_SIZE, camera);
        if (IsKeyPressed(KEY_C)) SpawnMultipleAnimals(ANIMAL_CAT, 5, CHUNK_SIZE, camera);
        if (IsKeyPressed(KEY_G)) SpawnMultipleAnimals(ANIMAL_DOG, 5, CHUNK_SIZE, camera); // Changed from KEY_D to KEY_G
        if (IsKeyPressed(KEY_O)) SpawnMultipleAnimals(ANIMAL_COW, 5, CHUNK_SIZE, camera);
        if (IsKeyPressed(KEY_K)) SpawnMultipleAnimals(ANIMAL_CHICKEN, 5, CHUNK_SIZE, camera);
        if (IsKeyPressed(KEY_P)) SpawnMultipleAnimals(ANIMAL_PIG, 5, CHUNK_SIZE, camera);
        
        // Clear all animals when DELETE is pressed
        if (IsKeyPressed(KEY_DELETE)) {
            animalCount = 0;
            for (int i = 0; i < ANIMAL_COUNT; i++) {
                animalCountByType[i] = 0;
            }
        }
        
        // Update all animals
        for (int i = 0; i < animalCount; i++) {
            if (animals[i].active) {
                UpdateAnimal(&animals[i], CHUNK_SIZE);
            }
        }

        // Draw
        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(camera);

        // Draw the skybox (scaled to be big)
        rlDisableBackfaceCulling();
        rlDisableDepthMask();
        DrawModel(skybox, camera.position, 50.0f, WHITE);
        rlEnableBackfaceCulling();
        rlEnableDepthMask();

        // Draw terrain chunks
        DrawTerrainChunks();

        // Draw all animals
        DrawAnimals();

        // Draw decorative columns
        for (int i = 0; i < MAX_COLUMNS; i++)
        {
            DrawCube(positions[i], 2.0f, heights[i], 2.0f, colors[i]);
            DrawCubeWires(positions[i], 2.0f, heights[i], 2.0f, MAROON);
        }

        // Draw player cube in third-person mode
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

        // Animal controls info
        DrawRectangle(5, 110, 330, 120, Fade(LIGHTGRAY, 0.5f));
        DrawRectangleLines(5, 110, 330, 120, GRAY);
        
        DrawText("Animal Controls:", 15, 120, 10, BLACK);
        DrawText("- Press H: Spawn horses", 15, 135, 10, BLACK);
        DrawText("- Press C: Spawn cats", 15, 150, 10, BLACK);
        DrawText("- Press G: Spawn dogs", 15, 165, 10, BLACK);
        DrawText("- Press O: Spawn cows", 15, 180, 10, BLACK);
        DrawText("- Press K: Spawn chickens", 15, 195, 10, BLACK);
        DrawText("- Press P: Spawn pigs", 15, 210, 10, BLACK);
        DrawText("- DELETE: Clear all animals", 15, 225, 10, BLACK);
        
        // Camera status info
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

        // Animal count display
        DrawRectangle(600, 110, 195, 100, Fade(LIGHTGREEN, 0.5f));
        DrawRectangleLines(600, 110, 195, 100, GREEN);
        
        DrawText("Animal Count:", 610, 120, 10, BLACK);
        DrawText(TextFormat("Total: %d/%d", animalCount, MAX_ANIMALS), 610, 135, 10, BLACK);
        DrawText(TextFormat("Horses: %d", animalCountByType[ANIMAL_HORSE]), 610, 150, 10, BLACK);
        DrawText(TextFormat("Cats: %d", animalCountByType[ANIMAL_CAT]), 610, 165, 10, BLACK);
        DrawText(TextFormat("Dogs: %d", animalCountByType[ANIMAL_DOG]), 610, 180, 10, BLACK);
        DrawText(TextFormat("Others: %d", animalCountByType[ANIMAL_COW] + animalCountByType[ANIMAL_CHICKEN] + animalCountByType[ANIMAL_PIG]), 610, 195, 10, BLACK);

        EndDrawing();
    }

    // De-Initialization
    UnloadTexture(terrainTexture);
    UnloadTexture(skyboxTexture);
    UnloadModel(skybox);
    
    // Unload all animals
    for (int i = 0; i < animalCount; i++) {
        if (!animals[i].active) continue;
        
        UnloadModel(animals[i].walkingModel);
        UnloadModel(animals[i].idleModel);
        
        if (animals[i].walkingAnim != NULL && animals[i].walkingAnimCount > 0) {
            UnloadModelAnimations(animals[i].walkingAnim, animals[i].walkingAnimCount);
        }
        
        if (animals[i].idleAnim != NULL && animals[i].idleAnimCount > 0) {
            UnloadModelAnimations(animals[i].idleAnim, animals[i].idleAnimCount);
        }
    }

    // Unload terrain chunks
    for (int i = 0; i < MAX_TERRAIN_CHUNKS; i++) {
        if (terrainChunks[i].active) {
            UnloadModel(terrainChunks[i].model);
        }
    }

    CloseWindow();
    return 0;
}