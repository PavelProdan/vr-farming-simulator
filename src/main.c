#include "raylib.h"
#include "rcamera.h"
#include "resource_dir.h"
#include "raymath.h"    // For Vector3Distance
#include "rlgl.h"       // For rl* functions
#include "math.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h> 
#include <string.h>  // For bool type
#define MAX_COLUMNS 20
#define MAX_ANIMALS 100
#define MAX_BUILDINGS 2  // For barn and horse barn
#define MAX_CLOUDS 5000   // Increased number of clouds
#define MAX_CLOUD_TYPES 1 // Number of different cloud types/textures
#define CLOUD_LAYER_HEIGHT 120.0f  // Height for all clouds
#define CLOUD_COVERAGE_RADIUS 1500.0f // Expanded cloud coverage for more distant clouds
#define CLOUD_MIN_SIZE 2.0f  // Smaller minimum cloud size
#define CLOUD_MAX_SIZE 8.0f // Smaller maximum cloud size
#define CLOUD_VIEW_DISTANCE 800.0f // Increased view distance for clouds
#define FIXED_TERRAIN_SIZE 512.0f      // Total terrain size
#define TERRAIN_CHUNKS_PER_SIDE 5      // 5x5 grid of terrain chunks
#define CHUNK_SIZE (FIXED_TERRAIN_SIZE / TERRAIN_CHUNKS_PER_SIDE)  // Size of each terrain chunk
#define MAX_TERRAIN_CHUNKS (TERRAIN_CHUNKS_PER_SIDE * TERRAIN_CHUNKS_PER_SIDE)  // Total number of chunks
#define CAMERA_MOVE_SPEED 0.08f // Reduced for walking simulator feel
#define HUMAN_HEIGHT 1.75f     // Average human height in meters
#define FOG_DENSITY 0.02f      // Fog density for distance effect
#define FOG_COLOR (Color){ 200, 225, 255, 255 }  // Light blue fog
#define SKY_COLOR (Color){ 135, 206, 235, 255 }  // Light blue sky color

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

Model roadModel;
Texture2D roadTexture;
Vector3 roadPosition;
float roadRotationAngle;
float roadLength;
float roadWidth = 4.0f; 

#define MAX_PATH_POINTS 200       // Maximum points in a recorded path
#define MAX_CUSTOM_ROADS 10

typedef struct {
    Vector3 points[MAX_PATH_POINTS];
    int numPoints;
    Model segments[MAX_PATH_POINTS - 1]; // MAX_PATH_POINTS-1 segments
    Vector3 segmentPositions[MAX_PATH_POINTS - 1];
    float segmentRotations[MAX_PATH_POINTS - 1];
    int segmentCount;
    bool isActive; // Is this road currently being drawn/used?
    char name[64]; // Optional name for the road
} CustomRoad;

CustomRoad allCustomRoads[MAX_CUSTOM_ROADS];
int totalCustomRoadsCount = 0; // How many roads are currently defined

// Buffer for the path currently being recorded
bool isRecordingPath = false;
Vector3 currentRecordingBuffer[MAX_PATH_POINTS];
int currentRecordingPointCount = 0;
float minRecordDistanceSq = 2.0f * 2.0f;

Model customRoadSegments[MAX_PATH_POINTS - 1];         // Models for each segment of the custom road
Vector3 customRoadSegmentPositions[MAX_PATH_POINTS - 1]; // Pre-calculated positions for each segment
float customRoadSegmentRotations[MAX_PATH_POINTS - 1];   // Pre-calculated rotations for each segment
int customRoadSegmentCount = 0;
bool useCustomRoad = false;

// Cloud structure for 3D sky
typedef struct {
    Vector3 position;
    float scale;
    float rotation;
    int type; // Cloud type for different textures
} Cloud;

// Global array of clouds
Cloud clouds[MAX_CLOUDS];
Texture2D cloudTextures[MAX_CLOUD_TYPES];  // Different cloud textures for variety

Vector3 Farm_Entrance_points[] = {
    { -14.73f, 0.15f, -5.82f },
    { -16.56f, 0.15f, -4.85f },
    { -18.33f, 0.15f, -3.91f },
    { -20.10f, 0.15f, -2.97f },
    { -21.87f, 0.15f, -2.04f },
    { -22.24f, 0.15f, -0.03f },
    { -21.08f, 0.15f, 1.69f },
    { -19.91f, 0.15f, 3.41f },
    { -18.75f, 0.15f, 5.14f },
    { -17.59f, 0.15f, 6.86f },
    { -16.42f, 0.15f, 8.58f },
    { -15.26f, 0.15f, 10.31f },
    { -14.09f, 0.15f, 12.03f },
    { -12.93f, 0.15f, 13.75f },
    { -11.76f, 0.15f, 15.48f },
    { -10.60f, 0.15f, 17.20f },
    { -9.44f, 0.15f, 18.92f },
    { -8.27f, 0.15f, 20.65f },
    { -7.15f, 0.15f, 22.31f },
    { -6.03f, 0.15f, 23.96f },
    { -4.42f, 0.15f, 22.67f },
    { -2.91f, 0.15f, 21.36f },
    { -1.39f, 0.15f, 20.05f },
    { 0.12f, 0.15f, 18.74f },
    { 1.63f, 0.15f, 17.42f },
    { 3.14f, 0.15f, 16.11f },
    { 4.65f, 0.15f, 14.80f },
    { 6.16f, 0.15f, 13.49f },
    { 7.67f, 0.15f, 12.18f },
};

int Farm_Entrance_numPoints = 29;
char Farm_Entrance_name[] = "Farm Entrance";

// Function to generate road segment models for a specific CustomRoad
void GenerateRoadSegments(CustomRoad* road, float rWidth, Texture2D rTexture) {
    if (!road || road->numPoints < 2) {
        if (road) {
            road->segmentCount = 0;
            road->isActive = false;
        }
        TraceLog(LOG_INFO, "Path has less than 2 points or road is null, cannot generate segments.");
        return;
    }

    // Unload previous segments if any for this specific road
    for (int i = 0; i < road->segmentCount; i++) {
        if (road->segments[i].meshCount > 0) UnloadModel(road->segments[i]);
    }

    road->segmentCount = road->numPoints - 1;
    if (road->segmentCount >= MAX_PATH_POINTS) { // Should be MAX_PATH_POINTS - 1
        road->segmentCount = MAX_PATH_POINTS - 2; // Ensure valid indexing for segments array
        TraceLog(LOG_WARNING, "Path too long for road '%s', truncated to %d segments.", road->name, road->segmentCount);
    }

    TraceLog(LOG_INFO, "Generating %d segments for road '%s'.", road->segmentCount, road->name);

    for (int i = 0; i < road->segmentCount; i++) {
        Vector3 p1 = road->points[i];
        Vector3 p2 = road->points[i+1];

        p1.y = 0.15f;
        p2.y = 0.15f;

        Vector3 segmentVector = Vector3Subtract(p2, p1);
        float segmentLength = Vector3Length(segmentVector);

        if (segmentLength < 0.01f) {
            TraceLog(LOG_WARNING, "Segment %d for road '%s' is too short.", i, road->name);
            Mesh emptyMesh = GenMeshPlane(0.01f, rWidth, 1, 1);
            road->segments[i] = LoadModelFromMesh(emptyMesh);
            road->segmentPositions[i] = p1;
            road->segmentRotations[i] = 0;
        } else {
            road->segmentPositions[i] = Vector3Scale(Vector3Add(p1, p2), 0.5f);
            road->segmentRotations[i] = atan2f(segmentVector.x, segmentVector.z) * RAD2DEG;

            Mesh segmentMesh = GenMeshPlane(segmentLength, rWidth, 1, 1);
            for (int v = 0; v < segmentMesh.vertexCount; v++) {
                segmentMesh.texcoords[v*2] *= segmentLength / rWidth;
            }
            road->segments[i] = LoadModelFromMesh(segmentMesh);
        }
        
        if (road->segments[i].meshCount > 0) {
             road->segments[i].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = rTexture;
        } else {
            TraceLog(LOG_ERROR, "Failed to load model for segment %d of road '%s'", i, road->name);
        }
    }
    road->isActive = true;
    TraceLog(LOG_INFO, "Segments generated successfully for road '%s'.", road->name);
}

// Function to draw all active custom roads
void DrawAllCustomRoads() {
    for (int i = 0; i < totalCustomRoadsCount; i++) {
        if (allCustomRoads[i].isActive && allCustomRoads[i].segmentCount > 0) {
            for (int j = 0; j < allCustomRoads[i].segmentCount; j++) {
                if (allCustomRoads[i].segments[j].meshCount > 0) {
                    DrawModelEx(allCustomRoads[i].segments[j], 
                                allCustomRoads[i].segmentPositions[j], 
                                (Vector3){0.0f, 1.0f, 0.0f}, 
                                allCustomRoads[i].segmentRotations[j], 
                                Vector3One(), WHITE);
                }
            }
        }
    }
}

// ... InitAnimal function ...

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
            animal->speed = 0.0075f; // Reduced speed
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
            animal->speed = 0.00825f; // Reduced speed
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

// ...existing code...
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
            // Check for collision with animals
            int collidedAnimalIndex = -1;
            bool animalCollision = IsCollisionWithAnimal(newPosition, playerCollisionRadius, &collidedAnimalIndex);
            
            if (!animalCollision) {
                // No collisions, apply the movement
                camera->position = newPosition;
                camera->target = Vector3Add(camera->target, translation);
            } else {
                // We collided with an animal, but check if we're trying to move away from it
                if (collidedAnimalIndex >= 0) {
                    // Calculate direction from animal to player
                    Vector3 animalToPlayer = Vector3Subtract(camera->position, animals[collidedAnimalIndex].position);
                    animalToPlayer.y = 0.0f; // Project onto horizontal plane
                    animalToPlayer = Vector3Normalize(animalToPlayer);
                    
                    // Calculate dot product between movement vector and direction away from animal
                    // Higher value means we're moving away from the animal
                    float dot = Vector3DotProduct(Vector3Normalize(translation), animalToPlayer);
                    
                    if (dot > 0.0f) {
                        // Moving away from animal, allow movement
                        camera->position = newPosition;
                        camera->target = Vector3Add(camera->target, translation);
                    }
                }
            }
        }
    }

    // Process mouse movement for camera look with deadzone
    Vector2 mouseDelta = GetMouseDelta();
    float mouseSensitivity = 0.1f;
    float deadzone = 0.5f; // Adjust this value if needed. Higher means less sensitive.

    float yawInput = 0.0f;
    float pitchInput = 0.0f;

    if (fabsf(mouseDelta.x) > deadzone) yawInput = mouseDelta.x * mouseSensitivity;
    if (fabsf(mouseDelta.y) > deadzone) pitchInput = mouseDelta.y * mouseSensitivity;
    
    UpdateCameraPro(camera, 
        (Vector3){ 0.0f, 0.0f, 0.0f },      // Movement (handled by our code above)
        (Vector3){ yawInput, pitchInput, 0.0f }, // Rotation (yaw, pitch, roll)
        0.0f);                               // Zoom
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
                animalRadius = 1.8f; // Reduced from 2.5f to 1.8f for easier navigation
                break;
            case ANIMAL_COW:
                animalRadius = 1.7f; // Larger collision for cow
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

// Draw a realistic sky: flat dome + hemisphere
void DrawRealisticSky(Vector3 center, Texture2D skyTexture) {
    // Parameters for the sky
    float flatRadius = 200.0f; // Radius of the flat part
    float flatHeight = 80.0f;  // Height above ground for the flat part
    float hemiRadius = 200.0f; // Radius of the hemisphere
    float hemiHeight = flatHeight; // Hemisphere starts at the edge of the flat part

    // Draw flat disc (sky dome)
    Mesh flatMesh = GenMeshCylinder(flatRadius, 0.1f, 64);
    Model flatModel = LoadModelFromMesh(flatMesh);
    flatModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTexture;
    SetMaterialTexture(&flatModel.materials[0], MATERIAL_MAP_DIFFUSE, skyTexture);
    DrawModelEx(flatModel, (Vector3){center.x, center.y + flatHeight, center.z}, (Vector3){1,0,0}, 0, (Vector3){1,1,1}, WHITE);
    UnloadModel(flatModel);

    // Draw hemisphere (connects flat sky to ground)
    Mesh hemiMesh = GenMeshHemiSphere(hemiRadius, 64, 32);
    Model hemiModel = LoadModelFromMesh(hemiMesh);
    hemiModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTexture;
    SetMaterialTexture(&hemiModel.materials[0], MATERIAL_MAP_DIFFUSE, skyTexture);
    // Position hemisphere so its base aligns with the flat disc
    DrawModelEx(hemiModel, (Vector3){center.x, center.y + hemiHeight, center.z}, (Vector3){1,0,0}, 0, (Vector3){1,1,1}, WHITE);
    UnloadModel(hemiModel);
}

// Initialize clouds for a Minecraft-style sky
void InitClouds(float terrainSize) {
    // Create a single cloud texture - just a white rectangle
    Image cloudImage = GenImageColor(32, 32, WHITE);
    Texture2D cloudTexture = LoadTextureFromImage(cloudImage);
    
    // Use the same texture for all clouds
    for (int i = 0; i < MAX_CLOUD_TYPES; i++) {
        cloudTextures[i] = cloudTexture;
    }
    UnloadImage(cloudImage);
    
    // Improved cloud distribution to eliminate empty zones
    // Use a grid-based approach with some randomization for more uniform coverage
    
    // Calculate how many clouds to place in each part of the sky
    int cloudGridSize = 16;  // 16x16 grid for cloud placement
    int cloudsPerCell = MAX_CLOUDS / (cloudGridSize * cloudGridSize);
    
    int cloudIndex = 0;
    
    // Fill the sky with clouds in a grid pattern (more uniform distribution)
    for (int gridX = 0; gridX < cloudGridSize; gridX++) {
        for (int gridZ = 0; gridZ < cloudGridSize; gridZ++) {
            // Calculate the base position for this grid cell
            float baseX = ((float)gridX / cloudGridSize) * 2.0f - 1.0f; // -1.0 to 1.0
            float baseZ = ((float)gridZ / cloudGridSize) * 2.0f - 1.0f; // -1.0 to 1.0
            
            // Place multiple clouds in each grid cell with some randomness
            for (int i = 0; i < cloudsPerCell; i++) {
                if (cloudIndex >= MAX_CLOUDS) break;
                
                // Add randomness within each grid cell
                float offsetX = GetRandomValue(-100, 100) / 200.0f; // -0.5 to 0.5
                float offsetZ = GetRandomValue(-100, 100) / 200.0f; // -0.5 to 0.5
                
                // Calculate final normalized position (-1 to 1)
                float normalizedX = baseX + offsetX / cloudGridSize;
                float normalizedZ = baseZ + offsetZ / cloudGridSize;
                
                // Convert to world coordinates with distance from center
                float distance = CLOUD_COVERAGE_RADIUS * (0.2f + (sqrtf(normalizedX*normalizedX + normalizedZ*normalizedZ) * 0.8f));
                float angle = atan2f(normalizedZ, normalizedX);
                
                // Position the cloud
                clouds[cloudIndex].position.x = cosf(angle) * distance;
                clouds[cloudIndex].position.z = sinf(angle) * distance;
                
                // Vary cloud height
                clouds[cloudIndex].position.y = CLOUD_LAYER_HEIGHT + GetRandomValue(-25, 35);
                
                // Adjust cloud size - slightly larger for more distant clouds
                float distanceRatio = distance / CLOUD_COVERAGE_RADIUS;
                clouds[cloudIndex].scale = CLOUD_MIN_SIZE + GetRandomValue(0, (int)(CLOUD_MAX_SIZE - CLOUD_MIN_SIZE));
                
                // Ensure slight size variation
                if (i % 3 == 0) clouds[cloudIndex].scale *= 1.2f;
                if (i % 7 == 0) clouds[cloudIndex].scale *= 0.8f;
                
                // No rotation 
                clouds[cloudIndex].rotation = 0;
                clouds[cloudIndex].type = 0;
                
                cloudIndex++;
            }
        }
    }
    
    // Add some extra clouds to fill any remaining gaps
    int extraCloudCount = MAX_CLOUDS - cloudIndex;
    if (extraCloudCount > 0) {
        // Add remaining clouds with a focus on filling gaps
        for (int i = 0; i < extraCloudCount; i++) {
            // Choose a random angle but with bias toward diagonal areas (where gaps are more likely)
            float angle = GetRandomValue(0, 7) * PI/4 + GetRandomValue(-15, 15) * DEG2RAD;
            float distance = GetRandomValue(20, (int)CLOUD_COVERAGE_RADIUS);
            
            clouds[cloudIndex].position.x = cosf(angle) * distance;
            clouds[cloudIndex].position.z = sinf(angle) * distance;
            clouds[cloudIndex].position.y = CLOUD_LAYER_HEIGHT + GetRandomValue(-20, 35);
            
            clouds[cloudIndex].scale = CLOUD_MIN_SIZE + GetRandomValue(0, (int)(CLOUD_MAX_SIZE - CLOUD_MIN_SIZE));
            clouds[cloudIndex].rotation = 0;
            clouds[cloudIndex].type = 0;
            
            cloudIndex++;
        }
    }
}

// Draw all clouds in the sky - Static Minecraft style
void DrawClouds(Camera camera) {
    for (int i = 0; i < MAX_CLOUDS; i++) {
        // Calculate distance from camera to cloud
        float distance = Vector3Distance(camera.position, clouds[i].position);
        
        // Skip clouds that are too far away
        if (distance > CLOUD_VIEW_DISTANCE) continue;
        
        // Draw flat rectangular clouds (Minecraft style)
        DrawCube(clouds[i].position, clouds[i].scale, clouds[i].scale * 0.2f, clouds[i].scale, WHITE);
        
        // Draw additional blocks for larger cloud shapes (2x2 or 3x3 patterns)
        if (i % 3 == 0) { // Every third cloud gets a more complex shape
            // Add 2-4 adjacent blocks to create a larger cloud formation
            for (int bx = -1; bx <= 1; bx++) {
                for (int bz = -1; bz <= 1; bz++) {
                    // Skip center block (already drawn above)
                    if (bx == 0 && bz == 0) continue;
                    
                    // Skip some blocks randomly for more varied shapes
                    if (abs(bx) + abs(bz) > 1 && (i % 5 > 2)) continue;
                    
                    // Draw additional cloud blocks at fixed positions
                    Vector3 blockPos = clouds[i].position;
                    blockPos.x += bx * clouds[i].scale * 0.9f;
                    blockPos.z += bz * clouds[i].scale * 0.9f;
                    blockPos.y += (i % 3 - 1) * 0.1f * clouds[i].scale; // Small fixed height variation
                    
                    DrawCube(blockPos, 
                            clouds[i].scale * 0.95f, 
                            clouds[i].scale * 0.18f, 
                            clouds[i].scale * 0.95f, 
                            WHITE);
                }
            }
        }
    }
}

int main(void)
{
    // const int screenWidth = 1512;
    // const int screenHeight = 1080;
    int currentMonitor = GetCurrentMonitor();
    const int screenWidth = GetMonitorWidth(currentMonitor);
    const int screenHeight = GetMonitorHeight(currentMonitor);

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
    
    // Set up basic fog effect for distance
    float fogDensity = FOG_DENSITY;
    Color fogColor = FOG_COLOR;
    
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


    // --- Road Initialization ---
    // Load road texture
    roadTexture = LoadTexture("textures/rocky_trail_diff_8k.jpg"); // Make sure you have this texture
    GenTextureMipmaps(&roadTexture); // Ensure mipmaps are generated
    SetTextureFilter(roadTexture, TEXTURE_FILTER_ANISOTROPIC_16X);
    SetTextureWrap(roadTexture, TEXTURE_WRAP_REPEAT);

    // Calculate road parameters
    Vector3 building1Pos = buildings[0].position;
    Vector3 building2Pos = buildings[1].position;

    // Midpoint between buildings
    roadPosition = Vector3Scale(Vector3Add(building1Pos, building2Pos), 0.5f);
    roadPosition.y = 0.1f; // Slightly above ground to prevent z-fighting

    // Vector from building 1 to building 2
    Vector3 roadVector = Vector3Subtract(building2Pos, building1Pos);
    roadLength = Vector3Length(roadVector);

    // Angle of the road (rotation around Y axis)
    roadRotationAngle = atan2f(roadVector.x, roadVector.z) * RAD2DEG;

    // Generate road mesh (Plane width is road length, Plane height is road width)
    Mesh roadMesh = GenMeshPlane(roadLength, roadWidth, 1, 1);

    // Adjust UVs for texture tiling along the length
    for (int i = 0; i < roadMesh.vertexCount; i++) {
        roadMesh.texcoords[i*2] *= roadLength / roadWidth; // Tile texture based on length/width ratio
    }

    roadModel = LoadModelFromMesh(roadMesh);
    roadModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = roadTexture;
    // --- End Road Initialization ---
    if (totalCustomRoadsCount < MAX_CUSTOM_ROADS && Farm_Entrance_numPoints > 1) {
        CustomRoad* newRoad = &allCustomRoads[totalCustomRoadsCount];
        snprintf(newRoad->name, sizeof(newRoad->name), "%s", Farm_Entrance_name);
        newRoad->numPoints = Farm_Entrance_numPoints;
        // Use memcpy to copy the points array
        memcpy(newRoad->points, Farm_Entrance_points, sizeof(Vector3) * newRoad->numPoints);
        GenerateRoadSegments(newRoad, roadWidth, roadTexture); // Generate the visuals
        totalCustomRoadsCount++; // Increment the count of active roads
    }
    

    // Initialize the cloud system
    InitClouds(FIXED_TERRAIN_SIZE);

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

        
        // --- Path Recording Logic ---
        if (IsKeyPressed(KEY_R)) {
            isRecordingPath = !isRecordingPath;
            if (isRecordingPath) {
                currentRecordingPointCount = 0;
                TraceLog(LOG_INFO, "Path recording STARTED. Move around.");
            } else {
                TraceLog(LOG_INFO, "Path recording STOPPED. %d points recorded.", currentRecordingPointCount);
                if (currentRecordingPointCount >= 2) {
                    if (totalCustomRoadsCount < MAX_CUSTOM_ROADS) {
                        CustomRoad* newRoad = &allCustomRoads[totalCustomRoadsCount];
                        snprintf(newRoad->name, sizeof(newRoad->name), "Recorded Road %d", totalCustomRoadsCount + 1);
                        newRoad->numPoints = currentRecordingPointCount;
                        memcpy(newRoad->points, currentRecordingBuffer, sizeof(Vector3) * newRoad->numPoints);
                        
                        GenerateRoadSegments(newRoad, roadWidth, roadTexture); // Generate segments for the new road
                        
                        totalCustomRoadsCount++;
                        TraceLog(LOG_INFO, "New custom road '%s' created with %d points.", newRoad->name, newRoad->numPoints);
                    } else {
                        TraceLog(LOG_WARNING, "Max custom roads limit reached (%d). Cannot add new recorded path.", MAX_CUSTOM_ROADS);
                    }
                } else {
                     TraceLog(LOG_WARNING, "Not enough points (%d) to form a path.", currentRecordingPointCount);
                }
            }
        }

        if (isRecordingPath && currentRecordingPointCount < MAX_PATH_POINTS) {
            Vector3 currentGroundPos = { camera.position.x, 0.15f, camera.position.z };
            if (currentRecordingPointCount == 0 || Vector3DistanceSqr(currentGroundPos, currentRecordingBuffer[currentRecordingPointCount - 1]) > minRecordDistanceSq) {
                currentRecordingBuffer[currentRecordingPointCount] = currentGroundPos;
                currentRecordingPointCount++;
                // TraceLog(LOG_INFO, "Path point %d recorded at (%.2f, %.2f, %.2f)", currentRecordingPointCount, currentGroundPos.x, currentGroundPos.y, currentGroundPos.z);
            }
        }
        
        // Export path and generate road for preview
        if (IsKeyPressed(KEY_E)) {
            if (totalCustomRoadsCount > 0 && !isRecordingPath) { // Export the last defined road
                CustomRoad* roadToExp = &allCustomRoads[totalCustomRoadsCount - 1];
                if (roadToExp->numPoints > 1) {
                    printf("// --- Recorded Path Data for: %s ---\n", roadToExp->name);
                    printf("Vector3 %s_points[] = {\n", roadToExp->name); // Use road name for array
                    for (int i = 0; i < roadToExp->numPoints; i++) {
                        printf("    { %.2ff, %.2ff, %.2ff },\n", roadToExp->points[i].x, roadToExp->points[i].y, roadToExp->points[i].z);
                    }
                    printf("};\n");
                    printf("int %s_numPoints = %d;\n", roadToExp->name, roadToExp->numPoints);
                    printf("char %s_name[] = \"%s\";\n", roadToExp->name, roadToExp->name);
                    printf("// --- End Recorded Path Data ---\n");
                    TraceLog(LOG_INFO, "Path data for '%s' printed to console.", roadToExp->name);
                } else {
                    TraceLog(LOG_WARNING, "Last road '%s' has insufficient points to export.", roadToExp->name);
                }
            } else if (isRecordingPath) {
                TraceLog(LOG_WARNING, "Stop recording (press R) before exporting (E).");
            } else {
                TraceLog(LOG_WARNING, "No custom roads recorded or defined to export.");
            }
        }
        // --- End Path Recording Logic ---
        
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

        // Clear background with light blue sky color
        ClearBackground((Color){ 135, 206, 255, 255 });  // Light blue for sunny sky

        BeginMode3D(camera);

        // Draw terrain chunks
        DrawTerrainChunks();
        if (totalCustomRoadsCount > 0) {
            DrawAllCustomRoads();
        } else {
            // Fallback to drawing the original straight road if no custom roads are active
            if (roadModel.meshCount > 0) {
                 DrawModelEx(roadModel, roadPosition, (Vector3){0.0f, 1.0f, 0.0f}, roadRotationAngle, Vector3One(), WHITE);
            }
        }

        // Draw clouds
        DrawClouds(camera);

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
        int centerX = GetScreenWidth() / 2;
        int centerY = GetScreenHeight() / 2;
        int crosshairSize = 8; // Size of the crosshair arms
        DrawLine(centerX - crosshairSize, centerY, centerX + crosshairSize, centerY, WHITE); // Horizontal line
        DrawLine(centerX, centerY - crosshairSize, centerX, centerY + crosshairSize, WHITE); // Vertical line


        // Draw info boxes
        DrawRectangle(5, 5, 330, 100, Fade(SKYBLUE, 0.5f));
        DrawRectangleLines(5, 5, 330, 100, BLUE);

        DrawText("Camera controls:", 15, 15, 10, BLACK);
        DrawText("- Move keys: W, A, S, D, Space, Left-Ctrl", 15, 30, 10, BLACK);
        DrawText("- Look around: arrow keys or mouse", 15, 45, 10, BLACK);
        DrawText("- Camera mode keys: 1, 2, 3, 4", 15, 60, 10, BLACK);
        DrawText("- Zoom keys: num-plus, num-minus or mouse scroll", 15, 75, 10, BLACK);
        DrawText("- Camera projection key: P", 15, 90, 10, BLACK);
        // ... existing DrawText for animal controls ...
DrawText("- R: Start/Stop Path Recording", 15, 240, 10, BLACK);
DrawText("- E: Export Path to Console (after stopping recording)", 15, 255, 10, BLACK);

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

    // Unload custom road segments
for (int i = 0; i < customRoadSegmentCount; i++) {
    if (customRoadSegments[i].meshCount > 0) UnloadModel(customRoadSegments[i]);
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

    // Unload cloud textures
    for (int i = 0; i < MAX_CLOUD_TYPES; i++) {
        UnloadTexture(cloudTextures[i]);
    }

    CloseWindow();
    return 0;
}