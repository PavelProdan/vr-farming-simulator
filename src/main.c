#include "raylib.h"
#include "rcamera.h"
#include "resource_dir.h"
#include "raymath.h"    // For Vector3Distance
#include "rlgl.h"       // For rl* functions
#include "math.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>    // For bool type
#define MAX_COLUMNS 20
#define MAX_ANIMALS 100
#define MAX_BUILDINGS 2  // For barn and horse barn
#define FIXED_TERRAIN_SIZE 512.0f      // Total terrain size
#define TERRAIN_CHUNKS_PER_SIDE 5      // 5x5 grid of terrain chunks
#define CHUNK_SIZE (FIXED_TERRAIN_SIZE / TERRAIN_CHUNKS_PER_SIDE)  // Size of each terrain chunk
#define MAX_TERRAIN_CHUNKS (TERRAIN_CHUNKS_PER_SIDE * TERRAIN_CHUNKS_PER_SIDE)  // Total number of chunks
#define CAMERA_MOVE_SPEED 0.08f // Reduced for walking simulator feel
#define HUMAN_HEIGHT 1.75f     // Average human height in meters
#define FOG_DENSITY 0.02f      // Fog density for distance effect
#define FOG_COLOR (Color){ 200, 225, 255, 255 }  // Light blue fog

// Define LIGHTGREEN color if it's not defined in raylib
#ifndef LIGHTGREEN
    #define LIGHTGREEN (Color){ 200, 255, 200, 255 }
#endif

// Function prototypes
bool IsCollisionWithBuilding(Vector3 position, float radius, int* buildingIndex);
bool IsCollisionWithAnimal(Vector3 position, float radius, int* animalIndex);

// Building structure
typedef struct {
    Model model;
    Vector3 position;
    float scale;
    float rotationAngle;
} Building;

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
    Vector3 spawnPosition;   // Original spawn position to return to
    Vector3 direction;
    float scale;
    float speed;
    float rotationAngle;
    float moveTimer;
    float moveInterval;
    float maxWanderDistance; // Maximum distance from spawn point
    bool isMoving;
    bool active;
} Animal;

// Global array of animals
Animal animals[MAX_ANIMALS];
int animalCount = 0;
int animalCountByType[ANIMAL_COUNT] = {0};

// Global array of buildings
Building buildings[MAX_BUILDINGS];

// Global array of terrain chunks
TerrainChunk terrainChunks[MAX_TERRAIN_CHUNKS];

// Function to initialize a new animal
void InitAnimal(Animal* animal, AnimalType type, Vector3 position) {
    animal->type = type;
    animal->position = position;
    animal->spawnPosition = position;  // Store original spawn position
    animal->direction = (Vector3){0.0f, 0.0f, 1.0f};  // Default direction
    animal->animFrameCounter = 0;
    animal->moveTimer = 0.0f;
    animal->moveInterval = 1.5f + GetRandomValue(0, 20) / 10.0f; // 1.5-3.5 seconds between decisions
    animal->isMoving = false;
    animal->rotationAngle = 0.0f;
    animal->active = true;
    animal->maxWanderDistance = 15.0f + GetRandomValue(0, 50) / 10.0f; // Each animal has a territory of 15-20 units
    
    // Set type-specific properties
    switch(type) {
        case ANIMAL_HORSE:
            animal->walkingModel = LoadModel("animals/walking_horse.glb");
            animal->idleModel = LoadModel("animals/idle_horse.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_horse.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_horse.glb", &animal->idleAnimCount);
            animal->scale = 1.0f;
            animal->speed = 0.015f; // Reduced speed
            break;
        case ANIMAL_CAT:
            animal->walkingModel = LoadModel("animals/walking_cat.glb");
            animal->idleModel = LoadModel("animals/idle_cat.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_cat.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_cat.glb", &animal->idleAnimCount);
            animal->scale = 0.9f;
            animal->speed = 0.02f;  // Reduced speed
            break;
        case ANIMAL_DOG:
            animal->walkingModel = LoadModel("animals/walking_dog.glb");
            animal->idleModel = LoadModel("animals/idle_dog.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_dog.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_dog.glb", &animal->idleAnimCount);
            animal->scale = 0.8f;
            animal->speed = 0.018f; // Reduced speed
            break;
        case ANIMAL_COW:
            animal->walkingModel = LoadModel("animals/walking_cow.glb");
            animal->idleModel = LoadModel("animals/idle_cow.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_cow.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_cow.glb", &animal->idleAnimCount);
            animal->scale = 0.27f;  // Reduced from 1.2f by 10x
            animal->speed = 0.01f;  // Reduced speed
            break;
        case ANIMAL_CHICKEN:
            animal->walkingModel = LoadModel("animals/walking_chicken.glb");
            animal->idleModel = LoadModel("animals/idle_chicken.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_chicken.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_chicken.glb", &animal->idleAnimCount);
            animal->scale = 1.0f;
            animal->speed = 0.006f; // Reduced speed
            break;
        case ANIMAL_PIG:
            animal->walkingModel = LoadModel("animals/walking_pig.glb");
            animal->idleModel = LoadModel("animals/idle_pig.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_pig.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_pig.glb", &animal->idleAnimCount);
            animal->scale = 0.16f;  // Reduced from 0.8f by 5x
            animal->speed = 0.011f; // Reduced speed
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
            
            // Calculate distance from spawn point
            float distanceFromSpawn = Vector3Distance(animal->position, animal->spawnPosition);
            
            // 80% chance to move forward in current direction or toward spawn if too far away
            if (GetRandomValue(0, 100) < 80 || distanceFromSpawn > animal->maxWanderDistance) {
                if (distanceFromSpawn > animal->maxWanderDistance * 0.7f) {
                    // If animal is getting far from spawn, steer back toward spawn point
                    Vector3 toSpawn = Vector3Subtract(animal->spawnPosition, animal->position);
                    float len = sqrtf(toSpawn.x * toSpawn.x + toSpawn.z * toSpawn.z);
                    if (len > 0) {
                        animal->direction.x = toSpawn.x / len;
                        animal->direction.z = toSpawn.z / len;
                    }
                } else {
                    // Small random adjustment to current direction (continue forward with slight variation)
                    float turnAmount = GetRandomValue(-10, 10) / 100.0f; // Small direction change (-0.1 to 0.1)
                    float currentAngle = atan2f(animal->direction.x, animal->direction.z);
                    float newAngle = currentAngle + turnAmount;
                    animal->direction.x = sinf(newAngle);
                    animal->direction.z = cosf(newAngle);
                }
            } else {
                // 20% chance to make a significant direction change
                float turnAngle = GetRandomValue(-90, 90) * DEG2RAD;
                float currentAngle = atan2f(animal->direction.x, animal->direction.z);
                float newAngle = currentAngle + turnAngle;
                animal->direction.x = sinf(newAngle);
                animal->direction.z = cosf(newAngle);
            }
            
            // Calculate rotation angle based on direction
            animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
        } else {
            animal->isMoving = false;
        }
        
        // Randomize next interval (slightly longer when idle to make movement more natural)
        animal->moveInterval = (animal->isMoving ? 1.0f : 2.0f) + GetRandomValue(0, 20) / 10.0f;
    }
    
    // Update position if animal is moving
    if (animal->isMoving) {
        // Store original position for collision handling
        Vector3 oldPosition = animal->position;
        
        // Try to move in the current direction
        animal->position.x += animal->direction.x * animal->speed;
        animal->position.z += animal->direction.z * animal->speed;
        
        // Keep within terrain boundaries
        float boundary = terrainSize/2.0f - 2.0f;
        if (animal->position.x < -boundary) animal->position.x = -boundary;
        if (animal->position.x > boundary) animal->position.x = boundary;
        if (animal->position.z < -boundary) animal->position.z = -boundary;
        if (animal->position.z > boundary) animal->position.z = boundary;
        
        // Check for collisions with buildings
        int collidedBuildingIndex = -1;
        if (IsCollisionWithBuilding(animal->position, animal->scale * 1.5f, &collidedBuildingIndex)) {
            // Collision occurred, revert position
            animal->position = oldPosition;
            
            // Calculate new direction away from building
            Vector3 awayFromBuilding = Vector3Subtract(animal->position, buildings[collidedBuildingIndex].position);
            float len = sqrtf(awayFromBuilding.x * awayFromBuilding.x + awayFromBuilding.z * awayFromBuilding.z);
            
            if (len > 0) {
                // Normalize the direction vector
                awayFromBuilding.x /= len;
                awayFromBuilding.z /= len;
                
                // Add some randomness to prevent animals getting stuck
                float randomAngle = GetRandomValue(-30, 30) * DEG2RAD;
                float currentAngle = atan2f(awayFromBuilding.x, awayFromBuilding.z);
                float newAngle = currentAngle + randomAngle;
                
                // Set new direction
                animal->direction.x = sinf(newAngle);
                animal->direction.z = cosf(newAngle);
                
                // Update rotation angle to match new direction
                animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
                
                // Reset timer to make a new decision sooner
                animal->moveTimer = animal->moveInterval - 0.5f;
                
                // Try to move in the new direction
                animal->position.x += animal->direction.x * animal->speed;
                animal->position.z += animal->direction.z * animal->speed;
            }
        }
        
        // Check for collisions with other animals
        for (int i = 0; i < animalCount; i++) {
            if (!animals[i].active || &animals[i] == animal) continue;
            
            // Simple collision detection
            float distance = Vector3Distance(animal->position, animals[i].position);
            float minDistance = (animal->scale + animals[i].scale) * 1.2f;
            
            if (distance < minDistance) {
                // Calculate direction away from the other animal
                Vector3 awayDir = {
                    animal->position.x - animals[i].position.x,
                    0,
                    animal->position.z - animals[i].position.z
                };
                
                float len = sqrtf(awayDir.x * awayDir.x + awayDir.z * awayDir.z);
                if (len > 0) {
                    // Normalize the direction
                    awayDir.x /= len;
                    awayDir.z /= len;
                    
                    // Move away from the collision
                    animal->position = oldPosition; // Revert to old position first
                    animal->position.x += awayDir.x * animal->speed * 1.5f;
                    animal->position.z += awayDir.z * animal->speed * 1.5f;
                    
                    // Update direction and angle for natural movement
                    animal->direction = awayDir;
                    animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
                    
                    // Reset timer to make a new decision sooner
                    animal->moveTimer = animal->moveInterval - 0.5f;
                }
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

// Function to initialize all terrain chunks in a fixed grid
void InitAllTerrainChunks(Texture2D terrainTexture) {
    int index = 0;
    for (int z = 0; z < TERRAIN_CHUNKS_PER_SIDE; z++) {
        for (int x = 0; x < TERRAIN_CHUNKS_PER_SIDE; x++) {
            Vector2 chunkPos = { x - TERRAIN_CHUNKS_PER_SIDE/2.0f, z - TERRAIN_CHUNKS_PER_SIDE/2.0f };
            InitTerrainChunk(&terrainChunks[index], chunkPos, terrainTexture);
            index++;
        }
    }
}

// Function to update terrain chunks - simplified to just check boundaries
void UpdateTerrainChunks(Vector3 playerPosition, Texture2D terrainTexture) {
    // Restrict player movement within terrain bounds
    float boundary = FIXED_TERRAIN_SIZE/2.0f - 5.0f;
    if (playerPosition.x < -boundary) playerPosition.x = -boundary;
    if (playerPosition.x > boundary) playerPosition.x = boundary;
    if (playerPosition.z < -boundary) playerPosition.z = -boundary;
    if (playerPosition.z > boundary) playerPosition.z = boundary;
}

// Function to draw terrain chunks with fog effect
void DrawTerrainChunks(void) {
    // Draw terrain chunks without using BeginShaderMode
    for (int i = 0; i < MAX_TERRAIN_CHUNKS; i++) {
        if (terrainChunks[i].active) {
            DrawModel(terrainChunks[i].model, terrainChunks[i].worldPos, 1.0f, WHITE);
        }
    }
}

// Custom camera update function with collision detection
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
        // Store original position for collision detection
        Vector3 oldPosition = camera->position;
        
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

        // Calculate new position
        Vector3 newPosition = Vector3Add(camera->position, translation);
        
        // Check for collisions with buildings
        float playerCollisionRadius = 0.5f;
        int collidedWithIndex = -1;
        
        // Only update position if there's no collision with buildings
        if (!IsCollisionWithBuilding(newPosition, playerCollisionRadius, &collidedWithIndex)) {
            // No building collision, now check for animal collisions
            if (!IsCollisionWithAnimal(newPosition, playerCollisionRadius, NULL)) {
                // No collisions, apply the movement
                camera->position = newPosition;
                camera->target = Vector3Add(camera->target, translation);
            }
        }
    }

    // Process mouse movement for camera look
    UpdateCameraPro(camera, 
        (Vector3){ 0.0f, 0.0f, 0.0f },
        (Vector3){ GetMouseDelta().x * 0.1f, GetMouseDelta().y * 0.1f, 0.0f }, 
        0.0f);
}

// Function to check if an animal is colliding with a building
bool IsCollisionWithBuilding(Vector3 animalPosition, float animalRadius, int* buildingIndex) {
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        // Calculate distance between animal and building center
        float distance = Vector3Distance(animalPosition, buildings[i].position);
        
        // Use a collision radius based on the building type
        float buildingRadius;
        if (i == 0) { // barn.glb - needs a larger radius relative to its small scale
            buildingRadius = 6.0f; // Fixed size regardless of scale since the scale is very small (0.05)
        } else { // horse_barn.glb
            buildingRadius = 6.0f * buildings[i].scale;
        }
        
        // Check for collision
        if (distance < (animalRadius + buildingRadius)) {
            if (buildingIndex != NULL) *buildingIndex = i;
            return true;
        }
    }
    
    return false;
}

// Function to check if the player is colliding with an animal
bool IsCollisionWithAnimal(Vector3 playerPosition, float playerRadius, int* animalIndex) {
    for (int i = 0; i < animalCount; i++) {
        if (!animals[i].active) continue;
        
        // Calculate distance between player and animal
        float distance = Vector3Distance(playerPosition, animals[i].position);
        
        // Use a collision radius based on the animal type for more accurate collision
        float animalRadius;
        switch(animals[i].type) {
            case ANIMAL_HORSE:
                animalRadius = 2.5f; // Larger collision for horse
                break;
            case ANIMAL_COW:
                animalRadius = 2.0f; // Larger collision for cow
                break;
            case ANIMAL_PIG:
                animalRadius = 1.5f; // Medium collision for pig
                break;
            case ANIMAL_DOG:
                animalRadius = 1.2f; // Medium collision for dog
                break;
            case ANIMAL_CAT:
                animalRadius = 1.0f; // Small collision for cat
                break;
            case ANIMAL_CHICKEN:
                animalRadius = 0.8f; // Small collision for chicken
                break;
            default:
                animalRadius = animals[i].scale * 1.5f; // Fallback
        }
        
        // Check for collision
        if (distance < (playerRadius + animalRadius)) {
            if (animalIndex != NULL) *animalIndex = i;
            return true;
        }
    }
    
    return false;
}

int main(void)
{
    const int screenWidth = 1920;
    const int screenHeight = 1080;

    InitWindow(screenWidth, screenHeight, "VR Farming Simulator");

    // Define the camera
    Camera camera = {0};
    camera.position = (Vector3){0.0f, HUMAN_HEIGHT, 0.0f}; // Set camera at human height
    camera.target = (Vector3){0.0f, HUMAN_HEIGHT, 1.0f};   // Look forward
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int cameraMode = CAMERA_FIRST_PERSON; // Default to first person

    // Update the texture loading part of your code
    SearchAndSetResourceDir("resources");
    // Change the filename to match your downloaded texture
    Texture2D terrainTexture = LoadTexture("textures/rocky_terrain_02_diff_8k.jpg");

    // Optimize texture quality
    SetTextureFilter(terrainTexture, TEXTURE_FILTER_ANISOTROPIC_16X);
    SetTextureWrap(terrainTexture, TEXTURE_WRAP_REPEAT);

    // Initialize all terrain chunks at once with fixed layout
    InitAllTerrainChunks(terrainTexture);
    
    // Initialize the animals array
    for (int i = 0; i < MAX_ANIMALS; i++) {
        animals[i].active = false;
    }
    
    // Model skybox
    Model skybox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    Texture2D skyboxTexture = LoadTexture(TextFormat("%s/sky/BlueSkySkybox.png", GetResourceDir()));
    skybox.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyboxTexture;
    
    // Set up basic fog effect for distance
    float fogDensity = FOG_DENSITY;
    Color fogColor = FOG_COLOR;
    
    // Simple fog setup that's compatible with most raylib versions
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "fogDensity"), 
                  &fogDensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(skybox.materials[0].shader, GetShaderLocation(skybox.materials[0].shader, "fogColor"), 
                  &fogColor, SHADER_UNIFORM_VEC4);

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

    // Load building models
    buildings[0].model = LoadModel("buildings/barn.glb");
    buildings[0].position = (Vector3){ -10.0f, 0.0f, -10.0f };
    buildings[0].scale = 0.05f;
    buildings[0].rotationAngle = 45.0f;

    buildings[1].model = LoadModel("buildings/horse_barn.glb");
    buildings[1].position = (Vector3){ 10.0f, 0.0f, 10.0f };
    buildings[1].scale = 0.5f;
    buildings[1].rotationAngle = 135.0f;

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
    SetTargetFPS(60);

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

        // Draw buildings
        for (int i = 0; i < MAX_BUILDINGS; i++)
        {
            DrawModelEx(
                buildings[i].model, 
                buildings[i].position, 
                (Vector3){0.0f, 1.0f, 0.0f},  // Rotation axis (Y-axis)
                buildings[i].rotationAngle,   // Rotation angle
                (Vector3){buildings[i].scale, buildings[i].scale, buildings[i].scale}, 
                WHITE
            );
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
    
    // Unload building models
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        UnloadModel(buildings[i].model);
    }

    CloseWindow();
    return 0;
}