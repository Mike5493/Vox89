#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include "Noise.h"

#define CHUNK_SIZE_X    16
#define CHUNK_SIZE_Y    384
#define CHUNK_SIZE_Z    16
#define BLOCK_SIZE      1.0f

#define SEA_LEVEL       63
#define BEDROCK_LEVEL   4

#define RENDER_DISTANCE 1
#define MAX_CHUNKS      256

typedef struct {
    int     type; /* 0=Air, 1=Grass, 2=Dirt, 3=Stone, 4=Bedrock */
} Block;

typedef struct {
    Block   blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    int     chunkX, chunkZ;
    int     active;
} Chunk;

typedef struct {
    char* memory;
    unsigned int size;
    unsigned int used;
} Arena;

typedef struct {
    Chunk** chunks;
    int     count;
    Arena   arena;
} ChunkManager;

/* Arena Setup */
void ArenaInit(Arena* arena, unsigned int size) {
    arena->memory = (char*)malloc(size);
    arena->size = size;
    arena->used = 0;
}

void ArenaFree(Arena* arena) {
    if (arena->memory) free(arena->memory);
    arena->memory = NULL;
    arena->size = 0;
    arena->used = 0;
}

void* ArenaAlloc(Arena* arena, unsigned int size) {
    if (arena->used + size > arena->size) return NULL;
    void* ptr = arena->memory + arena->used;
    arena->used += size;
    return ptr;
}

void ArenaReset(Arena* arena) {
    arena->used = 0;
}

void GenerateChunk(Chunk* chunk, int chunkX, int chunkZ) {
    int         x, y, z;
    double      scale = 0.02;
    double      persistence = 0.5;
    double      lacunarity = 2.0;
    int         octaves = 4;

    chunk->chunkX = chunkX;
    chunk->chunkZ = chunkZ;
    chunk->active = 1;

    for (x = 0; x < CHUNK_SIZE_X; x++) {
        for (z = 0; z < CHUNK_SIZE_Z; z++) {
            double amplitude = 1.0;
            double frequency = 1.0;
            double noise_height = 0.0;
            int i;

            double worldX = (chunkX * CHUNK_SIZE_X + x);
            double worldZ = (chunkZ * CHUNK_SIZE_Z + z);

            for (i = 0; i < octaves; i++) {
                double sample_x = worldX * scale * frequency;
                double sample_z = worldZ * scale * frequency;
                double perlin_value = noise2(sample_x, sample_z) * 2.0 - 1.0;
                noise_height += perlin_value * amplitude;

                amplitude *= persistence;
                frequency *= lacunarity;
            }

            int height = (int)(SEA_LEVEL + noise_height * 20.0);
            if (height < BEDROCK_LEVEL + 1) height = BEDROCK_LEVEL + 1;
            if (height > CHUNK_SIZE_Y - 1) height = CHUNK_SIZE_Y - 1;

            for (y = 0; y < CHUNK_SIZE_Y; y++) {
                if (y > height) {
                    chunk->blocks[x][y][z].type = 0; /* Air */
                } else if (y == height) {
                    chunk->blocks[x][y][z].type = 1; /* Grass */
                } else if (y > height - 5) {
                    chunk->blocks[x][y][z].type = 2; /* Dirt */
                } else if (y <= BEDROCK_LEVEL) {
                    chunk->blocks[x][y][z].type = 4; /* Bedrock */
                } else {
                    chunk->blocks[x][y][z].type = 3; /* Stone */
                }
            }
        }
    }
}

int IsBlockSolid(Chunk* chunk, int x, int y, int z) {
    if (x < 0 || x >= CHUNK_SIZE_X || y < 0 || y >= CHUNK_SIZE_Y || z < 0 || z >= CHUNK_SIZE_Z)
        return 0;
    return chunk->blocks[x][y][z].type != 0;
}

void DrawChunk(Chunk* chunk) {
    int x, y, z;
    for (x = 0; x < CHUNK_SIZE_X; x++) {
        for (y = 0; y < CHUNK_SIZE_Y; y++) {
            for (z = 0; z < CHUNK_SIZE_Z; z++) {
                if (chunk->blocks[x][y][z].type != 0) {
                    Vector3 pos;
                    pos.x = (float)(chunk->chunkX * CHUNK_SIZE_X + x) * BLOCK_SIZE;
                    pos.y = (float)y * BLOCK_SIZE;
                    pos.z = (float)(chunk->chunkZ * CHUNK_SIZE_Z + z) * BLOCK_SIZE;
                    Color color;
                    switch (chunk->blocks[x][y][z].type) {
                        case 1: color = GREEN; break;
                        case 2: color = BROWN; break;
                        case 3: color = GRAY; break;
                        case 4: color = DARKGRAY; break;
                        default: color = WHITE;
                    }

                    int drawFaces = 0;
                    if (!IsBlockSolid(chunk, x + 1, y, z)) drawFaces |= 1; /* Right face */
                    if (!IsBlockSolid(chunk, x - 1, y, z)) drawFaces |= 2; /* Left face */
                    if (!IsBlockSolid(chunk, x, y + 1, z)) drawFaces |= 4; /* Top face */
                    if (!IsBlockSolid(chunk, x, y - 1, z)) drawFaces |= 8; /* Bottom face */
                    if (!IsBlockSolid(chunk, x, y, z + 1)) drawFaces |= 16; /* Front face */
                    if (!IsBlockSolid(chunk, x, y, z - 1)) drawFaces |= 32; /* Back face */

                    if (drawFaces) {
                        DrawCube(pos, BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, color);
                        DrawCubeWires(pos, BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, BLACK);
                    }
                }
            }
        }
    }
}

void UpdateChunks(ChunkManager* manager, Vector3 playerPos) {
    int     playerChunkX = (int)(playerPos.x / (CHUNK_SIZE_X * BLOCK_SIZE));
    int     playerChunkZ = (int)(playerPos.z / (CHUNK_SIZE_Z * BLOCK_SIZE));
    int     i;

    for (i = 0; i < manager->count; i++) {
        manager->chunks[i]->active = 0;
    }

    int cx, cz;
    for (cx = playerChunkX - RENDER_DISTANCE; cx <= playerChunkX + RENDER_DISTANCE; cx++) {
        for (cz = playerChunkZ - RENDER_DISTANCE; cz <= playerChunkZ + RENDER_DISTANCE; cz++) {
            int found = 0;
            for (i = 0; i < manager->count; i++) {
                if (manager->chunks[i]->chunkX == cx && manager->chunks[i]->chunkZ == cz) {
                    manager->chunks[i]->active = 1;
                    found = 1;
                    break;
                }
            }
            if (!found && manager->count < MAX_CHUNKS) {
                Chunk* newChunk = (Chunk*)ArenaAlloc(&manager->arena, sizeof(Chunk));
                if (newChunk) {
                    GenerateChunk(newChunk, cx, cz);
                    manager->chunks[manager->count] = newChunk;
                    manager->count++;
                }
            }
        }
    }
}

int main(void) {
    InitWindow(1280, 720, "~ VANIR ~");
    SetTargetFPS(60);
    DisableCursor();

    Camera3D camera;
    camera.position = (Vector3){ 8.0f, SEA_LEVEL + 1.8f, 8.0f };
    camera.target = (Vector3){ 8.0f, SEA_LEVEL + 0.8f, 9.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;


    ChunkManager manager;
    manager.count = 0;
    ArenaInit(&manager.arena, MAX_CHUNKS * sizeof(Chunk));
    manager.chunks = (Chunk**)ArenaAlloc(&manager.arena, MAX_CHUNKS * sizeof(Chunk*));
    if (!manager.chunks) {
        CloseWindow();
        return 1;
    }

    UpdateChunks(&manager, camera.position);

    while (!WindowShouldClose()) {
        UpdateCamera(&camera, CAMERA_FREE);
        UpdateChunks(&manager, camera.position);

        BeginDrawing();
        ClearBackground(SKYBLUE);
        BeginMode3D(camera);
        int i;
        for (i = 0; i < manager.count; i++) {
            if (manager.chunks[i]->active) {
                DrawChunk(manager.chunks[i]);
            }
        }
        EndMode3D();
        DrawFPS(10, 10);
        EndDrawing();
    }

    ArenaFree(&manager.arena);
    CloseWindow();
    return 0;
}