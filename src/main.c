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
#define MAX_BUILDINGS 120  // Increased for more fence segments and future expansion
#define MAX_CLOUDS 5000   // Increased number of clouds
#define MAX_PLANTS 3000 // Maximum number of plants (increased from 1000 to 2000)
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
#define DIALOG_DISPLAY_TIME 8.0f  // Time to display dialog message in seconds

// Chicken enclosure constants
#define ENCLOSURE_CENTER_2 (Vector3){ -45.0f, 0.0f, -3.0f }
#define ENCLOSURE_WIDTH_2 15.0f
#define ENCLOSURE_LENGTH_2 12.0f

// Pig enclosure constants (first enclosure)
#define ENCLOSURE_CENTER_1 (Vector3){ 20.0f, 0.0f, 15.0f }
#define ENCLOSURE_WIDTH_1 10.0f
#define ENCLOSURE_LENGTH_1 12.0f

// Define LIGHTGREEN color if it's not defined in raylib
#ifndef LIGHTGREEN
    #define LIGHTGREEN (Color){ 200, 255, 200, 255 }
#endif

//#define DEVMODE

#define NUMBER_OF_TREES 160
#define NUMBER_OF_GRASS 700
#define NUMBER_OF_FLOWERS 300
#define NUMBER_OF_FLOWER_TYPE2 900
#define NUMBER_OF_BUSH_WITH_FLOWERS 400
#define MAX_PATH_POINTS 200       // Maximum points in a recorded path

// Plant types enum
typedef enum {
    PLANT_TREE,
    PLANT_GRASS,
    PLANT_FLOWER,
    PLANT_FLOWER_TYPE2,
    PLANT_BUSH_WITH_FLOWERS, // Added new bush with flowers type
    PLANT_TYPE_COUNT
} PlantType;

// Plant structure
typedef struct {
    PlantType type;
    Model model; // This will now refer to one of the globally loaded models
    Vector3 position;
    float scale;
    float rotationAngle;
    bool active;
} Plant;

// Function prototypes
bool IsCollisionWithBuilding(Vector3 position, float radius, int* buildingIndex);
bool IsCollisionWithAnimal(Vector3 position, float radius, int* animalIndex);
bool IsPositionOnRoad(Vector3 position, float roadWidth); // New function prototype
void InitPlant(Plant* plant, PlantType type, Vector3 position, float scale, float rotation);
void SpawnPlant(PlantType type, Vector3 position, float scale, float rotation);
Vector3 GetRandomPlantPosition(float terrainSize);
void DrawPlants(Camera camera); // Modified to accept Camera
void UnloadPlantResources(void);
void ClearPlantsNearRoads(float clearExtraRadius); // Function to clear plants blocking roads
bool IsNearBankOrOnRoadToBank(Vector3 position); // Check if player is near bank or on road to bank

// Human character states
typedef enum {
    HUMAN_STATE_WALKING,
    HUMAN_STATE_IDLE_AT_INTERSECTION,
    HUMAN_STATE_TALKING,
    HUMAN_STATE_DISAPPEARING,
    HUMAN_STATE_INACTIVE
} HumanState;

// Human character structure
typedef struct {
    Model walkingModel;
    Model idleModel;
    Model lookingModel;
    ModelAnimation* walkingAnim;
    ModelAnimation* idleAnim;
    ModelAnimation* lookingAnim;
    int walkingAnimCount;
    int idleAnimCount;
    int lookingAnimCount;
    int animFrameCounter;
    Vector3 position;
    Vector3 targetPosition;
    Vector3 direction;
    float speed;
    float scale;
    float rotationAngle;
    HumanState state;
    float stateTimer;
    float disappearAlpha;
    char dialogMessage[128];
    bool showDialog;
    float dialogTimer;
    int currentPathIndex;
    int pathPointCount;
    Vector3 pathPoints[MAX_PATH_POINTS];
    bool active;
    bool waitForKeyPress;
} Human;

// --- Global Models for Plants ---
Model globalTreeModel;
Model globalGrassModel;
Model globalFlowerModel;
Model globalFlowerModel_type2;
Model globalBushWithFlowersModel; // Added for the new bush with flowers type
// --- End Global Models for Plants ---

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
    void* soundData; // Added sound data pointer
} Animal;

// Global array of animals
Animal animals[MAX_ANIMALS];
int animalCount = 0;
int animalCountByType[ANIMAL_COUNT] = {0};

// Global human character
Human human;

// Global array of buildings
Building buildings[MAX_BUILDINGS];

// Global array of plants
Plant plants[MAX_PLANTS];
int plantCount = 0;

// Moved road-related global variables and definitions
Model roadModel;
Texture2D roadTexture;
Vector3 roadPosition;
float roadRotationAngle;
float roadLength;
float roadWidth = 4.0f; 

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

// Function to initialize a new plant
void InitPlant(Plant* plant, PlantType type, Vector3 position, float scale, float rotation) {
    plant->type = type;
    plant->position = position;
    plant->scale = scale;
    plant->rotationAngle = rotation;
    plant->active = true;

    // Assign pre-loaded global model based on type
    switch(type) {
        case PLANT_TREE:
            plant->model = globalTreeModel;
            break;
        case PLANT_GRASS:
            plant->model = globalGrassModel;
            break;
        case PLANT_FLOWER:
            plant->model = globalFlowerModel;
            break;
        case PLANT_FLOWER_TYPE2:
            plant->model = globalFlowerModel_type2;
            break;
        case PLANT_BUSH_WITH_FLOWERS: // Added case for new bush with flowers type
            plant->model = globalBushWithFlowersModel;
            break;
        default:
            plant->active = false; // Invalid type
            TraceLog(LOG_WARNING, "Attempted to initialize invalid plant type.");
            // Assign a default or empty model to avoid issues if model is not loaded
            // For safety, ensure global models are loaded before this is called.
            // If a type is invalid, plant->model might remain uninitialized if not handled.
            // Consider assigning a known invalid/empty model or checking plant->active before drawing.
            break;
    }
    // No need to check plant->model.meshCount here if we ensure global models are loaded correctly.
    // The active flag will prevent drawing if the type was invalid.
}

// Function to spawn a plant of specified type
void SpawnPlant(PlantType type, Vector3 position, float scale, float rotation) {
    if (plantCount >= MAX_PLANTS) {
        TraceLog(LOG_WARNING, "Cannot spawn more plants - maximum limit reached.");
        return;
    }
    InitPlant(&plants[plantCount], type, position, scale, rotation);
    if (plants[plantCount].active) {
        plantCount++;
    }
}

// Helper to generate a random position for spawning plants
Vector3 GetRandomPlantPosition(float terrainSize) { // terrainSize argument is kept for signature consistency
    Vector3 position;
    float minX = -100.0f; 
    float maxX = 100.0f;
    float minZ = -100.0f;
    float maxZ = 100.0f;
    int maxAttempts = 100; // Increased attempts from 50 to 100

    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        position.x = GetRandomValue(minX * 100, maxX * 100) / 100.0f;
        position.z = GetRandomValue(minZ * 100, maxZ * 100) / 100.0f;
        position.y = 0.0f; // On the ground

        // First check if position is on any road
        if (IsPositionOnRoad(position, roadWidth)) {
            continue; // Skip this position if it's on a road
        }

        bool collisionWithBuilding = false;
        for (int i = 0; i < MAX_BUILDINGS; i++) {
            if (buildings[i].model.meshCount == 0) continue; // Skip uninitialized buildings

            float exclusionRadiusForPlant;
            const float FENCE_MODEL_SCALE_CONST = 0.2f; // From main()

            if (i == 0) { // barn.glb (index 0, scale 0.05f)
                exclusionRadiusForPlant = 8.0f; // Increased from 7.0f
            } else if (i == 1) { // horse_barn.glb (index 1, scale 0.5f)
                exclusionRadiusForPlant = 7.0f; // Increased from 4.5f
            } else if (i == 2) { // Bank.glb (index 2, scale 0.0002f)
                exclusionRadiusForPlant = 12.0f; // Restored to larger value to prevent road blockage
            } else if (i == 3) { // constructionHouse.glb (index 3, scale 0.1f)
                exclusionRadiusForPlant = 9.0f;  // Exclusion radius for Construction House
            } else if (i == 4) { // FarmHouse.glb (index 4)
                exclusionRadiusForPlant = 10.0f;  // Reduced exclusion radius for FarmHouse
            } else if (i >= 4 && buildings[i].scale == FENCE_MODEL_SCALE_CONST) { // Likely a fence segment
                exclusionRadiusForPlant = 1.5f; // Half-length (1.0f) + 0.5f buffer
            } else { 
                // Generic fallback for other unknown buildings
                exclusionRadiusForPlant = buildings[i].scale * 3.0f + 1.5f; // Base scaled size + buffer
                if (exclusionRadiusForPlant < 3.0f) exclusionRadiusForPlant = 3.0f; // Increased minimum from 2.0f
            }

            if (Vector3Distance(position, buildings[i].position) < exclusionRadiusForPlant) {
                collisionWithBuilding = true;
                break;
            }
        }

        if (!collisionWithBuilding) {
            return position; // Found a good spot
        }
    }

    TraceLog(LOG_WARNING, "GetRandomPlantPosition: Could not find a non-colliding position after %d attempts. Placing at last attempted spot.", maxAttempts);
    return position;
}

// Draw all active plants
void DrawPlants(Camera camera) { // Modified to accept Camera
    float maxDrawDistanceFlowerType2 = 40.0f; // Max distance to draw flower type 2
    float maxDrawDistanceBushWithFlowers = 50.0f; // Max distance to draw bush with flowers

    for (int i = 0; i < plantCount; i++) {
        if (plants[i].active) {
            if (plants[i].type == PLANT_FLOWER_TYPE2) {
                if (Vector3DistanceSqr(camera.position, plants[i].position) < maxDrawDistanceFlowerType2 * maxDrawDistanceFlowerType2) {
                    DrawModelEx(plants[i].model, plants[i].position, (Vector3){0.0f, 1.0f, 0.0f}, plants[i].rotationAngle, (Vector3){plants[i].scale, plants[i].scale, plants[i].scale}, WHITE);
                }
            } else if (plants[i].type == PLANT_BUSH_WITH_FLOWERS) {
                 if (Vector3DistanceSqr(camera.position, plants[i].position) < maxDrawDistanceBushWithFlowers * maxDrawDistanceBushWithFlowers) {
                    DrawModelEx(plants[i].model, plants[i].position, (Vector3){0.0f, 1.0f, 0.0f}, plants[i].rotationAngle, (Vector3){plants[i].scale, plants[i].scale, plants[i].scale}, WHITE);
                }
            }else {
                DrawModelEx(plants[i].model, plants[i].position, (Vector3){0.0f, 1.0f, 0.0f}, plants[i].rotationAngle, (Vector3){plants[i].scale, plants[i].scale, plants[i].scale}, WHITE);
            }
        }
    }
}

// Unload all plant resources
void UnloadPlantResources(void) {
    // Unload the globally loaded plant models
    if (globalTreeModel.meshCount > 0) UnloadModel(globalTreeModel);
    if (globalGrassModel.meshCount > 0) UnloadModel(globalGrassModel);
    if (globalFlowerModel.meshCount > 0) UnloadModel(globalFlowerModel);
    if (globalFlowerModel_type2.meshCount > 0) UnloadModel(globalFlowerModel_type2);
    if (globalBushWithFlowersModel.meshCount > 0) UnloadModel(globalBushWithFlowersModel); // Added unloading for new bush model

    // Clear the plant array itself if needed, but models are now global
    plantCount = 0;
    // Note: The individual plant instances in the `plants` array no longer "own" the model data
    // in terms of loading/unloading. They just use a reference to the global ones.
}

// Function to clear plants that might be blocking roads
void ClearPlantsNearRoads(float clearExtraRadius) {
    TraceLog(LOG_INFO, "Checking for plants blocking roads...");
    int removedCount = 0;
    
    // Check every plant
    for (int i = 0; i < plantCount; i++) {
        if (!plants[i].active) continue;
        
        // For each road
        for (int roadIndex = 0; roadIndex < totalCustomRoadsCount; roadIndex++) {
            if (!allCustomRoads[roadIndex].isActive || allCustomRoads[roadIndex].numPoints < 2) continue;
            
            // Check each segment of this road
            for (int segmentIndex = 0; segmentIndex < allCustomRoads[roadIndex].numPoints - 1; segmentIndex++) {
                Vector3 p1 = allCustomRoads[roadIndex].points[segmentIndex];
                Vector3 p2 = allCustomRoads[roadIndex].points[segmentIndex + 1];
                
                // Project plant position onto the road segment
                Vector3 segmentVec = Vector3Subtract(p2, p1);
                Vector3 plantVec = Vector3Subtract(plants[i].position, p1);
                
                float segmentLengthSq = Vector3LengthSqr(segmentVec);
                if (segmentLengthSq == 0.0f) continue; // Avoid division by zero
                
                float t = Vector3DotProduct(plantVec, segmentVec) / segmentLengthSq;
                t = Clamp(t, 0.0f, 1.0f); // Ensure projection is on the segment
                
                Vector3 closestPoint = Vector3Add(p1, Vector3Scale(segmentVec, t));
                
                // Check if plant is too close to the road
                // Use a larger radius for taller/larger plants like trees
                float plantSize = (plants[i].type == PLANT_TREE) ? plants[i].scale * 1.7f : plants[i].scale;
                float clearRadius = (roadWidth / 2.0f) + clearExtraRadius + plantSize;
                
                if (Vector3Distance(plants[i].position, closestPoint) < clearRadius) {
                    // Plant is too close to road - deactivate it
                    plants[i].active = false;
                    removedCount++;
                    break; // No need to check other segments for this plant
                }
            }
            
            if (!plants[i].active) break; // Skip to next plant if already marked for removal
        }
    }
    
    // Check specifically along the path to the bank (second road)
    if (totalCustomRoadsCount >= 2) {
        // Get bank position
        Vector3 bankPosition = buildings[2].position;
        float bankClearRadius = 15.0f; // Extra clearance around the bank
        
        // Remove plants too close to the bank
        for (int i = 0; i < plantCount; i++) {
            if (!plants[i].active) continue;
            
            if (Vector3Distance(plants[i].position, bankPosition) < bankClearRadius) {
                plants[i].active = false;
                removedCount++;
            }
        }
    }
    
    TraceLog(LOG_INFO, "Removed %d plants that were blocking roads.", removedCount);
}

// Global array of terrain chunks
TerrainChunk terrainChunks[MAX_TERRAIN_CHUNKS];

// New function to check if a position is on any road
bool IsPositionOnRoad(Vector3 position, float rWidth) {
    for (int i = 0; i < totalCustomRoadsCount; i++) {
        if (allCustomRoads[i].isActive && allCustomRoads[i].numPoints >= 2) {
            // For ribbon-style roads (single mesh), we check against the original points
            // This is a simplification; true collision with a mesh is more complex.
            // We'll check distance to line segments formed by the road points.
            for (int j = 0; j < allCustomRoads[i].numPoints - 1; j++) {
                Vector3 p1 = allCustomRoads[i].points[j];
                Vector3 p2 = allCustomRoads[i].points[j+1];

                // Project position onto the line segment
                Vector3 segmentVec = Vector3Subtract(p2, p1);
                Vector3 pointVec = Vector3Subtract(position, p1);

                float segmentLengthSq = Vector3LengthSqr(segmentVec);
                if (segmentLengthSq == 0.0f) continue; // Segment has zero length

                float t = Vector3DotProduct(pointVec, segmentVec) / segmentLengthSq;
                t = Clamp(t, 0.0f, 1.0f); // Clamp t to be on the segment

                Vector3 closestPointOnSegment = Vector3Add(p1, Vector3Scale(segmentVec, t));
                
                // Check distance from the plant's position to the closest point on the segment's centerline
                if (Vector3Distance(position, closestPointOnSegment) < (rWidth / 2.0f) + 2.5f) { // Increased buffer to prevent road blockage
                    return true; // Position is on this road segment
                }
            }
        }
    }
    return false; // Position is not on any road
}

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
    { -10.97f, 0.15f, -7.52f },
    { -12.55f, 0.15f, -6.29f },
    { -14.13f, 0.15f, -5.07f },
    { -15.71f, 0.15f, -3.84f },
    { -17.35f, 0.15f, -2.56f },
    { -18.99f, 0.15f, -1.29f },
    { -20.38f, 0.15f, 0.25f },
    { -21.14f, 0.15f, 2.18f },
    { -20.92f, 0.15f, 4.23f },
    { -19.95f, 0.15f, 6.07f },
    { -18.76f, 0.15f, 7.77f },
    { -17.57f, 0.15f, 9.48f },
    { -16.37f, 0.15f, 11.18f },
    { -15.15f, 0.15f, 12.86f },
    { -13.92f, 0.15f, 14.54f },
    { -12.65f, 0.15f, 16.19f },
    { -11.34f, 0.15f, 17.80f },
    { -10.07f, 0.15f, 19.35f },
    { -8.69f, 0.15f, 20.90f },
    { -7.07f, 0.15f, 22.20f },
    { -5.11f, 0.15f, 22.84f },
    { -3.07f, 0.15f, 22.82f },
    { -1.20f, 0.15f, 21.91f },
    { 0.45f, 0.15f, 20.65f },
    { 1.89f, 0.15f, 19.15f },
    { 3.28f, 0.15f, 17.61f },
    { 4.65f, 0.15f, 16.05f },
    { 5.97f, 0.15f, 14.54f },
    { 7.29f, 0.15f, 13.03f },
    { 8.65f, 0.15f, 11.47f },
};

int Farm_Entrance_numPoints = 30;
char Farm_Entrance_name[] = "Farm Entrance";

// Define points for the new road
Vector3 secondRoadPoints[] = {
    { -14.96f, 1.75f, 13.54f },
    { -13.42f, 1.75f, 12.26f },
    { -11.88f, 1.75f, 10.99f },
    { -10.34f, 1.75f, 9.71f },
    { -8.80f, 1.75f, 8.43f },
    { -7.26f, 1.75f, 7.16f },
    { -5.66f, 1.75f, 5.83f },
    { -3.72f, 1.75f, 5.12f },
    { -2.11f, 1.75f, 3.79f },
    { -0.51f, 1.75f, 2.46f },
    { 1.09f, 1.75f, 1.14f },
    { 2.68f, 1.75f, -0.20f },
    { 4.21f, 1.75f, -1.60f },
    { 5.62f, 1.75f, -3.14f },
    { 6.86f, 1.75f, -4.81f },
    { 7.90f, 1.75f, -6.60f },
    { 8.84f, 1.75f, -8.46f },
    { 9.73f, 1.75f, -10.34f },
    { 10.58f, 1.75f, -12.24f },
    { 11.42f, 1.75f, -14.14f },
    { 12.24f, 1.75f, -16.06f },
    { 13.05f, 1.75f, -17.97f },
    { 13.87f, 1.75f, -19.88f },
    { 14.69f, 1.75f, -21.79f },
    { 15.51f, 1.75f, -23.71f },
    { 16.32f, 1.75f, -25.62f },
    { 17.14f, 1.75f, -27.53f },
    { 17.96f, 1.75f, -29.44f },
    { 18.78f, 1.75f, -31.36f },
    { 19.56f, 1.75f, -33.20f },
    { 20.35f, 1.75f, -35.04f },
    { 21.14f, 1.75f, -36.87f },
    { 21.92f, 1.75f, -38.71f },
    { 22.71f, 1.75f, -40.55f },
    { 23.49f, 1.75f, -42.39f },
    { 24.28f, 1.75f, -44.23f },
    { 25.07f, 1.75f, -46.07f },
    { 25.85f, 1.75f, -47.91f },
    { 26.64f, 1.75f, -49.75f },
    { 27.42f, 1.75f, -51.59f },
    { 28.21f, 1.75f, -53.43f }
};
int secondRoadNumPoints = 41;
char secondRoadName[] = "Second Road";

Vector3 thirdRoadPoints[] = {
    { -13.91f, 1.75f, 13.08f },
    { -15.59f, 1.75f, 14.18f },
    { -17.33f, 1.75f, 15.32f },
    { -19.07f, 1.75f, 16.46f },
    { -20.81f, 1.75f, 17.60f },
    { -22.57f, 1.75f, 18.70f },
    { -24.40f, 1.75f, 19.68f },
    { -26.35f, 1.75f, 20.41f },
    { -28.39f, 1.75f, 20.82f },
    { -30.46f, 1.75f, 20.97f },
    { -32.46f, 1.75f, 21.09f },
    { -34.45f, 1.75f, 21.20f },
    { -36.51f, 1.75f, 21.51f },
    { -38.51f, 1.75f, 22.08f }
};
int thirdRoadNumPoints = 14;
char thirdRoadName[] = "Third Road";

// Define points for the fourth road
Vector3 fourthRoadPoints[] = {
    { 17.01f, 1.75f, 15.76f },
    { 14.94f, 1.75f, 15.81f },
    { 12.90f, 1.75f, 16.19f },
    { 11.05f, 1.75f, 17.12f },
    { 9.71f, 1.75f, 18.70f },
    { 8.69f, 1.75f, 20.51f },
    { 7.76f, 1.75f, 22.37f },
    { 6.79f, 1.75f, 24.21f },
    { 5.08f, 1.75f, 25.38f },
    { 3.05f, 1.75f, 25.04f },
    { 1.37f, 1.75f, 23.82f },
    { -0.15f, 1.75f, 22.41f }
};
int fourthRoadNumPoints = 12;
char fourthRoadName[] = "Fourth Road";

// Define points for the fifth road
Vector3 fifthRoadPoints[] = {
    { -21.79f, 1.75f, 1.48f },
    { -23.77f, 1.75f, 1.22f },
    { -25.84f, 1.75f, 0.98f },
    { -27.91f, 1.75f, 0.82f },
    { -29.99f, 1.75f, 0.77f },
    { -32.07f, 1.75f, 0.82f },
    { -34.07f, 1.75f, 0.91f }
};
int fifthRoadNumPoints = 7;
char fifthRoadName[] = "Fifth Road";

// Function to generate road segment models for a specific CustomRoad
void GenerateRoadSegments(CustomRoad* road, float rWidth, Texture2D rTexture) {
    if (!road || road->numPoints < 2) {
        if (road) {
            road->segmentCount = 0;
            road->isActive = false;
        }
        TraceLog(LOG_INFO, "Path has less than 2 points or road is null, cannot generate road.");
        return;
    }

    // Unload previous segments if any
    for (int i = 0; i < road->segmentCount; i++) {
        if (road->segments[i].meshCount > 0) UnloadModel(road->segments[i]);
    }

    // We'll create a single mesh for the entire road
    road->segmentCount = 1;

    // Create a mesh structure
    Mesh roadMesh = { 0 };
    
    // Two vertices per point (left and right edges of the road)
    int vertexCount = road->numPoints * 2;
    // Two triangles between each pair of points
    int triangleCount = (road->numPoints - 1) * 2;
    
    roadMesh.vertexCount = vertexCount;
    roadMesh.triangleCount = triangleCount;
    
    // Allocate memory for mesh data
    roadMesh.vertices = (float*)MemAlloc(vertexCount * 3 * sizeof(float)); // x,y,z for each vertex
    roadMesh.texcoords = (float*)MemAlloc(vertexCount * 2 * sizeof(float)); // u,v for each vertex
    roadMesh.indices = (unsigned short*)MemAlloc(triangleCount * 3 * sizeof(unsigned short)); // 3 indices per triangle
    roadMesh.normals = (float*)MemAlloc(vertexCount * 3 * sizeof(float)); // x,y,z normal per vertex
    roadMesh.colors = NULL;
    
    TraceLog(LOG_INFO, "Creating ribbon-like road with %d points, %d vertices, %d triangles", 
             road->numPoints, vertexCount, triangleCount);
    
    // Calculate total path length for texture mapping
    float totalPathLength = 0;
    float* segmentLengths = (float*)MemAlloc((road->numPoints - 1) * sizeof(float));
    
    for (int i = 0; i < road->numPoints - 1; i++) {
        segmentLengths[i] = Vector3Distance(road->points[i], road->points[i+1]);
        totalPathLength += segmentLengths[i];
    }
    
    // Generate vertices along the path with their left and right offsets
    float currentDistance = 0;
    
    for (int i = 0; i < road->numPoints; i++) {
        Vector3 currentPoint = road->points[i];
        Vector3 direction;
        
        // Calculate direction at this point
        if (i < road->numPoints - 1) {
            // Forward direction for all except the last point
            direction = Vector3Normalize(Vector3Subtract(road->points[i+1], currentPoint));
        } else if (i > 0) {
            // For last point, use direction from previous point
            direction = Vector3Normalize(Vector3Subtract(currentPoint, road->points[i-1]));
        } else {
            // Fallback for a single point (should not happen)
            direction = (Vector3){ 0.0f, 0.0f, 1.0f };
        }
        
        // Calculate perpendicular vector for road width
        Vector3 perpendicular = { -direction.z, 0.0f, direction.x }; // Right perpendicular
        
        // Calculate left and right edge positions
        Vector3 leftEdge = Vector3Subtract(currentPoint, Vector3Scale(perpendicular, roadWidth / 2.0f));
        Vector3 rightEdge = Vector3Add(currentPoint, Vector3Scale(perpendicular, roadWidth / 2.0f));
        
        // Set consistent height at terrain level (just slightly above to prevent z-fighting)
        leftEdge.y = 0.01f;
        rightEdge.y = 0.01f;
        
        // Store left edge vertex
        int leftIndex = i * 6;  // 2 triangles per segment * 3 indices per triangle
        roadMesh.vertices[leftIndex] = leftEdge.x;
        roadMesh.vertices[leftIndex + 1] = leftEdge.y;
        roadMesh.vertices[leftIndex + 2] = leftEdge.z;
        
        // Store right edge vertex
        int rightIndex = leftIndex + 3;
        roadMesh.vertices[rightIndex] = rightEdge.x;
        roadMesh.vertices[rightIndex + 1] = rightEdge.y;
        roadMesh.vertices[rightIndex + 2] = rightEdge.z;
        
        // Create UV mapping based on distance along path
        // This ensures texture flows continuously along the path
        float v = currentDistance / totalPathLength * 10.0f; // Scale for repetition
        
        // U coordinate goes from 0 (left) to 1 (right) across the road width
        roadMesh.texcoords[i * 4] = 0.0f;     // Left edge
        roadMesh.texcoords[i * 4 + 1] = v;    // V coordinate along path
        roadMesh.texcoords[i * 4 + 2] = 1.0f; // Right edge
        roadMesh.texcoords[i * 4 + 3] = v;    // V coordinate along path
        
        // Set normals (pointing up)
        int normalIndex = i * 6;  // 2 vertices per point, 3 components per normal
        for (int j = 0; j < 2; j++) {
            roadMesh.normals[normalIndex + j*3] = 0.0f;     // X component
            roadMesh.normals[normalIndex + j*3 + 1] = 1.0f; // Y component (up)
            roadMesh.normals[normalIndex + j*3 + 2] = 0.0f; // Z component
        }
        
        // Update distance along path (for texture mapping)
        if (i < road->numPoints - 1) {
            currentDistance += segmentLengths[i];
        }
    }
    
    // Create triangles connecting vertices
    for (int i = 0; i < road->numPoints - 1; i++) {
        // Each segment needs two triangles to connect adjacent points
        int indexBase = i * 6;  // 2 triangles per segment * 3 indices per triangle
        
        // Vertex indices for this segment
        unsigned short i0 = i * 2;         // Current point, left edge
        unsigned short i1 = i * 2 + 1;     // Current point, right edge 
        unsigned short i2 = (i + 1) * 2;   // Next point, left edge
        unsigned short i3 = (i + 1) * 2 + 1; // Next point, right edge
        
        // First triangle: (current left, current right, next left)
        roadMesh.indices[indexBase] = i0;
        roadMesh.indices[indexBase + 1] = i1;
        roadMesh.indices[indexBase + 2] = i2;
        
        // Second triangle: (current right, next right, next left)
        roadMesh.indices[indexBase + 3] = i1;
        roadMesh.indices[indexBase + 4] = i3;
        roadMesh.indices[indexBase + 5] = i2;
    }
    
    // Upload mesh to GPU
    UploadMesh(&roadMesh, false);
    
    // Create model from mesh
    road->segments[0] = LoadModelFromMesh(roadMesh);
    
    // Apply texture to the model
    road->segments[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = roadTexture;
    
    // Set road properties for drawing
    road->segmentPositions[0] = (Vector3){0, 0, 0}; // We'll draw at origin since vertices are in world space
    road->segmentRotations[0] = 0.0f; 
    road->isActive = true;
    
    // Free temporary resources
    MemFree(segmentLengths);
    
    TraceLog(LOG_INFO, "Road ribbon created successfully for '%s' with %d points", 
             road->name, road->numPoints);
}

// Function to generate a single smooth road mesh for a specific CustomRoad
void GenerateSmoothRoad(CustomRoad* road, float rWidth, Texture2D rTexture) {
    if (!road || road->numPoints < 2) {
        if (road) {
            road->segmentCount = 0;
            road->isActive = false;
        }
        TraceLog(LOG_INFO, "Path has less than 2 points or road is null, cannot generate smooth road.");
        return;
    }

    // Unload previous segments if any for this specific road
    for (int i = 0; i < road->segmentCount; i++) {
        if (road->segments[i].meshCount > 0) UnloadModel(road->segments[i]);
    }

    // We will create a SINGLE mesh for the entire road
    Mesh roadMesh = { 0 };
    
    // Two vertices per point (left and right edges of the road)
    int vertexCount = road->numPoints * 2;
    // Two triangles between each pair of points
    int triangleCount = (road->numPoints - 1) * 2;
    
    roadMesh.vertexCount = vertexCount;
    roadMesh.triangleCount = triangleCount;
    
    // Allocate memory for all the mesh data
    roadMesh.vertices = (float*)MemAlloc(vertexCount * 3 * sizeof(float)); // x,y,z for each vertex
    roadMesh.texcoords = (float*)MemAlloc(vertexCount * 2 * sizeof(float)); // u,v for each vertex
    roadMesh.colors = NULL; // No vertex colors
    roadMesh.indices = (unsigned short*)MemAlloc(triangleCount * 3 * sizeof(unsigned short)); // 3 indices per triangle
    roadMesh.normals = (float*)MemAlloc(vertexCount * 3 * sizeof(float)); // x,y,z normals
    
    // Generate the road geometry
    for (int i = 0; i < road->numPoints; i++) {
        Vector3 currentPoint = road->points[i];
        Vector3 direction;
        
        // Calculate segment direction
        if (i < road->numPoints - 1) {
            // Looking forward for all points except the last
            direction = Vector3Normalize(Vector3Subtract(road->points[i + 1], currentPoint));
        } else {
            // For the last point, use the direction from the previous point
            direction = Vector3Normalize(Vector3Subtract(currentPoint, road->points[i - 1]));
        }
        
        // Calculate road edges (perpendicular to direction)
        Vector3 right = { direction.z, 0, -direction.x }; // Right-hand perpendicular vector
        Vector3 leftEdge = Vector3Subtract(currentPoint, Vector3Scale(right, rWidth / 2));
        Vector3 rightEdge = Vector3Add(currentPoint, Vector3Scale(right, rWidth / 2));
        
        // Ensure consistent height
        leftEdge.y = 0.15f;
        rightEdge.y = 0.15f;
        
        // Add vertices for left and right edges of the road
        int vBase = i * 6; // 3 floats per vertex, 2 vertices per point
        
        // Left vertex
        roadMesh.vertices[vBase] = leftEdge.x;
        roadMesh.vertices[vBase + 1] = leftEdge.y;
        roadMesh.vertices[vBase + 2] = leftEdge.z;
        
        // Right vertex
        roadMesh.vertices[vBase + 3] = rightEdge.x;
        roadMesh.vertices[vBase + 4] = rightEdge.y;
        roadMesh.vertices[vBase + 5] = rightEdge.z;
        
        // Texture coordinates - map the texture along the path
        float t = (float)i / (road->numPoints - 1); // Normalized distance along the path
        int tcBase = i * 4; // 2 floats per vertex, 2 vertices per point
        
        roadMesh.texcoords[tcBase] = 0.0f; // Left edge U (0 to 1 across the road width)
        roadMesh.texcoords[tcBase + 1] = t * 10.0f; // V coordinate (tiled along the road length)
        roadMesh.texcoords[tcBase + 2] = 1.0f; // Right edge U
        roadMesh.texcoords[tcBase + 3] = t * 10.0f; // V coordinate
        
        // Set normals pointing up
        int nBase = i * 6; // 3 floats per normal, 2 normals per point
        for (int j = 0; j < 2; j++) {
            roadMesh.normals[nBase + (j*3)] = 0.0f;
            roadMesh.normals[nBase + (j*3) + 1] = 1.0f;
            roadMesh.normals[nBase + (j*3) + 2] = 0.0f;
        }
    }
    
    // Create triangles between points
    for (int i = 0; i < road->numPoints - 1; i++) {
        int iBase = i * 6; // 3 indices per triangle, 2 triangles per segment
        
        // Triangle 1: (i,left) -> (i,right) -> (i+1,left)
        roadMesh.indices[iBase] = i * 2; // Current point, left edge
        roadMesh.indices[iBase + 1] = i * 2 + 1; // Current point, right edge
        roadMesh.indices[iBase + 2] = (i + 1) * 2; // Next point, left edge
        
        // Triangle 2: (i,right) -> (i+1,right) -> (i+1,left)
        roadMesh.indices[iBase + 3] = i * 2 + 1; // Current point, right edge
        roadMesh.indices[iBase + 4] = (i + 1) * 2 + 1; // Next point, right edge
        roadMesh.indices[iBase + 5] = (i + 1) * 2; // Next point, left edge
    }
    
    // Upload mesh to GPU
    UploadMesh(&roadMesh, false);
    
    // Create model from mesh
    road->segments[0] = LoadModelFromMesh(roadMesh);
    road->segments[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = roadTexture;
    
    // Set as single-segment road
    road->segmentCount = 1;
    road->isActive = true;
    
    TraceLog(LOG_INFO, "Single-mesh road generated successfully for road '%s' with %d points.", 
             road->name, road->numPoints);
}

// Function to draw all active custom roads
void DrawAllCustomRoads() {
    for (int i = 0; i < totalCustomRoadsCount; i++) {
        if (allCustomRoads[i].isActive && allCustomRoads[i].segmentCount > 0) {
            // Draw each segment individually
            for (int j = 0; j < allCustomRoads[i].segmentCount; j++) {
                if (allCustomRoads[i].segments[j].meshCount > 0) {
                    Vector3 drawPos = allCustomRoads[i].segmentPositions[j];
                    // Small Y offset to prevent Z-fighting with ground
                    drawPos.y += 0.01f;
                    
                    // Draw the segment at its position with its rotation
                    DrawModelEx(allCustomRoads[i].segments[j], 
                                drawPos, 
                                (Vector3){0.0f, 1.0f, 0.0f}, // Rotation around Y-axis
                                allCustomRoads[i].segmentRotations[j], 
                                Vector3One(), // Scale of 1
                                WHITE);
                }
            }
        }
    }
}

// --- InitAnimal function ...

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
            animal->speed = 0.022f; // Increased speed for better exploration
            animal->maxWanderDistance = 40.0f + GetRandomValue(0, 100) / 10.0f; // Increased wander distance for horses (40-50 units)
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
            animal->speed = 0.018f;  // Increased speed for better exploration
            animal->maxWanderDistance = 35.0f + GetRandomValue(0, 100) / 10.0f; // Increased wander distance for cows (35-45 units)
            break;
        case ANIMAL_CHICKEN:
            animal->walkingModel = LoadModel("animals/walking_chicken.glb");
            animal->idleModel = LoadModel("animals/idle_chicken.glb");
            animal->walkingAnim = LoadModelAnimations("animals/walking_chicken.glb", &animal->walkingAnimCount);
            animal->idleAnim = LoadModelAnimations("animals/idle_chicken.glb", &animal->idleAnimCount);
            animal->scale = 1.8f;  // Increased from 1.0f to make chickens bigger
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

// Global flag for collision detection
bool collisionDetectionEnabled = true; // Default is enabled
// Global flag for debug visualization
bool showDebugVisualization = false; // Default is disabled

// Function to update animal position and state
void UpdateAnimal(Animal* animal, float terrainSize) {
    // Update timer for state changes
    animal->moveTimer += GetFrameTime();

    if (animal->type == ANIMAL_CHICKEN) {
        // Chicken-specific logic
        float enclosurePadding = 0.1f; // Consistent padding

        if (animal->moveTimer >= animal->moveInterval) {
            animal->moveTimer = 0.0f; // Reset timer for next decision
            animal->isMoving = true;  // Chickens tend to keep moving or turning

            // New direction: Adjust current direction by a random angle (smoother turns)
            float turnStrength = 45.0f; // Max turn in degrees for wandering
            float turnAngle = GetRandomValue(-turnStrength, turnStrength) * DEG2RAD;
            float currentAngleRad = atan2f(animal->direction.x, animal->direction.z);
            // Ensure there's a default direction if x and z are zero (e.g. at start)
            if (animal->direction.x == 0.0f && animal->direction.z == 0.0f) {
                currentAngleRad = GetRandomValue(0,360) * DEG2RAD;
            }
            float newAngleRad = currentAngleRad + turnAngle;

            animal->direction.x = sinf(newAngleRad);
            animal->direction.z = cosf(newAngleRad);
            animal->direction = Vector3Normalize(animal->direction);
            
            // Longer interval for chickens before random turn
            animal->moveInterval = 2.0f + GetRandomValue(0, 15)/10.0f; // 2.0 to 3.5 seconds
        }

        if (animal->isMoving) {
            Vector3 newPosition = animal->position; // Start with current position

            newPosition.x += animal->direction.x * animal->speed;
            newPosition.z += animal->direction.z * animal->speed;

            // Enclosure boundaries
            float minX = ENCLOSURE_CENTER_2.x - (ENCLOSURE_WIDTH_2 / 2.0f);
            float maxX = ENCLOSURE_CENTER_2.x + (ENCLOSURE_WIDTH_2 / 2.0f);
            float minZ = ENCLOSURE_CENTER_2.z - (ENCLOSURE_LENGTH_2 / 2.0f);
            float maxZ = ENCLOSURE_CENTER_2.z + (ENCLOSURE_LENGTH_2 / 2.0f);
            bool bounced = false;

            // X-axis reflection
            if (newPosition.x <= minX + enclosurePadding) {
                newPosition.x = minX + enclosurePadding;
                animal->direction.x *= -1.0f;
                animal->direction.z += GetRandomValue(-1, 1) / 20.0f; // Tiny perpendicular nudge
                bounced = true;
            } else if (newPosition.x >= maxX - enclosurePadding) {
                newPosition.x = maxX - enclosurePadding;
                animal->direction.x *= -1.0f;
                animal->direction.z += GetRandomValue(-1, 1) / 20.0f;
                bounced = true;
            }

            // Z-axis reflection
            if (newPosition.z <= minZ + enclosurePadding) {
                newPosition.z = minZ + enclosurePadding;
                animal->direction.z *= -1.0f;
                animal->direction.x += GetRandomValue(-1, 1) / 20.0f;
                bounced = true;
            } else if (newPosition.z >= maxZ - enclosurePadding) {
                newPosition.z = maxZ - enclosurePadding;
                animal->direction.z *= -1.0f;
                animal->direction.x += GetRandomValue(-1, 1) / 20.0f;
                bounced = true;
            }

            if (bounced) {
                animal->direction = Vector3Normalize(animal->direction);
                // Force commitment to bounce direction: Reset timer to make it take longer for next RANDOM turn decision
                animal->moveTimer = animal->moveInterval * 0.25f; // Positive value, spend 75% of interval committed to bounce
                                                                // Effectively, next random turn will be delayed.
            }
            animal->position = newPosition; // Position after bounce/clamp
            animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
        }
    } else if (animal->type == ANIMAL_PIG) {
        // Pig-specific logic
        float enclosurePadding = 0.1f; // Consistent padding

        if (animal->moveTimer >= animal->moveInterval) {
            animal->moveTimer = 0.0f; // Reset timer for next decision
            animal->isMoving = true;  // Pigs tend to keep moving or turning

            // New direction: Adjust current direction by a random angle (smoother turns)
            float turnStrength = 40.0f; // Max turn in degrees for wandering
            float turnAngle = GetRandomValue(-turnStrength, turnStrength) * DEG2RAD;
            float currentAngleRad = atan2f(animal->direction.x, animal->direction.z);
            // Ensure there's a default direction if x and z are zero (e.g. at start)
            if (animal->direction.x == 0.0f && animal->direction.z == 0.0f) {
                currentAngleRad = GetRandomValue(0,360) * DEG2RAD;
            }
            float newAngleRad = currentAngleRad + turnAngle;

            animal->direction.x = sinf(newAngleRad);
            animal->direction.z = cosf(newAngleRad);
            animal->direction = Vector3Normalize(animal->direction);
            
            // Interval for pigs before random turn
            animal->moveInterval = 1.8f + GetRandomValue(0, 15)/10.0f; // 1.8 to 3.3 seconds
        }

        if (animal->isMoving) {
            Vector3 newPosition = animal->position; // Start with current position

            newPosition.x += animal->direction.x * animal->speed;
            newPosition.z += animal->direction.z * animal->speed;

            // Enclosure boundaries
            float minX = ENCLOSURE_CENTER_1.x - (ENCLOSURE_WIDTH_1 / 2.0f);
            float maxX = ENCLOSURE_CENTER_1.x + (ENCLOSURE_WIDTH_1 / 2.0f);
            float minZ = ENCLOSURE_CENTER_1.z - (ENCLOSURE_LENGTH_1 / 2.0f);
            float maxZ = ENCLOSURE_CENTER_1.z + (ENCLOSURE_LENGTH_1 / 2.0f);
            bool bounced = false;

            // X-axis reflection
            if (newPosition.x <= minX + enclosurePadding) {
                newPosition.x = minX + enclosurePadding;
                animal->direction.x *= -1.0f;
                animal->direction.z += GetRandomValue(-1, 1) / 20.0f; // Tiny perpendicular nudge
                bounced = true;
            } else if (newPosition.x >= maxX - enclosurePadding) {
                newPosition.x = maxX - enclosurePadding;
                animal->direction.x *= -1.0f;
                animal->direction.z += GetRandomValue(-1, 1) / 20.0f;
                bounced = true;
            }

            // Z-axis reflection
            if (newPosition.z <= minZ + enclosurePadding) {
                newPosition.z = minZ + enclosurePadding;
                animal->direction.z *= -1.0f;
                animal->direction.x += GetRandomValue(-1, 1) / 20.0f;
                bounced = true;
            } else if (newPosition.z >= maxZ - enclosurePadding) {
                newPosition.z = maxZ - enclosurePadding;
                animal->direction.z *= -1.0f;
                animal->direction.x += GetRandomValue(-1, 1) / 20.0f;
                bounced = true;
            }

            if (bounced) {
                animal->direction = Vector3Normalize(animal->direction);
                // Force commitment to bounce direction: Reset timer to make it take longer for next RANDOM turn decision
                animal->moveTimer = animal->moveInterval * 0.3f; // Positive value, spend 70% of interval committed to bounce
            }
            animal->position = newPosition; // Position after bounce/clamp
            animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
        }
    } else {
        // Original logic for other animals (unchanged from previous state)
        if (animal->moveTimer >= animal->moveInterval) {
            animal->moveTimer = 0.0f;
            
            // Enhanced behavior for horses and cows to explore further
            if (animal->type == ANIMAL_HORSE || animal->type == ANIMAL_COW) {
                // Horses and cows are more likely to keep moving
                if (GetRandomValue(0, 100) < 85) {  // Increased probability to move (was 70)
                    animal->isMoving = true;
                    float distanceFromSpawn = Vector3Distance(animal->position, animal->spawnPosition);
                    
                    // Lower probability of returning to spawn for these animals unless they're really far away
                    if (GetRandomValue(0, 100) < 50 || distanceFromSpawn > animal->maxWanderDistance) {
                        if (distanceFromSpawn > animal->maxWanderDistance * 0.9f) {
                            // Only return to spawn when very close to max distance
                            Vector3 toSpawn = Vector3Subtract(animal->spawnPosition, animal->position);
                            animal->direction = Vector3Normalize(toSpawn);
                        } else {
                            // Otherwise make smaller turns to create more natural paths
                            float turnAmount = GetRandomValue(-30, 30) * DEG2RAD;
                            float currentAngle = atan2f(animal->direction.x, animal->direction.z);
                            float newAngle = currentAngle + turnAmount;
                            animal->direction.x = sinf(newAngle);
                            animal->direction.z = cosf(newAngle);
                        }
                    } else {
                        // Occasional large direction changes for more exploration (up to 180 degrees)
                        float turnAngle = GetRandomValue(-180, 180) * DEG2RAD;
                        float currentAngle = atan2f(animal->direction.x, animal->direction.z);
                        float newAngle = currentAngle + turnAngle;
                        animal->direction.x = sinf(newAngle);
                        animal->direction.z = cosf(newAngle);
                    }
                    
                    // Horses and cows move for longer periods
                    animal->moveInterval = (1.5f + GetRandomValue(0, 25) / 10.0f);
                } else {
                    animal->isMoving = false;
                    // Shorter resting periods
                    animal->moveInterval = (1.0f + GetRandomValue(0, 10) / 10.0f);
                }
            } else {
                // Original logic for other animals
                if (GetRandomValue(0, 100) < 70) { 
                    animal->isMoving = true;
                    float distanceFromSpawn = Vector3Distance(animal->position, animal->spawnPosition);
                    if (GetRandomValue(0, 100) < 80 || distanceFromSpawn > animal->maxWanderDistance) {
                        if (distanceFromSpawn > animal->maxWanderDistance * 0.7f) {
                            Vector3 toSpawn = Vector3Subtract(animal->spawnPosition, animal->position);
                            animal->direction = Vector3Normalize(toSpawn);
                        } else {
                            float turnAmount = GetRandomValue(-45, 45) * DEG2RAD;
                            float currentAngle = atan2f(animal->direction.x, animal->direction.z);
                            float newAngle = currentAngle + turnAmount;
                            animal->direction.x = sinf(newAngle);
                            animal->direction.z = cosf(newAngle);
                        }
                    } else {
                        float turnAngle = GetRandomValue(-90, 90) * DEG2RAD;
                        float currentAngle = atan2f(animal->direction.x, animal->direction.z);
                        float newAngle = currentAngle + turnAngle;
                        animal->direction.x = sinf(newAngle);
                        animal->direction.z = cosf(newAngle);
                    }
                    animal->moveInterval = (animal->isMoving ? 1.0f : 2.0f) + GetRandomValue(0, 20) / 10.0f;
                } else {
                    animal->isMoving = false;
                    animal->moveInterval = (animal->isMoving ? 1.0f : 2.0f) + GetRandomValue(0, 20) / 10.0f;
                }
            }
        }

        if (animal->isMoving) {
            Vector3 oldPosition = animal->position; 
            animal->position.x += animal->direction.x * animal->speed;
            animal->position.z += animal->direction.z * animal->speed;
            
            float boundary = terrainSize/2.0f - 2.0f;
            if (animal->position.x < -boundary) animal->position.x = -boundary;
            if (animal->position.x > boundary) animal->position.x = boundary;
            if (animal->position.z < -boundary) animal->position.z = -boundary;
            if (animal->position.z > boundary) animal->position.z = boundary;
            
            animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
        }
    }

    // --- Generic Collision Handling for ALL Animals (AFTER type-specific movement) ---
    if (animal->isMoving) { // Only check collisions if animal attempted to move
        Vector3 preCollisionPosition = animal->position; // Position after movement but before building/other animal collision

        // Check for collisions with buildings
        int collidedBuildingIndex = -1;
        // The IsCollisionWithBuilding function already has logic to ignore buildings with scale 1.0f (like the ChickenCoop)
        if (IsCollisionWithBuilding(animal->position, animal->scale * 0.7f, &collidedBuildingIndex)) {
            animal->position = Vector3MoveTowards(preCollisionPosition, buildings[collidedBuildingIndex].position, -animal->speed); // Try to step back slightly

            // Simplified bounce off building for all animals
            Vector3 awayFromBuilding = Vector3Normalize(Vector3Subtract(animal->position, buildings[collidedBuildingIndex].position));
            animal->direction.x = awayFromBuilding.x + GetRandomValue(-1,1)/10.0f;
            animal->direction.z = awayFromBuilding.z + GetRandomValue(-1,1)/10.0f;
            animal->direction = Vector3Normalize(animal->direction);
            animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
            animal->moveTimer = 0; // Re-evaluate direction quickly
        }

        // Check for collisions with other animals
        for (int i = 0; i < animalCount; i++) {
            if (!animals[i].active || &animals[i] == animal) continue;

            float distance = Vector3Distance(animal->position, animals[i].position);
            float minDistance = (animal->scale + animals[i].scale) * 0.6f; // Reduced for less pushing

            if (distance < minDistance) {
                Vector3 resolutionVector = Vector3Normalize(Vector3Subtract(animal->position, animals[i].position));
                // Move both animals away from each other slightly
                animal->position = Vector3Add(animal->position, Vector3Scale(resolutionVector, (minDistance - distance) / 2.0f));
                // animals[i].position = Vector3Subtract(animals[i].position, Vector3Scale(resolutionVector, (minDistance - distance) / 2.0f)); 
                // ^ Modifying other animals here can be complex; focus on current animal's reaction.
                
                // Current animal decides to change direction
                animal->direction = resolutionVector; // Move directly away
                animal->rotationAngle = atan2f(animal->direction.x, animal->direction.z) * RAD2DEG;
                animal->moveTimer = animal->moveInterval * 0.1f; // Quicker re-evaluation
            }
        }
    }
    // --- FINAL FAILSAFE CLAMP for Chickens (after all other collisions) ---
    if (animal->type == ANIMAL_CHICKEN) {
        float minX = ENCLOSURE_CENTER_2.x - (ENCLOSURE_WIDTH_2 / 2.0f);
        float maxX = ENCLOSURE_CENTER_2.x + (ENCLOSURE_WIDTH_2 / 2.0f);
        float minZ = ENCLOSURE_CENTER_2.z - (ENCLOSURE_LENGTH_2 / 2.0f);
        float maxZ = ENCLOSURE_CENTER_2.z + (ENCLOSURE_LENGTH_2 / 2.0f);
        float padding = 0.05f; // Minimal padding

        if (animal->position.x < minX + padding) animal->position.x = minX + padding;
        if (animal->position.x > maxX - padding) animal->position.x = maxX - padding;
        if (animal->position.z < minZ + padding) animal->position.z = minZ + padding;
        if (animal->position.z > maxZ - padding) animal->position.z = maxZ - padding;
    }
    // --- FINAL FAILSAFE CLAMP for Pigs (after all other collisions) ---
    else if (animal->type == ANIMAL_PIG) {
        float minX = ENCLOSURE_CENTER_1.x - (ENCLOSURE_WIDTH_1 / 2.0f);
        float maxX = ENCLOSURE_CENTER_1.x + (ENCLOSURE_WIDTH_1 / 2.0f);
        float minZ = ENCLOSURE_CENTER_1.z - (ENCLOSURE_LENGTH_1 / 2.0f);
        float maxZ = ENCLOSURE_CENTER_1.z + (ENCLOSURE_LENGTH_1 / 2.0f);
        float padding = 0.05f; // Minimal padding

        if (animal->position.x < minX + padding) animal->position.x = minX + padding;
        if (animal->position.x > maxX - padding) animal->position.x = maxX - padding;
        if (animal->position.z < minZ + padding) animal->position.z = minZ + padding;
        if (animal->position.z > maxZ - padding) animal->position.z = maxZ - padding;
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
        
        // Check if player is near bank or on road to bank - disable collision in that area
        bool nearBankArea = IsNearBankOrOnRoadToBank(newPosition);
        
        // Skip collision checks if collision detection is disabled or near bank area
        if (!collisionDetectionEnabled || nearBankArea) {
            // No collision checks - apply movement directly
            camera->position = newPosition;
            camera->target = Vector3Add(camera->target, translation);
        } else {
            // Collision detection is enabled - perform collision checks
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

// Function to check if an animal is colliding with a building or tree
bool IsCollisionWithBuilding(Vector3 position, float radius, int* buildingIndex) {
    // First check if we're in the bank area or on the road to the bank
    if (IsNearBankOrOnRoadToBank(position)) {
        if (buildingIndex != NULL) *buildingIndex = -2; // Special value to indicate bank area
        return false; // No collision in bank area
    }
    
    // First check buildings
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (buildings[i].model.meshCount == 0) continue; // Skip uninitialized buildings

        // Skip collision check for the chicken coop by checking its scale
        if (buildings[i].scale == 1.0f) {
            continue; // Skip the chicken coop
        }

        // Calculate distance between position and building center
        float distance = Vector3Distance(position, buildings[i].position);
        
        // Use a collision radius based on the building type/index
        float buildingRadius;
        if (i == 0) { // barn.glb (index 0)
            buildingRadius = 6.0f; 
        } else if (i == 1) { // horse_barn.glb (index 1)
            buildingRadius = 5.0f; // Adjusted fixed radius
        } else if (i == 2) { // Bank.glb (index 2)
            buildingRadius = 10.0f; // Specific radius for Bank due to its small model scale
        } else if (i == 3) { // constructionHouse.glb (index 3)
            buildingRadius = 3.0f;  // Reduced specific radius for Construction House
        } else if (i == 4) { // FarmHouse.glb (index 4)
            buildingRadius = 2.0f;  // Reduced collision radius for FarmHouse
        } else { 
            // For fences or other unlisted buildings
            // Assuming FENCE_MODEL_SCALE_CONST is 0.2f for fences
            const float FENCE_MODEL_SCALE_CONST = 0.2f; 
            if (buildings[i].scale == FENCE_MODEL_SCALE_CONST) { // Likely a fence
                buildingRadius = 1.0f; // Small radius for fence segments
            } else {
                // Generic scaling for other unknown buildings, using a base factor.
                // Adjust this factor as needed if other buildings are added with varying scales.
                buildingRadius = buildings[i].scale * 20.0f; 
                if (buildingRadius < 1.5f) buildingRadius = 1.5f; // Minimum generic radius
            }
        }
        
        // Check for collision
        if (distance < (radius + buildingRadius)) {
            if (buildingIndex != NULL) *buildingIndex = i;
            return true;
        }
    }

    // Then check trees
    for (int i = 0; i < plantCount; i++) {
        if (!plants[i].active || plants[i].type != PLANT_TREE) continue;

        // Calculate distance between position and tree center
        float distance = Vector3Distance(position, plants[i].position);
        
        // Tree collision radius based on scale
        float treeRadius = plants[i].scale * 1.7f; // Adjust this multiplier as needed
        
        // Check for collision
        if (distance < (radius + treeRadius)) {
            if (buildingIndex != NULL) *buildingIndex = -1; // Use -1 to indicate tree collision
            return true;
        }
    }
    
    return false;
}

// Function to check if the player is colliding with an animal
bool IsCollisionWithAnimal(Vector3 playerPosition, float playerRadius, int* animalIndex) {
    // First check if we're in the bank area or on the road to the bank
    if (IsNearBankOrOnRoadToBank(playerPosition)) {
        if (animalIndex != NULL) *animalIndex = -1;
        return false; // No collision in bank area
    }
    
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

// Sound-related constants
#define MAX_SOUND_DISTANCE 30.0f  // Maximum distance at which sounds can be heard
#define MIN_SOUND_INTERVAL 9.5f   // Minimum time between sounds (in seconds)
#define MAX_SOUND_INTERVAL 17.0f  // Maximum time between sounds (in seconds)
#define MIN_SOUNDS_PER_MINUTE 8   // Minimum number of sounds per minute

// Chicken-specific sound intervals (more frequent)
#define CHICKEN_MIN_SOUND_INTERVAL 15.0f  // Chickens make sounds more frequently
#define CHICKEN_MAX_SOUND_INTERVAL 25.0f  // Maximum time between chicken sounds

// Sound structure for animals
typedef struct {
    Sound sound;
    float nextSoundTime;
    float soundInterval;
} AnimalSound;

// Function to load animal sounds
void LoadAnimalSounds(void) {
    // Initialize audio device
    InitAudioDevice();
    
    // Load sounds for each animal type
    for (int i = 0; i < animalCount; i++) {
        if (!animals[i].active) continue;
        
        AnimalSound* animalSound = (AnimalSound*)MemAlloc(sizeof(AnimalSound));
        if (animalSound == NULL) {
            TraceLog(LOG_ERROR, "Failed to allocate memory for animal sound");
            continue;
        }
        
        // Load appropriate sound based on animal type
        switch(animals[i].type) {
            case ANIMAL_HORSE:
                animalSound->sound = LoadSound("sounds/horse.mp3");
                break;
            case ANIMAL_CAT:
                animalSound->sound = LoadSound("sounds/cat.mp3");
                break;
            case ANIMAL_DOG:
                animalSound->sound = LoadSound("sounds/dog.mp3");
                break;
            case ANIMAL_COW:
                animalSound->sound = LoadSound("sounds/cow.mp3");
                break;
            case ANIMAL_PIG:
                animalSound->sound = LoadSound("sounds/pig.mp3");
                break;

            case ANIMAL_CHICKEN:
                animalSound->sound = LoadSound("sounds/chicken.mp3");
                break;
             
            default:
                animalSound->sound = (Sound){0};
                break;
        }
        
        // Initialize sound timing
        animalSound->nextSoundTime = GetTime() + GetRandomValue(0, 5); // Random initial delay
        animalSound->soundInterval = MIN_SOUND_INTERVAL + 
            (float)GetRandomValue(0, (int)((MAX_SOUND_INTERVAL - MIN_SOUND_INTERVAL) * 100)) / 100.0f;
        
        // Store the sound data with the animal
        animals[i].soundData = animalSound;
    }
}

// Function to play animal sounds with distance-based volume
void PlayAnimalSound(Animal* animal, Camera camera) {
    if (!animal || !animal->active || !animal->soundData) return;
    
    AnimalSound* soundData = (AnimalSound*)animal->soundData;
    float currentTime = GetTime();
    
    // Check if it's time to play the sound
    if (currentTime >= soundData->nextSoundTime) {
        // Calculate distance to camera
        float distance = Vector3Distance(animal->position, camera.position);
        
        // Only play sound if within hearing range
        if (distance <= MAX_SOUND_DISTANCE) {
            // Calculate volume based on distance (1.0 at 0 distance, 0.0 at MAX_SOUND_DISTANCE)
            float volume = 1.0f - (distance / MAX_SOUND_DISTANCE);
            volume = Clamp(volume, 0.0f, 1.0f);
            
            // Set volume and play sound
            SetSoundVolume(soundData->sound, volume);
            PlaySound(soundData->sound);
            
            // Schedule next sound
            soundData->nextSoundTime = currentTime + soundData->soundInterval;
            // Randomize next interval
            soundData->soundInterval = MIN_SOUND_INTERVAL + 
                (float)GetRandomValue(0, (int)((MAX_SOUND_INTERVAL - MIN_SOUND_INTERVAL) * 100)) / 100.0f;
        }
    }
}

// Function to unload animal sounds
void UnloadAnimalSounds(void) {
    for (int i = 0; i < animalCount; i++) {
        if (!animals[i].active || !animals[i].soundData) continue;
        
        AnimalSound* soundData = (AnimalSound*)animals[i].soundData;
        UnloadSound(soundData->sound);
        MemFree(soundData);
        animals[i].soundData = NULL;
    }
    
    CloseAudioDevice();
}

// Function to check if a position is inside the chicken enclosure
bool IsPositionInChickenEnclosure(Vector3 position) {
    // Define the boundaries of the chicken enclosure (second enclosure)
    float minX = ENCLOSURE_CENTER_2.x - (ENCLOSURE_WIDTH_2 / 2.0f);
    float maxX = ENCLOSURE_CENTER_2.x + (ENCLOSURE_WIDTH_2 / 2.0f);
    float minZ = ENCLOSURE_CENTER_2.z - (ENCLOSURE_LENGTH_2 / 2.0f);
    float maxZ = ENCLOSURE_CENTER_2.z + (ENCLOSURE_LENGTH_2 / 2.0f);

    // Add a small buffer to keep chickens away from the fence
    float buffer = 1.0f;
    return (position.x > minX + buffer && position.x < maxX - buffer &&
            position.z > minZ + buffer && position.z < maxZ - buffer);
}

// Function to get a random position inside the chicken enclosure
Vector3 GetRandomChickenEnclosurePosition(void) {
    float minX = ENCLOSURE_CENTER_2.x - (ENCLOSURE_WIDTH_2 / 2.0f);
    float maxX = ENCLOSURE_CENTER_2.x + (ENCLOSURE_WIDTH_2 / 2.0f);
    float minZ = ENCLOSURE_CENTER_2.z - (ENCLOSURE_LENGTH_2 / 2.0f);
    float maxZ = ENCLOSURE_CENTER_2.z + (ENCLOSURE_LENGTH_2 / 2.0f);

    // Add buffer to keep away from fence
    float buffer = 1.0f;
    Vector3 position;
    position.x = GetRandomValue((int)((minX + buffer) * 100), (int)((maxX - buffer) * 100)) / 100.0f;
    position.z = GetRandomValue((int)((minZ + buffer) * 100), (int)((maxZ - buffer) * 100)) / 100.0f;
    position.y = 0.0f;
    return position;
}

// Function to spawn multiple chickens in the enclosure
void SpawnChickensInEnclosure(int count) {
    for (int i = 0; i < count && animalCount < MAX_ANIMALS; i++) {
        Vector3 position = GetRandomChickenEnclosurePosition();
        SpawnAnimal(ANIMAL_CHICKEN, position, FIXED_TERRAIN_SIZE);
    }
}

// Function to check if a position is inside the pig enclosure
bool IsPositionInPigEnclosure(Vector3 position) {
    // Define the boundaries of the pig enclosure (first enclosure)
    float minX = ENCLOSURE_CENTER_1.x - (ENCLOSURE_WIDTH_1 / 2.0f);
    float maxX = ENCLOSURE_CENTER_1.x + (ENCLOSURE_WIDTH_1 / 2.0f);
    float minZ = ENCLOSURE_CENTER_1.z - (ENCLOSURE_LENGTH_1 / 2.0f);
    float maxZ = ENCLOSURE_CENTER_1.z + (ENCLOSURE_LENGTH_1 / 2.0f);

    // Add a small buffer to keep pigs away from the fence
    float buffer = 1.0f;
    return (position.x > minX + buffer && position.x < maxX - buffer &&
            position.z > minZ + buffer && position.z < maxZ - buffer);
}

// Function to get a random position inside the pig enclosure
Vector3 GetRandomPigEnclosurePosition(void) {
    float minX = ENCLOSURE_CENTER_1.x - (ENCLOSURE_WIDTH_1 / 2.0f);
    float maxX = ENCLOSURE_CENTER_1.x + (ENCLOSURE_WIDTH_1 / 2.0f);
    float minZ = ENCLOSURE_CENTER_1.z - (ENCLOSURE_LENGTH_1 / 2.0f);
    float maxZ = ENCLOSURE_CENTER_1.z + (ENCLOSURE_LENGTH_1 / 2.0f);

    // Add buffer to keep away from fence
    float buffer = 1.0f;
    Vector3 position;
    position.x = GetRandomValue((int)((minX + buffer) * 100), (int)((maxX - buffer) * 100)) / 100.0f;
    position.z = GetRandomValue((int)((minZ + buffer) * 100), (int)((maxZ - buffer) * 100)) / 100.0f;
    position.y = 0.0f;
    return position;
}

// Function to spawn multiple pigs in the enclosure
void SpawnPigsInEnclosure(int count) {
    for (int i = 0; i < count && animalCount < MAX_ANIMALS; i++) {
        Vector3 position = GetRandomPigEnclosurePosition();
        SpawnAnimal(ANIMAL_PIG, position, FIXED_TERRAIN_SIZE);
    }
}

// Function to check if player is on road to the bank or near the bank
bool IsNearBankOrOnRoadToBank(Vector3 position) {
    // Check if near the bank
    Vector3 bankPosition = buildings[2].position;
    float bankRadius = 15.0f; // Large radius around the bank
    
    if (Vector3Distance(position, bankPosition) < bankRadius) {
        return true; // Near the bank
    }
    
    // Check if on road to bank (second road)
    if (totalCustomRoadsCount >= 2) {
        CustomRoad* bankRoad = &allCustomRoads[1]; // Second road (to bank)
        
        // Check each segment of this road
        for (int segmentIndex = 0; segmentIndex < bankRoad->numPoints - 1; segmentIndex++) {
            Vector3 p1 = bankRoad->points[segmentIndex];
            Vector3 p2 = bankRoad->points[segmentIndex + 1];
            
            // Project position onto the road segment
            Vector3 segmentVec = Vector3Subtract(p2, p1);
            Vector3 posVec = Vector3Subtract(position, p1);
            
            float segmentLengthSq = Vector3LengthSqr(segmentVec);
            if (segmentLengthSq == 0.0f) continue; // Avoid division by zero
            
            float t = Vector3DotProduct(posVec, segmentVec) / segmentLengthSq;
            t = Clamp(t, 0.0f, 1.0f); // Ensure projection is on the segment
            
            Vector3 closestPoint = Vector3Add(p1, Vector3Scale(segmentVec, t));
            
            // Check if player is on or near the road
            float roadBuffer = roadWidth * 1.5f; // Wider buffer for player movement
            
            if (Vector3Distance(position, closestPoint) < roadBuffer) {
                return true; // On or near the road to bank
            }
        }
    }
    
    return false;
}

// Human character function definitions

// Function to initialize a human character
void InitHuman(Human* h) {
    TraceLog(LOG_INFO, "Initializing human character");
    
    // Create a default cube model as a fallback in case model loading fails
    Model fallbackModel = LoadModelFromMesh(GenMeshCube(1.0f, 2.0f, 1.0f));
    
    // Load models from the resources/humans folder with extended error handling
    TraceLog(LOG_INFO, "Loading human walking model...");
    h->walkingModel = LoadModel("humans/walking_character.glb");
    
    TraceLog(LOG_INFO, "Loading human idle model...");
    h->idleModel = LoadModel("humans/idle_character.glb");
    
    TraceLog(LOG_INFO, "Loading human looking model...");
    h->lookingModel = LoadModel("humans/looking_character.glb");
    
    // Check if models loaded successfully and use fallbacks if needed
    if (h->walkingModel.meshCount == 0) {
        TraceLog(LOG_ERROR, "Failed to load humans/walking_character.glb - using fallback cube");
        h->walkingModel = fallbackModel;
    }
    
    if (h->idleModel.meshCount == 0) {
        TraceLog(LOG_ERROR, "Failed to load humans/idle_character.glb - using fallback cube");
        h->idleModel = fallbackModel;
    }
    
    if (h->lookingModel.meshCount == 0) {
        TraceLog(LOG_ERROR, "Failed to load humans/looking_character.glb - using fallback cube");
        h->lookingModel = fallbackModel;
    }
    
    // Initialize animation pointers to NULL
    h->walkingAnim = NULL;
    h->idleAnim = NULL;
    h->lookingAnim = NULL;
    h->walkingAnimCount = 0;
    h->idleAnimCount = 0;
    h->lookingAnimCount = 0;
    
    // Load animations - this is where errors might occur
    TraceLog(LOG_INFO, "Loading human animations");
    h->walkingAnim = LoadModelAnimations("humans/walking_character.glb", &h->walkingAnimCount);
    h->idleAnim = LoadModelAnimations("humans/idle_character.glb", &h->idleAnimCount);
    h->lookingAnim = LoadModelAnimations("humans/looking_character.glb", &h->lookingAnimCount);
    h->animFrameCounter = 0;
    
    // Check if animations loaded successfully
    if (h->walkingAnimCount > 0) {
        TraceLog(LOG_INFO, "Human walking animation loaded successfully with %d frames", h->walkingAnim[0].frameCount);
    } else {
        TraceLog(LOG_ERROR, "Failed to load human walking animation");
    }
    
    if (h->idleAnimCount > 0) {
        TraceLog(LOG_INFO, "Human idle animation loaded successfully with %d frames", h->idleAnim[0].frameCount);
    } else {
        TraceLog(LOG_ERROR, "Failed to load human idle animation");
        // If no idle animation, copy walking animation as fallback
        if (h->walkingAnimCount > 0) {
            TraceLog(LOG_INFO, "Using walking animation as fallback for idle");
            h->idleModel = h->walkingModel;
            h->idleAnim = h->walkingAnim;
            h->idleAnimCount = h->walkingAnimCount;
        }
    }
    
    if (h->lookingAnimCount > 0) {
        TraceLog(LOG_INFO, "Human looking animation loaded successfully with %d frames", h->lookingAnim[0].frameCount);
    } else {
        TraceLog(LOG_ERROR, "Failed to load human looking animation");
        // If no looking animation, use idle animation as fallback
        if (h->idleAnimCount > 0) {
            TraceLog(LOG_INFO, "Using idle animation as fallback for looking");
            h->lookingModel = h->idleModel;
            h->lookingAnim = h->idleAnim;
            h->lookingAnimCount = h->idleAnimCount;
        }
    }
    
    // Set default values
    h->position = (Vector3){ 0.0f, 0.3f, 0.0f };  // Slightly above ground level
    h->targetPosition = (Vector3){ 0.0f, 0.3f, 0.0f };
    h->direction = (Vector3){ 0.0f, 0.0f, 1.0f };  // Default facing forward
    h->speed = 0.05f;  // Significantly reduced speed for slower, more natural movement
    h->scale = 1.0f;   // Further reduced scale from 2.0 to 1.0 for better proportions
    h->rotationAngle = 0.0f;
    h->state = HUMAN_STATE_WALKING;  // Start in walking state
    h->stateTimer = 0.0f;
    h->disappearAlpha = 1.0f;
    // Shortened message to fit within the 128 character buffer
    strncpy(h->dialogMessage, "Welcome to grandpa's farm! I'm here to guide you. Your grandpa left this place for you to continue his legacy. Let's make it thrive!", 127);
    h->dialogMessage[127] = '\0'; // Ensure null termination
    h->showDialog = false;
    h->dialogTimer = 0.0f;
    h->currentPathIndex = 0;
    h->pathPointCount = 0;
    h->active = true;  // Start active
    h->waitForKeyPress = false;
    
    TraceLog(LOG_INFO, "Human character initialized successfully with speed %.3f", h->speed);
}

// Check if a position is at a road intersection
bool IsRoadIntersection(Vector3 position, float threshold) {
    // First check against actual road intersections
    int crossingCount = 0;
    
    // Check each road to count how many are near this position
    for (int i = 0; i < totalCustomRoadsCount; i++) {
        for (int j = 0; j < allCustomRoads[i].numPoints; j++) {
            if (Vector3Distance(position, allCustomRoads[i].points[j]) < threshold) {
                crossingCount++;
                break;  // Count each road only once
            }
        }
    }
    
    // If more than one road is nearby, it's likely an intersection
    if (crossingCount > 1) {
        TraceLog(LOG_INFO, "Found road intersection at (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
        return true;
    }
    
    // For direct paths (fallback), create a virtual intersection at the midpoint
    // Only check this for the human character's path
    if (human.active && human.pathPointCount == 2) {
        // Calculate midpoint of the direct path
        Vector3 midpoint = {
            (human.pathPoints[0].x + human.pathPoints[1].x) / 2.0f,
            (human.pathPoints[0].y + human.pathPoints[1].y) / 2.0f,
            (human.pathPoints[0].z + human.pathPoints[1].z) / 2.0f
        };
        
        // Check if the position is near the midpoint
        if (Vector3Distance(position, midpoint) < threshold) {
            TraceLog(LOG_INFO, "Found virtual intersection at midpoint (%.2f, %.2f, %.2f)", 
                     midpoint.x, midpoint.y, midpoint.z);
            return true;
        }
    }
    
    return false;
}

// Function to set up the human path from farmhouse to barn
void SetupHumanPath(Human* h, Vector3 startPos, Vector3 endPos) {
    // Log input parameters
    TraceLog(LOG_INFO, "SetupHumanPath called with start=(%.2f, %.2f, %.2f), end=(%.2f, %.2f, %.2f)",
             startPos.x, startPos.y, startPos.z, endPos.x, endPos.y, endPos.z);
    
    // Log initial human state
    TraceLog(LOG_INFO, "Human state before path setup: active=%d, state=%d",
             h->active, h->state);
    
    // Make sure the human is slightly above ground to avoid Z-fighting
    h->position = startPos;
    h->position.y = 0.3f;
    h->pathPoints[0] = h->position;
    
    // Force the human to be active and in walking state immediately
    h->active = true;
    h->state = HUMAN_STATE_WALKING;
    
    TraceLog(LOG_INFO, "Setting up human path from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)", 
             h->position.x, h->position.y, h->position.z, endPos.x, endPos.y, endPos.z);
    
    // Set up direct path from start to end
    h->pathPointCount = 2;
    h->pathPoints[0] = h->position;
    h->pathPoints[1] = endPos;
    h->pathPoints[1].y = 0.3f; // Consistent height
    h->currentPathIndex = 0;
    h->targetPosition = h->pathPoints[1];
    
    // Calculate initial direction for direct path
    Vector3 pathVector = Vector3Subtract(h->targetPosition, h->position);
    float pathLength = Vector3Length(pathVector);
    
    // Make sure we have a valid direction
    if (pathLength > 0.001f) {
        h->direction = Vector3Scale(pathVector, 1.0f/pathLength); // Normalize manually
    } else {
        // Fallback direction if start and end are too close
        h->direction = (Vector3){ 0.0f, 0.0f, 1.0f };
        TraceLog(LOG_WARNING, "Path points too close, using default direction");
    }
    
    // Calculate initial rotation angle for direct path
    h->rotationAngle = atan2f(h->direction.x, h->direction.z) * RAD2DEG;
    
    // Reset state timer
    h->stateTimer = 0.0f;
    
    // Log the direction vector
    TraceLog(LOG_INFO, "Human direction set to (%.2f, %.2f, %.2f), rotation=%.2f", 
             h->direction.x, h->direction.y, h->direction.z, h->rotationAngle);
    
    // Log that we're activating the human
    TraceLog(LOG_INFO, "Human activated with direct path! active=%d, state=%d", 
            h->active, h->state);
}

// Function to draw the human character
void DrawHuman(Human* h, Camera camera) {
    // Add debug logging
    TraceLog(LOG_INFO, "DrawHuman called: active=%d, state=%d, position=(%.2f, %.2f, %.2f)", 
        h->active, h->state, h->position.x, h->position.y, h->position.z);
    
    // Removed debug markers: sphere, cube, vertical line, path point spheres/lines, destination cube.
    
    if (!h->active) {
        TraceLog(LOG_WARNING, "Skipping human model draw because active=false");
        return;
    }
    
    // Determine which model to use based on state
    Model modelToDraw;
    switch (h->state) {
        case HUMAN_STATE_WALKING:
            modelToDraw = h->walkingModel;
            TraceLog(LOG_INFO, "Using walking model for human");
            break;
        // HUMAN_STATE_IDLE_AT_INTERSECTION will now use the idle model for its 3D representation
        // The full-screen menu is handled separately by DrawHumanStartMenu
        case HUMAN_STATE_IDLE_AT_INTERSECTION: 
        case HUMAN_STATE_TALKING: // Fallthrough, TALKING might also use idle or looking
            modelToDraw = h->idleModel;  // Use idle model for idle/talking state
            TraceLog(LOG_INFO, "Using idle model for human while idle/talking");
            break;
        case HUMAN_STATE_DISAPPEARING:
            modelToDraw = h->idleModel;
            TraceLog(LOG_INFO, "Using idle model for disappearing human");
            break;
        default:
            TraceLog(LOG_WARNING, "Human in unexpected state: %d", h->state);
            modelToDraw = h->idleModel; // Default to idle model
            break;
    }
    
    // Check if model is valid
    if (modelToDraw.meshCount == 0) {
        TraceLog(LOG_ERROR, "Invalid model for human state %d", h->state);
        return;
    }
    
    // Calculate alpha for disappearing effect
    Color tint = WHITE;
    if (h->state == HUMAN_STATE_DISAPPEARING) {
        tint.a = (unsigned char)(h->disappearAlpha * 255.0f);
    }
    
    // Draw the human model
    TraceLog(LOG_INFO, "Drawing human model at (%.2f, %.2f, %.2f) with rotation %.2f, scale %.2f", 
           h->position.x, h->position.y, h->position.z, h->rotationAngle, h->scale);
    
    DrawModelEx(modelToDraw,
              h->position,
              (Vector3){0.0f, 1.0f, 0.0f},  // Rotation axis (Y-axis)
              h->rotationAngle,         // Rotation angle
              (Vector3){h->scale, h->scale, h->scale},
              tint);
    
    // Removed the old SIMPLE DIALOG MENU that was here for debugging.
    // The new full-screen start menu is handled by DrawHumanStartMenu() in the main loop.
}

// Function to unload human character resources
void UnloadHumanResources(Human* h) {
    UnloadModel(h->walkingModel);
    UnloadModel(h->idleModel);
    UnloadModel(h->lookingModel);
    
    // Unload animations
    if (h->walkingAnim != NULL && h->walkingAnimCount > 0) {
        UnloadModelAnimations(h->walkingAnim, h->walkingAnimCount);
    }
    
    if (h->idleAnim != NULL && h->idleAnimCount > 0) {
        UnloadModelAnimations(h->idleAnim, h->idleAnimCount);
    }
    
    if (h->lookingAnim != NULL && h->lookingAnimCount > 0) {
        UnloadModelAnimations(h->lookingAnim, h->lookingAnimCount);
    }
}

// Function to update the human character
void UpdateHuman(Human* h, float deltaTime) {
    // Add debug logging at the start of UpdateHuman
    TraceLog(LOG_INFO, "UpdateHuman called: active=%d, state=%d, position=(%.2f, %.2f, %.2f)", 
              h->active, h->state, h->position.x, h->position.y, h->position.z);
              
    // EMERGENCY FIX: If position is too far from the origin, reset to origin
    float distanceFromOrigin = Vector3Length(h->position);
    if (distanceFromOrigin > 500.0f) {
        TraceLog(LOG_WARNING, "Human position too far (%.2f units), resetting to origin", distanceFromOrigin);
        h->position = (Vector3){ 0.0f, 0.3f, 0.0f };
    }
    
    // If human is not active, no updates needed for its logic
    if (!h->active) {
        // The DrawHumanStartMenu will handle making the human inactive.
        // The H key press in main can reactivate it.
        return; 
    }
    
    // Update based on current state
    switch (h->state) {
        case HUMAN_STATE_WALKING: {
            TraceLog(LOG_INFO, "Human WALKING, time=%.2f", h->stateTimer);
            
            // Update walking animation
            if (h->walkingAnimCount > 0) {
                h->animFrameCounter++;
                UpdateModelAnimation(h->walkingModel, h->walkingAnim[0], h->animFrameCounter);
                if (h->animFrameCounter >= h->walkingAnim[0].frameCount) {
                    h->animFrameCounter = 0;
                }
                TraceLog(LOG_INFO, "Playing walking animation frame %d", h->animFrameCounter);
            } else {
                TraceLog(LOG_WARNING, "No walking animation available");
            }
            
            // Calculate movement distance for this frame
            float moveDistance = h->speed * deltaTime * 5.0f; // Significantly reduced from 100.0f to 5.0f for slower movement
            TraceLog(LOG_INFO, "Moving distance: %.4f per frame", moveDistance);
            
            // Calculate direction to current target point (second path point)
            Vector3 pathDirection = Vector3Normalize(Vector3Subtract(h->targetPosition, h->position));
            
            // Use exact path direction for movement - no blending with X-axis
            Vector3 exactPathDirection = pathDirection;
            
            // Move precisely along path
            Vector3 movement = Vector3Scale(exactPathDirection, moveDistance);
            h->position = Vector3Add(h->position, movement);
            
            // Update rotation to face movement direction
            h->rotationAngle = atan2f(exactPathDirection.x, exactPathDirection.z) * RAD2DEG;
            
            // Log the movement
            TraceLog(LOG_INFO, "Human position updated to (%.2f, %.2f, %.2f)", 
                     h->position.x, h->position.y, h->position.z);
            
            // Update timer but don't use it for state transitions
            h->stateTimer += deltaTime;
            
            // Check distance to destination instead of using timer
            float distanceToTarget = Vector3Distance(h->position, h->targetPosition);
            TraceLog(LOG_WARNING, "Walking time: %.2f, distance to target: %.2f", h->stateTimer, distanceToTarget);
            
            // Only switch states when we're close enough to the destination
            if (distanceToTarget < 0.5f) {
                // Arrived at destination, switch to idle state for start menu
                TraceLog(LOG_WARNING, "REACHED DESTINATION - SWITCHING TO IDLE FOR START MENU");
                h->state = HUMAN_STATE_IDLE_AT_INTERSECTION;
                h->stateTimer = 0.0f;
                h->animFrameCounter = 0; // Reset animation counter for new state
                                
                // Force human to stay active for the menu
                h->active = true; 
                
                TraceLog(LOG_WARNING, "Human reached destination, transitioning to HUMAN_STATE_IDLE_AT_INTERSECTION.");
                TraceLog(LOG_WARNING, "Human state after transition: %d, active: %d", 
                       h->state, h->active);
                
                // Snap to exact destination position to avoid floating point imprecision
                h->position = h->targetPosition;
            }
            
            break;
        }
            
        case HUMAN_STATE_IDLE_AT_INTERSECTION: {
            TraceLog(LOG_INFO, "Human IDLE_AT_INTERSECTION (Start Menu State)");

            h->active = true; // Human stays active to display the menu

            // Update idle animation
            if (h->idleAnimCount > 0) {
                h->animFrameCounter++;
                UpdateModelAnimation(h->idleModel, h->idleAnim[0], h->animFrameCounter);
                if (h->animFrameCounter >= h->idleAnim[0].frameCount) {
                    h->animFrameCounter = 0;
                }
            }
            // The DrawHumanStartMenu function handles input and state change to HUMAN_STATE_INACTIVE.
            // No timed transition out of this state from here.
            break;
        }
            
        case HUMAN_STATE_TALKING: {
            // This state is not currently used in the primary welcome flow.
            // If it were to be used, it would need its own dialog handling.
            TraceLog(LOG_WARNING, "Human TALKING (currently unused in welcome flow), dialog=%d, waitForKey=%d", h->showDialog, h->waitForKeyPress);
            
            h->active = true; // Ensure human is active if this state is somehow entered
            // Play an animation (e.g., looking or idle)
            if (h->lookingAnimCount > 0) {
                h->animFrameCounter++;
                UpdateModelAnimation(h->lookingModel, h->lookingAnim[0], h->animFrameCounter);
                if (h->animFrameCounter >= h->lookingAnim[0].frameCount) {
                    h->animFrameCounter = 0;
                }
            } else if (h->idleAnimCount > 0) {
                h->animFrameCounter++;
                UpdateModelAnimation(h->idleModel, h->idleAnim[0], h->animFrameCounter);
                if (h->animFrameCounter >= h->idleAnim[0].frameCount) {
                    h->animFrameCounter = 0;
                }
            }
            
            // Example: If it had its own dialog condition and timer
            // if (h->showDialog && h->waitForKeyPress) { ... }
            break;
        }
            
        case HUMAN_STATE_DISAPPEARING:
            // This state could be used for a fade-out animation if desired.
            // For now, it will just lead to inactive.
            TraceLog(LOG_INFO, "Human DISAPPEARING");
            h->disappearAlpha -= deltaTime * 0.5f; // Example fade out speed
            if (h->disappearAlpha <= 0.0f) {
                h->disappearAlpha = 0.0f;
                h->active = false; // Fully disappeared
                h->state = HUMAN_STATE_INACTIVE;
            }
            break;

        case HUMAN_STATE_INACTIVE:
            // Human is inactive, do nothing further here.
            // DrawHuman will not draw the model.
            // DrawHumanStartMenu will not draw the menu.
            TraceLog(LOG_INFO, "Human INACTIVE");
            h->active = false; // Ensure active is false
            break;

        default:
            TraceLog(LOG_WARNING, "Human in unexpected state %d, resetting to WALKING state", h->state);
            h->active = true;
            h->state = HUMAN_STATE_WALKING;
            h->stateTimer = 0.0f;
            h->currentPathIndex = 0; // Assuming path needs to be reset or re-evaluated
            // Re-initialize direction or path if necessary here
            // For safety, ensure pathPoints and targetPosition are valid if resetting to walking.
            if (h->pathPointCount > 0) {
                 h->targetPosition = h->pathPoints[h->currentPathIndex < h->pathPointCount ? h->currentPathIndex : 0];
                 h->direction = Vector3Normalize(Vector3Subtract(h->targetPosition, h->position));
                 h->rotationAngle = atan2f(h->direction.x, h->direction.z) * RAD2DEG;
            } else {
                // Fallback if no path, set a default direction
                h->direction = (Vector3){0.0f, 0.0f, 1.0f};
                h->rotationAngle = 0.0f;
            }
            h->showDialog = false;
            h->waitForKeyPress = false;
            h->disappearAlpha = 1.0f; // Ensure visible if reactivated
            break;
    }
}

// Function to draw the full-screen start menu for the human character
void DrawHumanStartMenu(Human* h) {
    if (h->active && h->state == HUMAN_STATE_IDLE_AT_INTERSECTION) {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // Draw full-screen semi-transparent background
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.85f));

        // Welcome Text
        const char* titleText = "Welcome to Grandpa's Farm!";
        int titleFontSize = 50; // Larger font for title
        int titleTextWidth = MeasureText(titleText, titleFontSize);
        DrawText(titleText, screenWidth/2 - titleTextWidth/2, screenHeight/2 - 150, titleFontSize, WHITE);
        
        const char* subText1 = "Your grandpa left this place for you to continue his legacy.";
        int subFontSize = 22;
        int subText1Width = MeasureText(subText1, subFontSize);
        DrawText(subText1, screenWidth/2 - subText1Width/2, screenHeight/2 - 80, subFontSize, LIGHTGRAY);

        const char* subText2 = "Let's make it thrive!";
        int subText2Width = MeasureText(subText2, subFontSize);
        DrawText(subText2, screenWidth/2 - subText2Width/2, screenHeight/2 - 50, subFontSize, LIGHTGRAY);

        // "START GAME" Button
        const int buttonWidth = 220; // Slightly wider button
        const int buttonHeight = 60; // Slightly taller button
        const int buttonX = screenWidth/2 - buttonWidth/2;
        const int buttonY = screenHeight/2 + 30; // Positioned below the text

        Vector2 mousePoint = GetMousePosition();
        bool mouseOverButton = CheckCollisionPointRec(mousePoint, (Rectangle){ (float)buttonX, (float)buttonY, (float)buttonWidth, (float)buttonHeight });

        // Draw button with hover effect
        DrawRectangle(buttonX, buttonY, buttonWidth, buttonHeight, mouseOverButton ? DARKBLUE : BLUE);
        DrawRectangleLines(buttonX, buttonY, buttonWidth, buttonHeight, WHITE); // Add a border

        const char* buttonText = "START GAME";
        int buttonFontSize = 24; // Larger button text
        int buttonTextWidth = MeasureText(buttonText, buttonFontSize);
        DrawText(buttonText, buttonX + buttonWidth/2 - buttonTextWidth/2, buttonY + buttonHeight/2 - buttonFontSize/2, buttonFontSize, WHITE);

        // Handle button click
        if (mouseOverButton && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            TraceLog(LOG_INFO, "START GAME button clicked - making human inactive.");
            h->active = false;
            h->state = HUMAN_STATE_INACTIVE;
        }

        // Handle Enter/Space key press to start game
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            TraceLog(LOG_INFO, "Enter/Space pressed - making human inactive.");
            h->active = false;
            h->state = HUMAN_STATE_INACTIVE;
        }
        
        // Instruction for keyboard start
        const char* instructionText = "Press ENTER or SPACE to start";
        int instructionFontSize = 18;
        int instructionTextWidth = MeasureText(instructionText, instructionFontSize);
        DrawText(instructionText, screenWidth/2 - instructionTextWidth/2, buttonY + buttonHeight + 20, instructionFontSize, GRAY);

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
    rlSetClipPlanes(1.0, 1500.0); // Adjust near/far clip planes for better depth precision

    // Define the camera
    Camera camera = {0};
    camera.position = (Vector3){0.0f, HUMAN_HEIGHT, 0.0f}; // Set camera at human height
    camera.target = (Vector3){0.0f, HUMAN_HEIGHT, 1.0f};   // Look forward (will be updated)
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int cameraMode = CAMERA_FIRST_PERSON; // Default to first person

    // Update the texture loading part of your code
    SearchAndSetResourceDir("resources");
    // Change the filename to match your downloaded texture
    Texture2D terrainTexture = LoadTexture("textures/rocky_terrain_02_diff_8k.jpg");

    // Load road texture
    roadTexture = LoadTexture("textures/rocky_trail_diff_8k.jpg"); // Initialize global roadTexture
    if (roadTexture.id == 0) { // Check if road texture failed to load
        TraceLog(LOG_ERROR, "Failed to load road texture: textures/rocky_trail_diff_8k.jpg");
    }

    // Optimize texture quality for terrain
    SetTextureFilter(terrainTexture, TEXTURE_FILTER_ANISOTROPIC_16X);
    SetTextureWrap(terrainTexture, TEXTURE_WRAP_REPEAT);

    // Optimize texture quality for road
    SetTextureFilter(roadTexture, TEXTURE_FILTER_ANISOTROPIC_16X);
    SetTextureWrap(roadTexture, TEXTURE_WRAP_REPEAT);

    // Initialize all terrain chunks at once with fixed layout
    InitAllTerrainChunks(terrainTexture);
    
    // Initialize the animals array
    for (int i = 0; i < MAX_ANIMALS; i++) {
        animals[i].active = false;
    }

    // Initialize the plants array
    for (int i = 0; i < MAX_PLANTS; i++) {
        plants[i].active = false;
    }

    // --- Load Global Plant Models ---
    // Ensure these paths are correct and models exist
    globalTreeModel = LoadModel("plants/tree.glb");
    if (globalTreeModel.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load tree.glb");

    globalGrassModel = LoadModel("plants/grass.glb");
    if (globalGrassModel.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load grass.glb");

    globalFlowerModel = LoadModel("plants/flower.glb");
    if (globalFlowerModel.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load flower.glb");

    globalFlowerModel_type2 = LoadModel("plants/flower2.glb");
    if (globalFlowerModel_type2.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load flower2.glb");

    globalBushWithFlowersModel = LoadModel("plants/bushWithFlowers.glb"); // Load new bush model
    if (globalBushWithFlowersModel.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load bushWithFlowers.glb");
    // --- End Load Global Plant Models ---

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
    if (buildings[0].model.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load buildings/barn.glb");
    buildings[0].position = (Vector3){ -10.0f, 0.0f, -10.0f };
    buildings[0].scale = 0.05f;
    buildings[0].rotationAngle = 45.0f;

    buildings[1].model = LoadModel("buildings/horse_barn.glb");
    if (buildings[1].model.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load buildings/horse_barn.glb");
    buildings[1].position = (Vector3){ 10.0f, 0.0f, 10.0f };
    buildings[1].scale = 0.75f;
    buildings[1].rotationAngle = -42.0f;

    // Load Bank model
    buildings[2].model = LoadModel("buildings/Bank.glb");
    if (buildings[2].model.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load buildings/Bank.glb");
    buildings[2].position = (Vector3){ 20.0f, 0.0f, -46.0f };
    buildings[2].scale = 0.0002f; // Drastically reduced scale for testing
    buildings[2].rotationAngle = 250.0f;

    // Load Construction House model
    buildings[3].model = LoadModel("buildings/constructionHouse.glb");
    if (buildings[3].model.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load buildings/constructionHouse.glb");
    buildings[3].position = (Vector3){ -40.0f, 0.1f, 26.0f }; // Opposite direction of Bank from Barn
    buildings[3].scale = 3.8f; // Adjust scale as needed
    buildings[3].rotationAngle = 0.0f; // Adjust rotation as needed

    // Load FarmHouse model
    buildings[4].model = LoadModel("buildings/FarmHouse.glb");
    if (buildings[4].model.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load buildings/FarmHouse.glb");
    buildings[4].position = (Vector3){ -35.0f, 0.1f, 20.0f }; // Positioned near the constructionHouse
    buildings[4].scale = 0.5f; // Adjust scale as needed
    buildings[4].rotationAngle = 108.0f; // Adjust rotation as needed

    // Orient camera to look at the farmhouse initially
    Vector3 farmhouseLookAtPosition = buildings[4].position;
    farmhouseLookAtPosition.y = HUMAN_HEIGHT; // Keep camera target at human height
    camera.target = farmhouseLookAtPosition;

    // Initialize the human character
    InitHuman(&human);
    
    // Set up human path from farmhouse to barn - with proper offsets and debug messages
    Vector3 farmhousePosition = buildings[4].position;  // FarmHouse position
    Vector3 barnPosition = buildings[0].position;       // Barn position
    
    // Make sure farmhouse and barn positions are far enough apart
    float pathDistance = Vector3Distance(farmhousePosition, barnPosition);
    TraceLog(LOG_INFO, "Distance between farmhouse and barn: %.2f units", pathDistance);
    
    // Offset start position to be in front of farmhouse
    farmhousePosition.x += 3.0f; // Move 3 units out from the farmhouse
    farmhousePosition.y = 0.3f;  // Set consistent height above ground
    
    // Offset end position to be more to the right of the barn
    barnPosition.x += 15.0f;  // Increased from 2.0f to move further right
    barnPosition.z += 5.0f;   // Also shift a bit forward for better visibility
    barnPosition.y = 0.3f;
    
    // Reduce the path length by 60%
    Vector3 pathVector = Vector3Subtract(barnPosition, farmhousePosition);
    float originalDistance = Vector3Length(pathVector);
    
    // Calculate 40% of the original distance (reducing path by 60%)
    float newDistance = originalDistance * 0.4f;
    
    // Update barn position to be 40% of the original distance from farmhouse
    Vector3 direction = Vector3Normalize(pathVector);
    barnPosition = Vector3Add(farmhousePosition, Vector3Scale(direction, newDistance));
    
    TraceLog(LOG_INFO, "Path shortened from %.2f to %.2f units (40%% of original)", 
             originalDistance, newDistance);
    
    TraceLog(LOG_INFO, "About to setup human path from farmhouse (%.2f, %.2f, %.2f) to barn (%.2f, %.2f, %.2f)",
             farmhousePosition.x, farmhousePosition.y, farmhousePosition.z,
             barnPosition.x, barnPosition.y, barnPosition.z);
    
    // Set up the path for the human
    SetupHumanPath(&human, farmhousePosition, barnPosition);
    
    // Extra failsafe to ensure the human is active and has good speed
    human.active = true;
    human.state = HUMAN_STATE_WALKING;
    human.stateTimer = 0.0f; // Start timer at 0
    human.showDialog = false; // No dialog initially
    human.speed = 0.15f; // Reduced speed from 0.3f for slower movement
    
    // Make sure direction is properly set
    if (Vector3Length(human.direction) < 0.1f) {
        // If direction isn't set correctly, use a default direction
        human.direction = (Vector3){ 1.0f, 0.0f, 0.0f }; // Default direction: right
        human.rotationAngle = 90.0f; // Facing East
        TraceLog(LOG_WARNING, "Human direction not properly set, using default");
    }
    
    TraceLog(LOG_INFO, "Human setup complete - active=%d, state=%d, direction=(%.2f, %.2f, %.2f), speed=%.2f",
             human.active, human.state, human.direction.x, human.direction.y, human.direction.z, human.speed);

    // Create animal enclosure using fences
    const float FENCE_MODEL_SCALE_CONST = 0.2f;
    const float ENCLOSURE_WIDTH = ENCLOSURE_WIDTH_1;  // Use the defined constant
    const float ENCLOSURE_LENGTH = ENCLOSURE_LENGTH_1; // Use the defined constant
    const float FENCE_SPACING = 1.0f;    // Space between fence segments
    const Vector3 ENCLOSURE_CENTER = ENCLOSURE_CENTER_1; // Use the defined constant

    // Calculate starting position (top-left corner)
    Vector3 startPos = {
        ENCLOSURE_CENTER.x - (ENCLOSURE_WIDTH / 2.0f),
        0.0f,
        ENCLOSURE_CENTER.z - (ENCLOSURE_LENGTH / 2.0f)
    };

    int fenceIndex = 5; // Start after existing buildings

    // Load fence model
    Model fenceModel = LoadModel("buildings/Fence.glb");
    if (fenceModel.meshCount == 0) {
        TraceLog(LOG_ERROR, "Failed to load buildings/Fence.glb");
    }

    // Top side (left to right, moved even more to the right)
    for (int i = 0; i <= (int)ENCLOSURE_WIDTH + 1; i++) {
        if (fenceIndex >= MAX_BUILDINGS) break;
        buildings[fenceIndex].model = fenceModel;
        buildings[fenceIndex].position = (Vector3){
            startPos.x - (2 * FENCE_SPACING) + (i * FENCE_SPACING) + (2 * FENCE_SPACING),
            startPos.y,
            startPos.z - FENCE_SPACING
        };
        buildings[fenceIndex].scale = FENCE_MODEL_SCALE_CONST;
        buildings[fenceIndex].rotationAngle = 0.0f;
        fenceIndex++;
    }
    // Right side (top to bottom, moved further to the exterior)
    for (int i = 0; i <= (int)ENCLOSURE_LENGTH; i++) {
        if (fenceIndex >= MAX_BUILDINGS) break;
        buildings[fenceIndex].model = fenceModel;
        buildings[fenceIndex].position = (Vector3){
            startPos.x + ((ENCLOSURE_WIDTH + 1) * FENCE_SPACING) + FENCE_SPACING,
            startPos.y,
            startPos.z + (i * FENCE_SPACING)
        };
        buildings[fenceIndex].scale = FENCE_MODEL_SCALE_CONST;
        buildings[fenceIndex].rotationAngle = 90.0f;
        fenceIndex++;
    }
    // Bottom side (right to left, move lower, inwards, and further to the left)
    for (int i = 0; i <= (int)ENCLOSURE_WIDTH + 1; i++) {
        if (fenceIndex >= MAX_BUILDINGS) break;
        buildings[fenceIndex].model = fenceModel;
        buildings[fenceIndex].position = (Vector3){
            startPos.x + FENCE_SPACING + ((ENCLOSURE_WIDTH + 1) * FENCE_SPACING) - (i * FENCE_SPACING) - FENCE_SPACING,
            startPos.y,
            startPos.z + ((ENCLOSURE_LENGTH) * FENCE_SPACING) + FENCE_SPACING
        };
        buildings[fenceIndex].scale = FENCE_MODEL_SCALE_CONST;
        buildings[fenceIndex].rotationAngle = 180.0f;
        fenceIndex++;
    }
    // Left side (top to bottom, move one position to the inside)
    for (int i = 0; i <= (int)ENCLOSURE_LENGTH; i++) {
        if (fenceIndex >= MAX_BUILDINGS) break;
        buildings[fenceIndex].model = fenceModel;
        buildings[fenceIndex].position = (Vector3){
            startPos.x - FENCE_SPACING,
            startPos.y,
            startPos.z + (i * FENCE_SPACING)
        };
        buildings[fenceIndex].scale = FENCE_MODEL_SCALE_CONST;
        buildings[fenceIndex].rotationAngle = 270.0f;
        fenceIndex++;
    }

    // --- Second Animal Enclosure ---
    Vector3 startPos2 = {
        ENCLOSURE_CENTER_2.x - (ENCLOSURE_WIDTH_2 / 2.0f),
        0.0f,
        ENCLOSURE_CENTER_2.z - (ENCLOSURE_LENGTH_2 / 2.0f)
    };

    int fenceIndex2 = fenceIndex; // Continue after the first enclosure

    // Top side (left to right, moved even more to the right)
    for (int i = 0; i <= (int)ENCLOSURE_WIDTH_2 + 1; i++) {
        if (fenceIndex2 >= MAX_BUILDINGS) break;
        buildings[fenceIndex2].model = fenceModel;
        buildings[fenceIndex2].position = (Vector3){
            startPos2.x - (2 * FENCE_SPACING) + (i * FENCE_SPACING) + (2 * FENCE_SPACING),
            startPos2.y,
            startPos2.z - FENCE_SPACING
        };
        buildings[fenceIndex2].scale = FENCE_MODEL_SCALE_CONST;
        buildings[fenceIndex2].rotationAngle = 0.0f;
        fenceIndex2++;
    }

    // Right side (top to bottom, moved further to the exterior)
    for (int i = 0; i <= (int)ENCLOSURE_LENGTH_2; i++) {
        if (fenceIndex2 >= MAX_BUILDINGS) break;
        buildings[fenceIndex2].model = fenceModel;
        buildings[fenceIndex2].position = (Vector3){
            startPos2.x + ((ENCLOSURE_WIDTH_2 + 1) * FENCE_SPACING) + FENCE_SPACING,
            startPos2.y,
            startPos2.z + (i * FENCE_SPACING)
        };
        buildings[fenceIndex2].scale = FENCE_MODEL_SCALE_CONST;
        buildings[fenceIndex2].rotationAngle = 90.0f;
        fenceIndex2++;
    }

    // Bottom side (right to left, move lower, inwards, and further to the left)
    for (int i = 0; i <= (int)ENCLOSURE_WIDTH_2 + 1; i++) {
        if (fenceIndex2 >= MAX_BUILDINGS) break;
        buildings[fenceIndex2].model = fenceModel;
        buildings[fenceIndex2].position = (Vector3){
            startPos2.x + FENCE_SPACING + ((ENCLOSURE_WIDTH_2 + 1) * FENCE_SPACING) - (i * FENCE_SPACING) - FENCE_SPACING,
            startPos2.y,
            startPos2.z + ((ENCLOSURE_LENGTH_2) * FENCE_SPACING) + FENCE_SPACING
        };
        buildings[fenceIndex2].scale = FENCE_MODEL_SCALE_CONST;
        buildings[fenceIndex2].rotationAngle = 180.0f;
        fenceIndex2++;
    }

    // Left side (top to bottom, move one position to the inside)
    for (int i = 0; i <= (int)ENCLOSURE_LENGTH_2; i++) {
        if (fenceIndex2 >= MAX_BUILDINGS) break;
        buildings[fenceIndex2].model = fenceModel;
        buildings[fenceIndex2].position = (Vector3){
            startPos2.x - FENCE_SPACING,
            startPos2.y,
            startPos2.z + (i * FENCE_SPACING)
        };
        buildings[fenceIndex2].scale = FENCE_MODEL_SCALE_CONST;
        buildings[fenceIndex2].rotationAngle = 270.0f;
        fenceIndex2++;
    }

    fenceIndex = fenceIndex2; // Update main fenceIndex for any further buildings

    // Load and place ChickenCoop in the second enclosure
    buildings[fenceIndex].model = LoadModel("buildings/ChickenCoop.glb");
    if (buildings[fenceIndex].model.meshCount == 0) TraceLog(LOG_ERROR, "Failed to load buildings/ChickenCoop.glb");
    buildings[fenceIndex].position = (Vector3){
        ENCLOSURE_CENTER_2.x,  // Center of the enclosure
        0.0f,
        ENCLOSURE_CENTER_2.z   // Center of the enclosure
    };
    buildings[fenceIndex].scale = 1.0f;  // Increased scale to 1.0f
    buildings[fenceIndex].rotationAngle = 45.0f;  // Rotate 45 degrees for better placement
    fenceIndex++;  // Increment for any future buildings

    // Load nature scene model
    // Model natureSceneModel = LoadModel("scenes/nature&mountains.glb");
    // Vector3 natureScenePosition = { -25.0f, 0.0f, -25.0f }; // Further from the barn
    // float natureSceneScale = 1.0f;

    // // Load second nature scene model
    // Model natureSceneModel2 = LoadModel("scenes/nature&mountains.glb");
    // Vector3 natureScenePosition2 = { -25.0f, 0.0f, -50.0f }; // Near the first nature scene
    // float natureSceneScale2 = 1.0f;

    // --- Custom Road Initialization ---
    // Example: Creating a custom road from the farm entrance points
    if (totalCustomRoadsCount < MAX_CUSTOM_ROADS && Farm_Entrance_numPoints > 1) {
        CustomRoad* newRoad = &allCustomRoads[totalCustomRoadsCount];
        snprintf(newRoad->name, sizeof(newRoad->name), "%s", Farm_Entrance_name);
        newRoad->numPoints = Farm_Entrance_numPoints;
        // Use memcpy to copy the points array
        memcpy(newRoad->points, Farm_Entrance_points, sizeof(Vector3) * newRoad->numPoints);
        // Use individual segments to create the road path
        GenerateRoadSegments(newRoad, roadWidth, roadTexture);
        totalCustomRoadsCount++; // Increment the count of active roads
        TraceLog(LOG_INFO, "Created road '%s' with %d points", newRoad->name, newRoad->numPoints);
    }

    // Add the second road
    if (totalCustomRoadsCount < MAX_CUSTOM_ROADS && secondRoadNumPoints > 1) {
        CustomRoad* newRoad = &allCustomRoads[totalCustomRoadsCount];
        snprintf(newRoad->name, sizeof(newRoad->name), "%s", secondRoadName);
        newRoad->numPoints = secondRoadNumPoints;
        // Use memcpy to copy the points array
        memcpy(newRoad->points, secondRoadPoints, sizeof(Vector3) * newRoad->numPoints);
        // Use individual segments to create the road path
        GenerateRoadSegments(newRoad, roadWidth, roadTexture);
        totalCustomRoadsCount++; // Increment the count of active roads
        TraceLog(LOG_INFO, "Created road '%s' with %d points", newRoad->name, newRoad->numPoints);
    }

    if (totalCustomRoadsCount < MAX_CUSTOM_ROADS && thirdRoadNumPoints > 1) {
        CustomRoad* newRoad = &allCustomRoads[totalCustomRoadsCount];
        snprintf(newRoad->name, sizeof(newRoad->name), "%s", thirdRoadName);
        newRoad->numPoints = thirdRoadNumPoints;
        // Use memcpy to copy the points array
        memcpy(newRoad->points, thirdRoadPoints, sizeof(Vector3) * newRoad->numPoints);
        // Use individual segments to create the road path
        GenerateRoadSegments(newRoad, roadWidth, roadTexture);
        totalCustomRoadsCount++; // Increment the count of active roads
        TraceLog(LOG_INFO, "Created road '%s' with %d points", newRoad->name, newRoad->numPoints);
    }

    // Add the fourth road
    if (totalCustomRoadsCount < MAX_CUSTOM_ROADS && fourthRoadNumPoints > 1) {
        CustomRoad* newRoad = &allCustomRoads[totalCustomRoadsCount];
        snprintf(newRoad->name, sizeof(newRoad->name), "%s", fourthRoadName);
        newRoad->numPoints = fourthRoadNumPoints;
        // Use memcpy to copy the points array
        memcpy(newRoad->points, fourthRoadPoints, sizeof(Vector3) * newRoad->numPoints);
        // Use individual segments to create the road path
        GenerateRoadSegments(newRoad, roadWidth, roadTexture);
        totalCustomRoadsCount++; // Increment the count of active roads
        TraceLog(LOG_INFO, "Created road '%s' with %d points", newRoad->name, newRoad->numPoints);
    }

    // Add the fifth road
    if (totalCustomRoadsCount < MAX_CUSTOM_ROADS && fifthRoadNumPoints > 1) {
        CustomRoad* newRoad = &allCustomRoads[totalCustomRoadsCount];
        snprintf(newRoad->name, sizeof(newRoad->name), "%s", fifthRoadName);
        newRoad->numPoints = fifthRoadNumPoints;
        // Use memcpy to copy the points array
        memcpy(newRoad->points, fifthRoadPoints, sizeof(Vector3) * newRoad->numPoints);
        // Use individual segments to create the road path
        GenerateRoadSegments(newRoad, roadWidth, roadTexture);
        totalCustomRoadsCount++; // Increment the count of active roads
        TraceLog(LOG_INFO, "Created road '%s' with %d points", newRoad->name, newRoad->numPoints);
    }
    
    // Spawn plants (numbers increased)
    int numberOfTrees = NUMBER_OF_TREES; // Increased from 50
    for (int i = 0; i < numberOfTrees; i++) {
        Vector3 pos = GetRandomPlantPosition(FIXED_TERRAIN_SIZE);
        float scale = GetRandomValue(80, 150) / 100.0f; // Random scale between 0.8 and 1.5
        float rotation = GetRandomValue(0, 360);
        SpawnPlant(PLANT_TREE, pos, scale, rotation);
    }

    int numberOfGrassPatches = NUMBER_OF_GRASS; // Increased from 100
    for (int i = 0; i < numberOfGrassPatches; i++) {
        Vector3 pos = GetRandomPlantPosition(FIXED_TERRAIN_SIZE);
        float scale = GetRandomValue(50, 120) / 100.0f; // Random scale
        float rotation = GetRandomValue(0, 360);
        SpawnPlant(PLANT_GRASS, pos, scale, rotation);
    }

    int numberOfFlowers = NUMBER_OF_FLOWERS; // Increased from 80
    for (int i = 0; i < numberOfFlowers; i++) {
        Vector3 pos = GetRandomPlantPosition(FIXED_TERRAIN_SIZE);
        float scale = GetRandomValue(70, 130) / 100.0f; // Random scale
        float rotation = GetRandomValue(0, 360);
        SpawnPlant(PLANT_FLOWER, pos, scale, rotation);
    }

    int numberOfFlowerType2 = NUMBER_OF_FLOWER_TYPE2; // Added for new flower type
    for (int i = 0; i < numberOfFlowerType2; i++) {
        Vector3 pos = GetRandomPlantPosition(FIXED_TERRAIN_SIZE);
        float scale = 0.003f; // Set fixed scale to 0.005f
        float rotation = GetRandomValue(0, 360);
        SpawnPlant(PLANT_FLOWER_TYPE2, pos, scale, rotation);
    }

    int numberOfBushWithFlowers = NUMBER_OF_BUSH_WITH_FLOWERS;
    for (int i = 0; i < numberOfBushWithFlowers; i++) {
        Vector3 pos = GetRandomPlantPosition(FIXED_TERRAIN_SIZE);
        float scale = GetRandomValue(80, 120) / 100.0f; // Random scale 0.8 to 1.2
        float rotation = GetRandomValue(0, 360);
        SpawnPlant(PLANT_BUSH_WITH_FLOWERS, pos, scale, rotation);
    }

    // Clear any plants that might be blocking roads
    ClearPlantsNearRoads(3.0f);

    // Initialize the cloud system
    InitClouds(FIXED_TERRAIN_SIZE);

    // Load animal sounds
    LoadAnimalSounds();

    // Generate random columns for visualization
    float heights[MAX_COLUMNS] = {0};
    Vector3 positions[MAX_COLUMNS] = {0};
    Color colors[MAX_COLUMNS] = {0};

    for (int i = 0; i < MAX_COLUMNS; i++)
    {
        heights[i] = (float)GetRandomValue(1, 12);
        positions[i] = (Vector3){(float)GetRandomValue(-15, 15), heights[i] / 2.0f, (float)GetRandomValue(-15, 15)};
        colors[i] = (Color){GetRandomValue(20, 255), GetRandomValue(10,  55), 30, 255};
    }

    DisableCursor();
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        // Update camera
        UpdateCameraCustom(&camera, cameraMode);

        // Update and play animal sounds
        for (int i = 0; i < animalCount; i++) {
            if (animals[i].active) {
                PlayAnimalSound(&animals[i], camera);
            }
        }

        // --- Path Recording Logic ---
        if (IsKeyPressed(KEY_R)) {
            isRecordingPath = !isRecordingPath;
            if (isRecordingPath) {
                currentRecordingPointCount = 0;
                TraceLog(LOG_INFO, "Path recording started.");
            } else {
                TraceLog(LOG_INFO, "Path recording stopped. %d points recorded.", currentRecordingPointCount);
            }
        }

        // Spawn 5 chickens in the enclosure when K is pressed
        if (IsKeyPressed(KEY_K)) {
            SpawnChickensInEnclosure(5);
        }

        // Spawn 5 pigs in the enclosure when P is pressed
        if (IsKeyPressed(KEY_P)) {
            SpawnPigsInEnclosure(5);
        }

        // Toggle debug visualization when V is pressed
        if (IsKeyPressed(KEY_V)) {
            showDebugVisualization = !showDebugVisualization;
            TraceLog(LOG_INFO, "Debug visualization: %s", showDebugVisualization ? "ON" : "OFF");
        }
        
        // Reset human character position when H is pressed
        if (IsKeyPressed(KEY_H)) {
            // Reset human to starting position
            human.position = farmhousePosition;
            human.state = HUMAN_STATE_WALKING;
            human.stateTimer = 0.0f;
            human.active = true;
            
            // Re-setup path
            SetupHumanPath(&human, farmhousePosition, barnPosition);
            TraceLog(LOG_INFO, "Human character reset to starting position");
        }

        if (isRecordingPath && currentRecordingPointCount < MAX_PATH_POINTS) {
            if (currentRecordingPointCount == 0) {
                // Always record the first point
                currentRecordingBuffer[currentRecordingPointCount++] = camera.position;
                TraceLog(LOG_INFO, "Recorded point %d: (%.2f, %.2f, %.2f)", currentRecordingPointCount, camera.position.x, camera.position.y, camera.position.z);
            } else {
                // Record subsequent points if moved far enough
                float distSq = Vector3DistanceSqr(camera.position, currentRecordingBuffer[currentRecordingPointCount - 1]);
                if (distSq > minRecordDistanceSq) {
                    currentRecordingBuffer[currentRecordingPointCount++] = camera.position;
                    TraceLog(LOG_INFO, "Recorded point %d: (%.2f, %.2f, %.2f)", currentRecordingPointCount, camera.position.x, camera.position.y, camera.position.z);
                }
            }
        }

        if (IsKeyPressed(KEY_E)) {
            if (currentRecordingPointCount > 0) {
                printf("Recorded Path Coordinates (%d points):\\n", currentRecordingPointCount);
                printf("Vector3 recordedPathPoints[] = {\\n");
                for (int i = 0; i < currentRecordingPointCount; i++) {
                    printf("    { %.2ff, %.2ff, %.2ff }%s\\n", 
                           currentRecordingBuffer[i].x, 
                           currentRecordingBuffer[i].y, 
                           currentRecordingBuffer[i].z, 
                           (i == currentRecordingPointCount - 1) ? "" : ",");
                }
                printf("};\\n");
                printf("int recordedPathNumPoints = %d;\\n", currentRecordingPointCount);
                // Optionally, clear the buffer after exporting if desired
                // currentRecordingPointCount = 0; 
                // isRecordingPath = false; // Or allow further recording/appending
            } else {
                TraceLog(LOG_INFO, "No path points recorded to export.");
            }
        }
        // --- End Path Recording Logic ---

        // Update terrain based on player position
        UpdateTerrainChunks(camera.position, terrainTexture);

        // Update animals
        for (int i = 0; i < animalCount; i++) {
            if (animals[i].active) {
                UpdateAnimal(&animals[i], FIXED_TERRAIN_SIZE);
            }
        }
        
        // Update human character
        UpdateHuman(&human, GetFrameTime());

        BeginDrawing();
        ClearBackground(SKY_COLOR); // Use sky color as background

        BeginMode3D(camera);

        // Draw terrain chunks
        DrawTerrainChunks();
        
        // Draw the road
        DrawModelEx(roadModel, roadPosition, (Vector3){0.0f, 1.0f, 0.0f}, roadRotationAngle, (Vector3){1.0f, 1.0f, 1.0f}, WHITE);

        // Draw all custom roads
        DrawAllCustomRoads();

        // --- DEBUG: Draw Chicken Enclosure Boundaries ---
        /*BoundingBox enclosureBounds = {
            (Vector3){ ENCLOSURE_CENTER_2.x - ENCLOSURE_WIDTH_2/2.0f, -0.5f, ENCLOSURE_CENTER_2.z - ENCLOSURE_LENGTH_2/2.0f },
            (Vector3){ ENCLOSURE_CENTER_2.x + ENCLOSURE_WIDTH_2/2.0f, 0.5f, ENCLOSURE_CENTER_2.z + ENCLOSURE_LENGTH_2/2.0f }
        };
        DrawBoundingBox(enclosureBounds, LIME); // Draw a lime green box for the enclosure
        */
        // --- END DEBUG ---

        // --- DEBUG: Draw Pig Enclosure Boundaries ---
        /*BoundingBox pigEnclosureBounds = {
            (Vector3){ ENCLOSURE_CENTER_1.x - ENCLOSURE_WIDTH_1/2.0f, -0.5f, ENCLOSURE_CENTER_1.z - ENCLOSURE_LENGTH_1/2.0f },
            (Vector3){ ENCLOSURE_CENTER_1.x + ENCLOSURE_WIDTH_1/2.0f, 0.5f, ENCLOSURE_CENTER_1.z + ENCLOSURE_LENGTH_1/2.0f }
        };
        DrawBoundingBox(pigEnclosureBounds, RED); // Draw a red box for the pig enclosure
        */
        // --- END DEBUG ---

        // --- DEBUG: Draw Bank Safe Zone (collision-free area) ---
        if (showDebugVisualization) {
            BoundingBox bankSafeZone = {
                (Vector3){ buildings[2].position.x - 15.0f, -0.5f, buildings[2].position.z - 15.0f },
                (Vector3){ buildings[2].position.x + 15.0f, 0.5f, buildings[2].position.z + 15.0f }
            };
            DrawBoundingBox(bankSafeZone, BLUE); // Draw a blue box for the bank safe zone
            
            // Draw the road to bank as a series of spheres
            if (totalCustomRoadsCount >= 2) {
                CustomRoad* bankRoad = &allCustomRoads[1]; // Second road (to bank)
                for (int i = 0; i < bankRoad->numPoints; i++) {
                    DrawSphere(bankRoad->points[i], roadWidth * 0.75f, (Color){ 0, 0, 255, 128 }); // Semi-transparent blue spheres
                }
            }
        }
        // --- END DEBUG ---

        // Draw buildings
        for (int i = 0; i < MAX_BUILDINGS; i++)
        {
            if (buildings[i].model.meshCount > 0) { // Check if model is loaded
                DrawModelEx(buildings[i].model, buildings[i].position, (Vector3){0.0f, 1.0f, 0.0f}, buildings[i].rotationAngle, (Vector3){buildings[i].scale, buildings[i].scale, buildings[i].scale}, WHITE);
            }
        }

        // Draw the nature scene model
        // DrawModel(natureSceneModel, natureScenePosition, natureSceneScale, WHITE);
        // // Draw the second nature scene model
        // DrawModel(natureSceneModel2, natureScenePosition2, natureSceneScale2, WHITE);

        // Draw plants
        DrawPlants(camera); // Pass camera object

        // Draw animals
        DrawAnimals();
        
        // Draw human character
        if (human.active) { // Only draw the 3D model if active
            DrawHuman(&human, camera);
        }
        
        // Draw clouds
        DrawClouds(camera);

        EndMode3D();

        // Draw crosshair in the center of the screen
        int centerX = GetScreenWidth() / 2;
        int centerY = GetScreenHeight() / 2;
        int crosshairSize = 8; // Size of the crosshair arms
        DrawLine(centerX - crosshairSize, centerY, centerX + crosshairSize, centerY, WHITE); // Horizontal line
        DrawLine(centerX, centerY - crosshairSize, centerX, centerY + crosshairSize, WHITE); // Vertical line


        // Draw info boxes
        DrawRectangle(5, 5, 330, 100, Fade(SKYBLUE, 0.5f));
        DrawRectangleLines(5, 5, 330, 100, BLUE);

        DrawText("- T - Disable collision", 15, 15, 10, BLACK);
        DrawText("- R: Start/Stop Path Recording", 15, 30, 10, BLACK);
        DrawText("- E: Export Path to Console (after stopping recording)", 15, 45, 10, BLACK);
        DrawText("- V: Toggle Debug Visualization", 15, 60, 10, BLACK);
        
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
        DrawRectangle(600, 110, 195,  100, Fade(LIGHTGREEN, 0.5f));
        DrawRectangleLines(600, 110, 195, 100, GREEN);
        
        DrawText("Animal Count:", 610, 120, 10, BLACK);
        DrawText(TextFormat("Total: %d/%d", animalCount, MAX_ANIMALS), 610, 135, 10, BLACK);
        DrawText(TextFormat("Horses: %d", animalCountByType[ANIMAL_HORSE]), 610, 150, 10, BLACK);
        DrawText(TextFormat("Cats: %d", animalCountByType[ANIMAL_CAT]), 610, 165, 10, BLACK);
        DrawText(TextFormat("Dogs: %d", animalCountByType[ANIMAL_DOG]), 610, 180,  10, BLACK);
        DrawText(TextFormat("Others: %d", animalCountByType[ANIMAL_COW] + animalCountByType[ANIMAL_CHICKEN] + animalCountByType[ANIMAL_PIG]), 610, 195, 10, BLACK);

        // Draw the human start menu (full-screen dialog)
        DrawHumanStartMenu(&human);

        EndDrawing();
    }

    // De-Initialization
    UnloadTexture(terrainTexture);
    
    // Unload animal sounds
    UnloadAnimalSounds();
    
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
    for (int i = 0; i < totalCustomRoadsCount; i++) {
        if (allCustomRoads[i].segmentCount > 0) {
            for (int j = 0; j < allCustomRoads[i].segmentCount; j++) {
                if (allCustomRoads[i].segments[j].meshCount > 0) {
                    UnloadModel(allCustomRoads[i].segments[j]);
                }
            }
        }
    }
    UnloadTexture(roadTexture);
    UnloadModel(roadModel);

    // Unload building models
    for (int i = 0; i < MAX_BUILDINGS; i++)
    {
        UnloadModel(buildings[i].model);
    }
    // UnloadModel(natureSceneModel); // Unload the nature scene model
    // UnloadModel(natureSceneModel2); // Unload the second nature scene model

    // Unload plant resources
    UnloadPlantResources();

    // Unload animal resources
    UnloadAnimalResources();
    
    // Unload human character resources
    UnloadHumanResources(&human);

    CloseWindow();
    return 0;
}