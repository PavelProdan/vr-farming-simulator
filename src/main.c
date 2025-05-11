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
#define MAX_BUILDINGS 24  // For barn, horse barn, and 22 fence segments for the enclosure
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
} Animal;

// Global array of animals
Animal animals[MAX_ANIMALS];
int animalCount = 0;
int animalCountByType[ANIMAL_COUNT] = {0};

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
                exclusionRadiusForPlant = 12.0f; // Increased from 8.0f
            } else if (i == 3) { // constructionHouse.glb (index 3, scale 0.1f)
                exclusionRadiusForPlant = 9.0f;  // Exclusion radius for Construction House
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

        bool onRoad = IsPositionOnRoad(position, roadWidth); // roadWidth is global

        if (!collisionWithBuilding && !onRoad) {
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
                if (Vector3Distance(position, closestPointOnSegment) < (rWidth / 2.0f) + 1.0f) { // Increased buffer from 0.5f to 1.0f
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

// Global flag for collision detection
bool collisionDetectionEnabled = true; // Default is enabled

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
        
        // Skip collision checks if collision detection is disabled
        if (!collisionDetectionEnabled) {
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

// Function to check if an animal is colliding with a building
bool IsCollisionWithBuilding(Vector3 animalPosition, float animalRadius, int* buildingIndex) {
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (buildings[i].model.meshCount == 0) continue; // Skip uninitialized or unloaded buildings

        // Calculate distance between animal and building center
        float distance = Vector3Distance(animalPosition, buildings[i].position);
        
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
    rlSetClipPlanes(1.0, 1500.0); // Adjust near/far clip planes for better depth precision

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
    buildings[1].scale = 0.5f;
    buildings[1].rotationAngle = 135.0f;

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
        colors[i] = (Color){GetRandomValue(20, 255), GetRandomValue(10,  55), 30, 255};
    }

    DisableCursor();
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        // Update camera
        UpdateCameraCustom(&camera, cameraMode);

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

        BeginDrawing();
        ClearBackground(SKY_COLOR); // Use sky color as background

        BeginMode3D(camera);

        // Draw terrain chunks
        DrawTerrainChunks();
        
        // Draw the road
        DrawModelEx(roadModel, roadPosition, (Vector3){0.0f, 1.0f, 0.0f}, roadRotationAngle, (Vector3){1.0f, 1.0f, 1.0f}, WHITE);

        // Draw all custom roads
        DrawAllCustomRoads();

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

    CloseWindow();
    return 0;
}