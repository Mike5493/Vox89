#include <raylib.h>
#include "Noise.h"

#define CHUNK_SIZE_X	 16
#define CHUNK_SIZE_Y	384
#define CHUNK_SIZE_Z	 16
#define BLOCK_SIZE	   1.0f

#define SEA_LEVEL		 63
#define BEDROCK_LEVEL	  4

#define RENDER_DISTANCE	  1
#define MAX_CHUNKS		256

typedef struct {
	int		type;				/* 0=Air, 1=Grass, 2=Dirt, 3=Stone */
} Block;

typedef struct {
	Block	blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
	Mesh	mesh;
	int		heightmap[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	int		meshValid;
	int		active;
	int		chunkX, chunkZ;
} Chunk;

typedef struct {
	char* memory;
	unsigned int size;
	unsigned int used;
} Arena;

typedef struct {
	Chunk** chunks;
	Arena	arena;
	int		count;
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

//Block chunk[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];

void GenerateChunk(Chunk* chunk, int chunkX, int chunkZ) {
	int x, y, z;
	double		scale = 0.02;	/* Terrain scale */
	double		persistence = 0.5;
	double		lacunarity = 2.0;
	int			octaves = 4;

	chunk->chunkX = chunkX;
	chunk->chunkZ = chunkZ;
	chunk->active = 1;
	chunk->meshValid = 0;	/* Rebuild mesh */

	for (x = 0; x < CHUNK_SIZE_X; x++) {
		for (z = 0; z < CHUNK_SIZE_Z; z++) {
			double amplitude = 1.0;
			double frequency = 1.0;
			double noise_height = 0.0;
			int i;

			/* Offset noise by chunk position */
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
			chunk->heightmap[x][z] = height;
		}
	}
	
	for (x = 0; x < CHUNK_SIZE_X; x++) {
		for (z = 0; z < CHUNK_SIZE_Z; z++) {
			int height = chunk->heightmap[x][z];
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

void BuildChunkMesh(Chunk* chunk) {
	int x, y, z, i;
	int		vertexCount = 0;
	int		maxVertices = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z * 24;
	float* vertices = (float*)malloc(maxVertices * 3 * sizeof(float));
	float* normals = (float*)malloc(maxVertices * 3 * sizeof(float));
	unsigned char* colors = (unsigned char*)malloc(maxVertices * 4 * sizeof(unsigned char));
	int* indices = (int*)malloc(maxVertices * 6 * sizeof(int));
	int indexCount = 0;

	for (x = 0; x < CHUNK_SIZE_X; x++) {
		for (y = 0; y < CHUNK_SIZE_Y; y++) {
			for (z = 0; z < CHUNK_SIZE_Z; z++) {
				if (chunk->blocks[x][y][z].type != 0) {
					float px = (float)(chunk->chunkX * CHUNK_SIZE_X + x) * BLOCK_SIZE;
					float py = (float)y * BLOCK_SIZE;
					float pz = (float)(chunk->chunkZ * CHUNK_SIZE_Z + z) * BLOCK_SIZE;
					float s = BLOCK_SIZE / 2.0f;
					Color color;
					switch (chunk->blocks[x][y][z].type) {
						case 1: color = GREEN; break;
						case 2: color = BROWN; break;
						case 3: color = GRAY; break;
						case 4: color = DARKGRAY; break;
						default: color = WHITE;
					}

					int drawFaces = 0;
					if (!IsBlockSolid(chunk, x + 1, y, z)) drawFaces |= 1;
					if (!IsBlockSolid(chunk, x - 1, y, z)) drawFaces |= 2;
					if (!IsBlockSolid(chunk, x, y + 1, z)) drawFaces |= 4;
					if (!IsBlockSolid(chunk, x, y - 1, z)) drawFaces |= 8;
					if (!IsBlockSolid(chunk, x, y, z + 1)) drawFaces |= 16;
					if (!IsBlockSolid(chunk, x, y, z - 1)) drawFaces |= 32;

					if (drawFaces) {
						if (drawFaces & 1) { /* Right (+X) */
							vertices[vertexCount * 3 + 0] = px + s; vertices[vertexCount * 3 + 1] = py + s; vertices[vertexCount * 3 + 2] = pz + s;
							vertices[vertexCount * 3 + 3] = px + s; vertices[vertexCount * 3 + 4] = py + s; vertices[vertexCount * 3 + 5] = pz - s;
							vertices[vertexCount * 3 + 6] = px + s; vertices[vertexCount * 3 + 7] = py - s; vertices[vertexCount * 3 + 8] = pz - s;
							vertices[vertexCount * 3 + 9] = px + s; vertices[vertexCount * 3 + 10] = py - s; vertices[vertexCount * 3 + 11] = pz + s;
							for (i = 0; i < 4; i++) {
								normals[vertexCount * 3 + i * 3 + 0] = 1.0f;
								normals[vertexCount * 3 + i * 3 + 1] = 0.0f;
								normals[vertexCount * 3 + i * 3 + 2] = 0.0f;
								colors[vertexCount * 4 + i * 4 + 0] = color.r;
								colors[vertexCount * 4 + i * 4 + 1] = color.g;
								colors[vertexCount * 4 + i * 4 + 2] = color.b;
								colors[vertexCount * 4 + i * 4 + 3] = color.a;
							}
							indices[indexCount + 0] = vertexCount + 0; indices[indexCount + 1] = vertexCount + 1; indices[indexCount + 2] = vertexCount + 2;
							indices[indexCount + 3] = vertexCount + 0; indices[indexCount + 4] = vertexCount + 2; indices[indexCount + 5] = vertexCount + 3;
							vertexCount += 4;
							indexCount += 6;
						}
						if (drawFaces & 2) { /* Left (-X) */
							vertices[vertexCount * 3 + 0] = px - s; vertices[vertexCount * 3 + 1] = py - s; vertices[vertexCount * 3 + 2] = pz + s;
							vertices[vertexCount * 3 + 3] = px - s; vertices[vertexCount * 3 + 4] = py - s; vertices[vertexCount * 3 + 5] = pz - s;
							vertices[vertexCount * 3 + 6] = px - s; vertices[vertexCount * 3 + 7] = py + s; vertices[vertexCount * 3 + 8] = pz - s;
							vertices[vertexCount * 3 + 9] = px - s; vertices[vertexCount * 3 + 10] = py + s; vertices[vertexCount * 3 + 11] = pz + s;
							for (i = 0; i < 4; i++) {
								normals[vertexCount * 3 + i * 3 + 0] = -1.0f;
								normals[vertexCount * 3 + i * 3 + 1] = 0.0f;
								normals[vertexCount * 3 + i * 3 + 2] = 0.0f;
								colors[vertexCount * 4 + i * 4 + 0] = color.r;
								colors[vertexCount * 4 + i * 4 + 1] = color.g;
								colors[vertexCount * 4 + i * 4 + 2] = color.b;
								colors[vertexCount * 4 + i * 4 + 3] = color.a;
							}
							indices[indexCount + 0] = vertexCount + 0; indices[indexCount + 1] = vertexCount + 1; indices[indexCount + 2] = vertexCount + 2;
							indices[indexCount + 3] = vertexCount + 0; indices[indexCount + 4] = vertexCount + 2; indices[indexCount + 5] = vertexCount + 3;
							vertexCount += 4;
							indexCount += 6;
						}
						if (drawFaces & 4) { /* Top (+Y) */
							vertices[vertexCount * 3 + 0] = px + s; vertices[vertexCount * 3 + 1] = py + s; vertices[vertexCount * 3 + 2] = pz + s;
							vertices[vertexCount * 3 + 3] = px - s; vertices[vertexCount * 3 + 4] = py + s; vertices[vertexCount * 3 + 5] = pz + s;
							vertices[vertexCount * 3 + 6] = px - s; vertices[vertexCount * 3 + 7] = py + s; vertices[vertexCount * 3 + 8] = pz - s;
							vertices[vertexCount * 3 + 9] = px + s; vertices[vertexCount * 3 + 10] = py + s; vertices[vertexCount * 3 + 11] = pz - s;
							for (i = 0; i < 4; i++) {
								normals[vertexCount * 3 + i * 3 + 0] = 0.0f;
								normals[vertexCount * 3 + i * 3 + 1] = 1.0f;
								normals[vertexCount * 3 + i * 3 + 2] = 0.0f;
								colors[vertexCount * 4 + i * 4 + 0] = color.r;
								colors[vertexCount * 4 + i * 4 + 1] = color.g;
								colors[vertexCount * 4 + i * 4 + 2] = color.b;
								colors[vertexCount * 4 + i * 4 + 3] = color.a;
							}
							indices[indexCount + 0] = vertexCount + 0; indices[indexCount + 1] = vertexCount + 1; indices[indexCount + 2] = vertexCount + 2;
							indices[indexCount + 3] = vertexCount + 0; indices[indexCount + 4] = vertexCount + 2; indices[indexCount + 5] = vertexCount + 3;
							vertexCount += 4;
							indexCount += 6;
						}
						if (drawFaces & 8) { /* Bottom (-Y) */
							vertices[vertexCount * 3 + 0] = px + s; vertices[vertexCount * 3 + 1] = py - s; vertices[vertexCount * 3 + 2] = pz - s;
							vertices[vertexCount * 3 + 3] = px - s; vertices[vertexCount * 3 + 4] = py - s; vertices[vertexCount * 3 + 5] = pz - s;
							vertices[vertexCount * 3 + 6] = px - s; vertices[vertexCount * 3 + 7] = py - s; vertices[vertexCount * 3 + 8] = pz + s;
							vertices[vertexCount * 3 + 9] = px + s; vertices[vertexCount * 3 + 10] = py - s; vertices[vertexCount * 3 + 11] = pz + s;
							for (i = 0; i < 4; i++) {
								normals[vertexCount * 3 + i * 3 + 0] = 0.0f;
								normals[vertexCount * 3 + i * 3 + 1] = -1.0f;
								normals[vertexCount * 3 + i * 3 + 2] = 0.0f;
								colors[vertexCount * 4 + i * 4 + 0] = color.r;
								colors[vertexCount * 4 + i * 4 + 1] = color.g;
								colors[vertexCount * 4 + i * 4 + 2] = color.b;
								colors[vertexCount * 4 + i * 4 + 3] = color.a;
							}
							indices[indexCount + 0] = vertexCount + 0; indices[indexCount + 1] = vertexCount + 1; indices[indexCount + 2] = vertexCount + 2;
							indices[indexCount + 3] = vertexCount + 0; indices[indexCount + 4] = vertexCount + 2; indices[indexCount + 5] = vertexCount + 3;
							vertexCount += 4;
							indexCount += 6;
						}
						if (drawFaces & 16) { /* Front (+Z) */
							vertices[vertexCount * 3 + 0] = px + s; vertices[vertexCount * 3 + 1] = py + s; vertices[vertexCount * 3 + 2] = pz + s;
							vertices[vertexCount * 3 + 3] = px - s; vertices[vertexCount * 3 + 4] = py + s; vertices[vertexCount * 3 + 5] = pz + s;
							vertices[vertexCount * 3 + 6] = px - s; vertices[vertexCount * 3 + 7] = py - s; vertices[vertexCount * 3 + 8] = pz + s;
							vertices[vertexCount * 3 + 9] = px + s; vertices[vertexCount * 3 + 10] = py - s; vertices[vertexCount * 3 + 11] = pz + s;
							for (i = 0; i < 4; i++) {
								normals[vertexCount * 3 + i * 3 + 0] = 0.0f;
								normals[vertexCount * 3 + i * 3 + 1] = 0.0f;
								normals[vertexCount * 3 + i * 3 + 2] = 1.0f;
								colors[vertexCount * 4 + i * 4 + 0] = color.r;
								colors[vertexCount * 4 + i * 4 + 1] = color.g;
								colors[vertexCount * 4 + i * 4 + 2] = color.b;
								colors[vertexCount * 4 + i * 4 + 3] = color.a;
							}
							indices[indexCount + 0] = vertexCount + 0; indices[indexCount + 1] = vertexCount + 1; indices[indexCount + 2] = vertexCount + 2;
							indices[indexCount + 3] = vertexCount + 0; indices[indexCount + 4] = vertexCount + 2; indices[indexCount + 5] = vertexCount + 3;
							vertexCount += 4;
							indexCount += 6;
						}
						if (drawFaces & 32) { /* Back (-Z) */
							vertices[vertexCount * 3 + 0] = px + s; vertices[vertexCount * 3 + 1] = py - s; vertices[vertexCount * 3 + 2] = pz - s;
							vertices[vertexCount * 3 + 3] = px - s; vertices[vertexCount * 3 + 4] = py - s; vertices[vertexCount * 3 + 5] = pz - s;
							vertices[vertexCount * 3 + 6] = px - s; vertices[vertexCount * 3 + 7] = py + s; vertices[vertexCount * 3 + 8] = pz - s;
							vertices[vertexCount * 3 + 9] = px + s; vertices[vertexCount * 3 + 10] = py + s; vertices[vertexCount * 3 + 11] = pz - s;
							for (i = 0; i < 4; i++) {
								normals[vertexCount * 3 + i * 3 + 0] = 0.0f;
								normals[vertexCount * 3 + i * 3 + 1] = 0.0f;
								normals[vertexCount * 3 + i * 3 + 2] = -1.0f;
								colors[vertexCount * 4 + i * 4 + 0] = color.r;
								colors[vertexCount * 4 + i * 4 + 1] = color.g;
								colors[vertexCount * 4 + i * 4 + 2] = color.b;
								colors[vertexCount * 4 + i * 4 + 3] = color.a;
							}
							indices[indexCount + 0] = vertexCount + 0; indices[indexCount + 1] = vertexCount + 1; indices[indexCount + 2] = vertexCount + 2;
							indices[indexCount + 3] = vertexCount + 0; indices[indexCount + 4] = vertexCount + 2; indices[indexCount + 5] = vertexCount + 3;
							vertexCount += 4;
							indexCount += 6;
						}
					}
				}
			}
		}
	}

	if (chunk->mesh.vertices) {
		UnloadMesh(chunk->mesh);
	}

	chunk->mesh.vertexCount = vertexCount;
	chunk->mesh.triangleCount = indexCount / 3;
	chunk->mesh.vertices = (float*)malloc(vertexCount * 3 * sizeof(float));
	chunk->mesh.normals = (float*)malloc(vertexCount * 4 * sizeof(float));
	chunk->mesh.colors = (unsigned char*)malloc(vertexCount * 4 * sizeof(unsigned char));
	chunk->mesh.indices = (unsigned short*)malloc(indexCount * sizeof(unsigned short));

	for (i = 0; i < vertexCount * 3; i++) {
		chunk->mesh.vertices[i] = vertices[i];
		chunk->mesh.normals[i] = normals[i];
	}
	for (i = 0; i < vertexCount * 4; i++) {
		chunk->mesh.colors[i] = colors[i];
	}
	for (i = 0; i < indexCount; i++) {
		chunk->mesh.indices[i] = (unsigned short)indices[i];
	}

	free(vertices);
	free(normals);
	free(colors);
	free(indices);

	UploadMesh(&chunk->mesh, 0);
	chunk->meshValid = 1;
}

void DrawChunk(Chunk* chunk) {
	if (!chunk->meshValid) {
		BuildChunkMesh(chunk);
	}
	Material material = LoadMaterialDefault();
	Material identity = MatrixIdentity();
	DrawMesh(chunk->mesh, material);
}

void UpdateChunks(ChunkManager* manager, Vector3 playerPos) {
	int		playerChunkX = (int)(playerPos.x / (CHUNK_SIZE_X * BLOCK_SIZE));
	int		playerChunkZ = (int)(playerPos.z / (CHUNK_SIZE_Z * BLOCK_SIZE));
	int		i;

	/* Mark all chunks as inactive */
	for (i = 0; i < manager->count; i++) {
		manager->chunks[i]->active = 0;
	}

	/* Load or activate chunks in render distance */
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

	/* Initial chunk load */
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

	int i;
	for (i = 0; i < manager.count; i++) {
		if (manager.chunks[i]->mesh.vertices) {
			UnloadMesh(manager.chunks[i]->mesh);
		}
	}
	ArenaFree(&manager.arena);
	CloseWindow();
	return 0;
}