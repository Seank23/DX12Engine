#pragma once

static constexpr unsigned int FRAMES_IN_FLIGHT = 2;
static constexpr unsigned int CONSTANT_RING_BYTES_PER_FRAME = 4 * 1024 * 1024; // 4 MB
static constexpr unsigned int COMMAND_ALLOCATOR_POOL_SIZE = FRAMES_IN_FLIGHT * 32;

#define MAX_TEXTURE_SUBRESOURCE_COUNT 8
#define MAX_UPLOAD_BATCH_SIZE 64
#define SHADOW_MAP_SIZE 2048
#define MAX_CSM_CASCADES 8
