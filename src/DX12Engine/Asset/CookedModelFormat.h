#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace DX12Engine
{
    // Block 1 runtime cooked model container constants.
    constexpr std::uint32_t kCookedModelMagic = 0x444D5844u; // 'DXMD'
    constexpr std::uint16_t kCookedModelVersionMajor = 1;
    constexpr std::uint16_t kCookedModelVersionMinor = 1;
    constexpr std::uint8_t kCookedModelEndianLittle = 1;
    constexpr std::uint32_t kCookedModelAlignment = 64;

    static_assert(sizeof(std::uint32_t) == 4, "Cooked format requires 32-bit uints");
    static_assert(sizeof(std::uint64_t) == 8, "Cooked format requires 64-bit uints");

    enum class CookedModelChunkType : std::uint32_t
    {
        Vertices = 0,
        IndicesLOD = 1,
        Meshes = 2,
        Nodes = 3,
        Materials = 4,
        Strings = 5,
        Animations = 6
    };

    struct CookedModelChunkDesc
    {
        std::uint32_t Type = static_cast<std::uint32_t>(CookedModelChunkType::Vertices);
        std::uint32_t Flags = 0;
        std::uint64_t Offset = 0;
        std::uint64_t Size = 0;
        std::uint32_t ElementCount = 0;
        std::uint32_t Reserved = 0;
    };

    struct CookedModelHeader
    {
        std::uint32_t Magic = kCookedModelMagic;
        std::uint16_t VersionMajor = kCookedModelVersionMajor;
        std::uint16_t VersionMinor = kCookedModelVersionMinor;
        std::uint8_t Endianness = kCookedModelEndianLittle;
        std::uint8_t HeaderSize = static_cast<std::uint8_t>(sizeof(CookedModelHeader));
        std::uint16_t Reserved0 = 0;

        std::uint32_t Alignment = kCookedModelAlignment;
        std::uint32_t Reserved1 = 0;

        std::uint64_t FileSize = 0;
        std::uint64_t ChunkTableOffset = 0;
        std::uint32_t ChunkCount = 0;
        std::uint32_t RequiredChunkMask = 0;

        std::uint64_t SourceContentHash = 0;
        std::uint64_t PayloadChecksum = 0;
    };

    inline constexpr std::uint32_t MakeChunkMask(CookedModelChunkType type)
    {
        return 1u << static_cast<std::uint32_t>(type);
    }

    inline constexpr std::uint32_t GetDefaultRequiredChunkMask()
    {
        return MakeChunkMask(CookedModelChunkType::Vertices)
            | MakeChunkMask(CookedModelChunkType::IndicesLOD)
            | MakeChunkMask(CookedModelChunkType::Meshes)
            | MakeChunkMask(CookedModelChunkType::Nodes)
            | MakeChunkMask(CookedModelChunkType::Materials)
            | MakeChunkMask(CookedModelChunkType::Strings);
    }

    // Version migration policy:
    // 1) reject unknown major versions,
    // 2) accept same major with equal or lower minor,
    // 3) accept same major with higher minor only when no unknown required fields are present.
    inline bool IsCookedModelVersionSupported(
        std::uint16_t fileMajor,
        std::uint16_t fileMinor,
        bool hasUnknownRequiredFields)
    {
        if (fileMajor != kCookedModelVersionMajor)
        {
            return false;
        }

        if (fileMinor <= kCookedModelVersionMinor)
        {
            return true;
        }

        return !hasUnknownRequiredFields;
    }

    inline bool ValidateCookedModelLayout(
        const CookedModelHeader& header,
        const std::vector<CookedModelChunkDesc>& chunks,
        std::uint64_t fileSize,
        std::string* outError)
    {
        auto fail = [&](const char* msg) {
            if (outError)
            {
                *outError = msg;
            }
            return false;
        };

        if (header.Magic != kCookedModelMagic)
        {
            return fail("Invalid cooked model magic");
        }

        if (header.Endianness != kCookedModelEndianLittle)
        {
            return fail("Unsupported cooked model endianness");
        }

        if (header.Alignment != kCookedModelAlignment)
        {
            return fail("Unsupported cooked model alignment");
        }

        if (header.ChunkCount != chunks.size())
        {
            return fail("Chunk count does not match chunk table length");
        }

        if (header.FileSize > fileSize)
        {
            return fail("Header file size exceeds actual file size");
        }

        if (header.ChunkTableOffset >= fileSize)
        {
            return fail("Chunk table offset is out of bounds");
        }

        std::uint32_t discoveredMask = 0;
        for (const CookedModelChunkDesc& chunk : chunks)
        {
            if (chunk.Offset > fileSize)
            {
                return fail("Chunk offset is out of bounds");
            }

            if (chunk.Size > fileSize || chunk.Offset + chunk.Size > fileSize || chunk.Offset + chunk.Size < chunk.Offset)
            {
                return fail("Chunk range exceeds file size");
            }

            if ((chunk.Offset % header.Alignment) != 0)
            {
                return fail("Chunk offset does not meet alignment requirements");
            }

            if (chunk.Type > static_cast<std::uint32_t>(CookedModelChunkType::Animations))
            {
                return fail("Unknown cooked model chunk type");
            }

            discoveredMask |= (1u << chunk.Type);

            // Basic overflow guard for count-based traversal.
            if (chunk.ElementCount > 0 && chunk.Size > 0)
            {
                const std::uint64_t maxPossibleStride = chunk.Size / static_cast<std::uint64_t>(chunk.ElementCount);
                if (maxPossibleStride == 0)
                {
                    return fail("Chunk count overflows declared chunk size");
                }
            }
        }

        if ((discoveredMask & header.RequiredChunkMask) != header.RequiredChunkMask)
        {
            return fail("Required cooked model chunks are missing");
        }

        return true;
    }

    inline bool ValidateIndicesInRange(
        const std::vector<std::uint32_t>& indices,
        std::uint32_t vertexCount)
    {
        if (vertexCount == 0)
        {
            return indices.empty();
        }

        return std::all_of(indices.begin(), indices.end(), [vertexCount](std::uint32_t i) {
            return i < vertexCount;
        });
    }
}
