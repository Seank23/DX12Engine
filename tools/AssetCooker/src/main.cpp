#include <DirectXTex.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <objbase.h>

#define NOMINMAX
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include "meshoptimizer.h"
#include <DX12Engine/Asset/CookedModelFormat.h>
#include <DX12Engine/Asset/Vertex.h>

namespace fs = std::filesystem;

namespace
{
    constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;
    constexpr const char* kCacheVersion = "asset-cooker-cache-v4";
    constexpr const char* kModelCompilerVersion = "dxmd-compiler-v2";
    constexpr std::array<float, 4> kCookLodRatios = { 1.0f, 0.5f, 0.25f, 0.125f };
    constexpr std::array<float, 4> kCookLodTargetErrors = { 0.0f, 0.01f, 0.02f, 0.04f };
    constexpr const char* kCookCompressionProfile = "meshopt-default";

    enum class TextureSemantic
    {
        Albedo,
        Normal,
        Roughness,
        Metallic,
        MetallicRoughness,
        AO,
        Emissive,
        Generic
    };

    struct CookSettings
    {
        TextureSemantic semantic = TextureSemantic::Generic;
        bool srgbHint = false;
    };

    struct Args
    {
        fs::path inputRoot;
        fs::path outputRoot;
        bool force = false;
        bool cleanCache = false;
    };

    struct CookStats
    {
        std::size_t scanned = 0;
        std::size_t cooked = 0;
        std::size_t skipped = 0;
        std::size_t failed = 0;
    };

    using CacheTable = std::unordered_map<std::string, std::uint64_t>;

    std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::string JsonEscape(const std::string& text)
    {
        std::string escaped;
        escaped.reserve(text.size() + 8);
        for (char c : text)
        {
            switch (c)
            {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
            }
        }
        return escaped;
    }

    std::string SanitizeName(std::string name)
    {
        if (name.empty())
        {
            return {};
        }

        for (char& c : name)
        {
            const bool isAllowed = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_';
            if (!isAllowed)
            {
                c = '_';
            }
        }

        while (!name.empty() && name.back() == '_')
        {
            name.pop_back();
        }

        return name;
    }

    const char* SemanticToString(TextureSemantic semantic)
    {
        switch (semantic)
        {
        case TextureSemantic::Albedo:
            return "albedo";
        case TextureSemantic::Normal:
            return "normal";
        case TextureSemantic::Roughness:
            return "roughness";
        case TextureSemantic::Metallic:
            return "metallic";
        case TextureSemantic::MetallicRoughness:
            return "metallicRoughness";
        case TextureSemantic::AO:
            return "ao";
        case TextureSemantic::Emissive:
            return "emissive";
        default:
            return "generic";
        }
    }

    bool IsSupportedTextureExtension(const fs::path& path)
    {
        const std::string ext = ToLower(path.extension().string());
        return ext == ".dds" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".hdr" || ext == ".tif" || ext == ".tiff";
    }

    bool IsSupportedGlbExtension(const fs::path& path)
    {
        return ToLower(path.extension().string()) == ".glb";
    }

    CookSettings InferCookSettings(const fs::path& sourcePath)
    {
        const std::string filename = ToLower(sourcePath.filename().string());
        CookSettings settings;

        if (filename.find("normal") != std::string::npos || filename.find("_n.") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Normal;
            settings.srgbHint = false;
        }
        else if (filename.find("roughness") != std::string::npos || filename.find("rough.") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Roughness;
            settings.srgbHint = false;
        }
        else if (filename.find("metallic") != std::string::npos || filename.find("metalness") != std::string::npos || filename.find("_m.") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Metallic;
            settings.srgbHint = false;
        }
        else if (filename.find("ao") != std::string::npos || filename.find("occlusion") != std::string::npos)
        {
            settings.semantic = TextureSemantic::AO;
            settings.srgbHint = false;
        }
        else if (filename.find("emissive") != std::string::npos || filename.find("emission") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Emissive;
            settings.srgbHint = true;
        }
        else if (filename.find("albedo") != std::string::npos || filename.find("basecolor") != std::string::npos || filename.find("diffuse") != std::string::npos || filename.find("color") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Albedo;
            settings.srgbHint = true;
        }
        else
        {
            settings.semantic = TextureSemantic::Generic;
            settings.srgbHint = false;
        }

        return settings;
    }

    std::uint64_t HashBytes(std::uint64_t hash, const std::uint8_t* bytes, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            hash ^= bytes[i];
            hash *= kFnvPrime;
        }
        return hash;
    }

    std::uint64_t HashText(std::uint64_t hash, const std::string& text)
    {
        return HashBytes(hash, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    }

    std::optional<std::uint64_t> HashFileWithSettings(const fs::path& sourcePath, const CookSettings& settings)
    {
        std::ifstream input(sourcePath, std::ios::binary);
        if (!input)
        {
            return std::nullopt;
        }

        std::uint64_t hash = kFnvOffsetBasis;
        std::vector<std::uint8_t> buffer(64 * 1024);
        while (input)
        {
            input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            const auto readCount = static_cast<std::size_t>(input.gcount());
            if (readCount > 0)
            {
                hash = HashBytes(hash, buffer.data(), readCount);
            }
        }

        hash = HashText(hash, kCacheVersion);
        hash = HashText(hash, SemanticToString(settings.semantic));
        hash = HashText(hash, settings.srgbHint ? "srgb=1" : "srgb=0");
        return hash;
    }

    std::optional<std::uint64_t> HashFileContents(const fs::path& sourcePath)
    {
        std::ifstream input(sourcePath, std::ios::binary);
        if (!input)
        {
            return std::nullopt;
        }

        std::uint64_t hash = kFnvOffsetBasis;
        std::vector<std::uint8_t> buffer(64 * 1024);
        while (input)
        {
            input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            const auto readCount = static_cast<std::size_t>(input.gcount());
            if (readCount > 0)
            {
                hash = HashBytes(hash, buffer.data(), readCount);
            }
        }

        return hash;
    }

    std::optional<std::uint64_t> HashGlbForCache(const fs::path& sourcePath)
    {
        const std::optional<std::uint64_t> sourceHash = HashFileContents(sourcePath);
        if (!sourceHash.has_value())
        {
            return std::nullopt;
        }

        std::uint64_t hash = *sourceHash;
        hash = HashText(hash, kCacheVersion);
        hash = HashText(hash, kModelCompilerVersion);
        hash = HashText(hash, kCookCompressionProfile);
        for (float ratio : kCookLodRatios)
        {
            hash = HashText(hash, std::to_string(ratio));
        }
        for (float error : kCookLodTargetErrors)
        {
            hash = HashText(hash, std::to_string(error));
        }

        return hash;
    }

    std::uint64_t GetFileSizeSafe(const fs::path& path)
    {
        std::error_code ec;
        const std::uint64_t size = static_cast<std::uint64_t>(fs::file_size(path, ec));
        return ec ? 0ull : size;
    }

    std::string FormatUtcTimestampIso8601()
    {
        const std::time_t now = std::time(nullptr);
        std::tm utcTime{};
        gmtime_s(&utcTime, &now);

        char buffer[32] = {};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime);
        return buffer;
    }

    std::string Hex64(std::uint64_t value)
    {
        std::ostringstream out;
        out << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
        return out.str();
    }

    bool ParseArgs(int argc, wchar_t* argv[], Args& outArgs)
    {
        if (argc <= 1)
        {
            return false;
        }

        for (int i = 1; i < argc; ++i)
        {
            const std::wstring arg = argv[i];
            if (arg == L"--in" && i + 1 < argc)
            {
                outArgs.inputRoot = argv[++i];
            }
            else if (arg == L"--out" && i + 1 < argc)
            {
                outArgs.outputRoot = argv[++i];
            }
            else if (arg == L"--force")
            {
                outArgs.force = true;
            }
            else if (arg == L"--clean-cache")
            {
                outArgs.cleanCache = true;
            }
            else if (arg == L"--help" || arg == L"-h")
            {
                return false;
            }
            else
            {
                std::wcerr << L"Unknown argument: " << arg << L"\n";
                return false;
            }
        }

        return !outArgs.inputRoot.empty() && !outArgs.outputRoot.empty();
    }

    void PrintUsage()
    {
        std::wcout << L"AssetCooker usage:\n"
                   << L"  AssetCooker --in <raw-assets-dir> --out <cooked-assets-dir> [--force] [--clean-cache]\n";
    }

    CacheTable LoadCache(const fs::path& cacheFile)
    {
        CacheTable cache;
        std::ifstream input(cacheFile);
        if (!input)
        {
            return cache;
        }

        std::string versionLine;
        std::getline(input, versionLine);
        if (versionLine != kCacheVersion)
        {
            return cache;
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty())
            {
                continue;
            }

            const auto delim = line.find('\t');
            if (delim == std::string::npos)
            {
                continue;
            }

            const std::string hashText = line.substr(0, delim);
            const std::string key = line.substr(delim + 1);
            try
            {
                const std::uint64_t hash = std::stoull(hashText, nullptr, 16);
                cache[key] = hash;
            }
            catch (...)
            {
                // Skip malformed cache rows.
            }
        }

        return cache;
    }

    bool SaveCache(const fs::path& cacheFile, const CacheTable& cache)
    {
        std::ofstream output(cacheFile, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << kCacheVersion << "\n";
        for (const auto& [key, hash] : cache)
        {
            output << std::hex << hash << std::dec << '\t' << key << "\n";
        }

        return true;
    }

    HRESULT LoadImageFile(const fs::path& sourcePath, DirectX::ScratchImage& image)
    {
        const std::string ext = ToLower(sourcePath.extension().string());
        if (ext == ".dds")
        {
            return DirectX::LoadFromDDSFile(sourcePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        }

        if (ext == ".tga")
        {
            return DirectX::LoadFromTGAFile(sourcePath.c_str(), nullptr, image);
        }

        if (ext == ".hdr")
        {
            return DirectX::LoadFromHDRFile(sourcePath.c_str(), nullptr, image);
        }

        return DirectX::LoadFromWICFile(sourcePath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
    }

    HRESULT LoadImageMemory(const std::vector<unsigned char>& encodedBytes, const std::string& mimeHint, DirectX::ScratchImage& image)
    {
        if (encodedBytes.empty())
        {
            return E_INVALIDARG;
        }

        const std::string mime = ToLower(mimeHint);
        if (mime.find("dds") != std::string::npos)
        {
            return DirectX::LoadFromDDSMemory(encodedBytes.data(), encodedBytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        }

        if (mime.find("tga") != std::string::npos)
        {
            return DirectX::LoadFromTGAMemory(encodedBytes.data(), encodedBytes.size(), nullptr, image);
        }

        if (mime.find("hdr") != std::string::npos)
        {
            return DirectX::LoadFromHDRMemory(encodedBytes.data(), encodedBytes.size(), nullptr, image);
        }

        return DirectX::LoadFromWICMemory(encodedBytes.data(), encodedBytes.size(), DirectX::WIC_FLAGS_NONE, nullptr, image);
    }

    bool WriteMetadataFile(const fs::path& metadataPath, const std::string& sourceTag, const CookSettings& settings, const DirectX::TexMetadata& metadata)
    {
        std::ofstream output(metadataPath, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << "{\n";
        output << "  \"source\": \"" << JsonEscape(sourceTag) << "\",\n";
        output << "  \"semantic\": \"" << SemanticToString(settings.semantic) << "\",\n";
        output << "  \"srgbHint\": " << (settings.srgbHint ? "true" : "false") << ",\n";
        output << "  \"width\": " << metadata.width << ",\n";
        output << "  \"height\": " << metadata.height << ",\n";
        output << "  \"depth\": " << metadata.depth << ",\n";
        output << "  \"arraySize\": " << metadata.arraySize << ",\n";
        output << "  \"mipLevels\": " << metadata.mipLevels << ",\n";
        output << "  \"isCubemap\": " << (metadata.IsCubemap() ? "true" : "false") << ",\n";
        output << "  \"dxgiFormat\": " << static_cast<int>(metadata.format) << "\n";
        output << "}\n";

        return true;
    }

    bool SaveCookedTexture(const DirectX::ScratchImage& sourceImage, const fs::path& outputDdsPath, const fs::path& metadataPath, const std::string& sourceTag, const CookSettings& settings)
    {
        const DirectX::TexMetadata sourceMetadata = sourceImage.GetMetadata();
        DirectX::ScratchImage generatedMipChain;
        const DirectX::ScratchImage* finalImage = &sourceImage;

        HRESULT hr = S_OK;
        if (sourceMetadata.mipLevels <= 1 && sourceMetadata.dimension != DirectX::TEX_DIMENSION_TEXTURE3D)
        {
            hr = DirectX::GenerateMipMaps(
                sourceImage.GetImages(),
                sourceImage.GetImageCount(),
                sourceMetadata,
                DirectX::TEX_FILTER_FANT,
                0,
                generatedMipChain);

            if (SUCCEEDED(hr) && generatedMipChain.GetImageCount() > 0)
            {
                finalImage = &generatedMipChain;
            }
        }

        hr = DirectX::SaveToDDSFile(
            finalImage->GetImages(),
            finalImage->GetImageCount(),
            finalImage->GetMetadata(),
            DirectX::DDS_FLAGS_NONE,
            outputDdsPath.c_str());

        if (FAILED(hr))
        {
            std::wcerr << L"[ERROR] Failed to save DDS: " << outputDdsPath << L"\n";
            return false;
        }

        if (!WriteMetadataFile(metadataPath, sourceTag, settings, finalImage->GetMetadata()))
        {
            std::wcerr << L"[ERROR] Failed to write metadata: " << metadataPath << L"\n";
            return false;
        }

        return true;
    }

    bool CookTextureFile(const fs::path& sourcePath, const fs::path& outputDdsPath, const fs::path& metadataPath, const fs::path& sourceRelativePath, const CookSettings& settings)
    {
        DirectX::ScratchImage sourceImage;
        const HRESULT hr = LoadImageFile(sourcePath, sourceImage);
        if (FAILED(hr))
        {
            std::wcerr << L"[ERROR] Failed to load: " << sourcePath << L"\n";
            return false;
        }

        return SaveCookedTexture(sourceImage, outputDdsPath, metadataPath, sourceRelativePath.generic_string(), settings);
    }

    void TryAssignSemantic(std::vector<TextureSemantic>& semantics, int textureIndex, TextureSemantic semantic)
    {
        if (textureIndex < 0 || static_cast<std::size_t>(textureIndex) >= semantics.size())
        {
            return;
        }

        if (semantics[textureIndex] == TextureSemantic::Generic)
        {
            semantics[textureIndex] = semantic;
        }
    }

    std::vector<TextureSemantic> InferGlbTextureSemantics(const tinygltf::Model& model)
    {
        std::vector<TextureSemantic> semantics(model.textures.size(), TextureSemantic::Generic);
        for (const tinygltf::Material& material : model.materials)
        {
            TryAssignSemantic(semantics, material.pbrMetallicRoughness.baseColorTexture.index, TextureSemantic::Albedo);
            TryAssignSemantic(semantics, material.normalTexture.index, TextureSemantic::Normal);
            TryAssignSemantic(semantics, material.occlusionTexture.index, TextureSemantic::AO);
            TryAssignSemantic(semantics, material.emissiveTexture.index, TextureSemantic::Emissive);
            TryAssignSemantic(semantics, material.pbrMetallicRoughness.metallicRoughnessTexture.index, TextureSemantic::MetallicRoughness);
        }
        return semantics;
    }

    std::optional<std::vector<unsigned char>> GetGlbEncodedImageBytes(const tinygltf::Model& model, const tinygltf::Image& image)
    {
        if (image.bufferView < 0 || static_cast<std::size_t>(image.bufferView) >= model.bufferViews.size())
        {
            return std::nullopt;
        }

        const tinygltf::BufferView& bufferView = model.bufferViews[image.bufferView];
        if (bufferView.buffer < 0 || static_cast<std::size_t>(bufferView.buffer) >= model.buffers.size())
        {
            return std::nullopt;
        }

        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const std::size_t begin = bufferView.byteOffset;
        const std::size_t end = begin + bufferView.byteLength;
        if (end > buffer.data.size() || begin >= end)
        {
            return std::nullopt;
        }

        return std::vector<unsigned char>(buffer.data.begin() + static_cast<std::ptrdiff_t>(begin), buffer.data.begin() + static_cast<std::ptrdiff_t>(end));
    }

    bool LoadGlbDecodedPixels(const tinygltf::Image& image, DirectX::ScratchImage& outScratch)
    {
        if (image.bits != 8 || image.width <= 0 || image.height <= 0 || image.component <= 0 || image.component > 4 || image.image.empty())
        {
            return false;
        }

        const std::size_t pixelCount = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
        const std::size_t expectedBytes = pixelCount * static_cast<std::size_t>(image.component);
        if (image.image.size() < expectedBytes)
        {
            return false;
        }

        std::vector<std::uint8_t> rgba(pixelCount * 4, 255);
        const std::uint8_t* src = image.image.data();
        for (std::size_t i = 0; i < pixelCount; ++i)
        {
            const std::size_t srcOffset = i * static_cast<std::size_t>(image.component);
            const std::size_t dstOffset = i * 4;
            const std::uint8_t r = src[srcOffset + 0];
            const std::uint8_t g = image.component > 1 ? src[srcOffset + 1] : r;
            const std::uint8_t b = image.component > 2 ? src[srcOffset + 2] : r;
            const std::uint8_t a = image.component > 3 ? src[srcOffset + 3] : 255;
            rgba[dstOffset + 0] = r;
            rgba[dstOffset + 1] = g;
            rgba[dstOffset + 2] = b;
            rgba[dstOffset + 3] = a;
        }

        HRESULT hr = outScratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<size_t>(image.width), static_cast<size_t>(image.height), 1, 1);
        if (FAILED(hr))
        {
            return false;
        }

        const DirectX::Image* dst = outScratch.GetImage(0, 0, 0);
        if (!dst || !dst->pixels)
        {
            return false;
        }

        const std::size_t srcRowPitch = static_cast<std::size_t>(image.width) * 4;
        for (int y = 0; y < image.height; ++y)
        {
            std::memcpy(dst->pixels + static_cast<std::size_t>(y) * dst->rowPitch, rgba.data() + static_cast<std::size_t>(y) * srcRowPitch, srcRowPitch);
        }

        return true;
    }

    bool LoadGlbImage(const tinygltf::Model& model, const tinygltf::Image& image, const fs::path& sourcePath, DirectX::ScratchImage& outScratch)
    {
        if (const auto encoded = GetGlbEncodedImageBytes(model, image))
        {
            const HRESULT hr = LoadImageMemory(*encoded, image.mimeType, outScratch);
            if (SUCCEEDED(hr))
            {
                return true;
            }
        }

        if (!image.uri.empty())
        {
            const fs::path uriPath = sourcePath.parent_path() / fs::path(image.uri);
            if (fs::exists(uriPath))
            {
                const HRESULT hr = LoadImageFile(uriPath, outScratch);
                if (SUCCEEDED(hr))
                {
                    return true;
                }
            }
        }

        return LoadGlbDecodedPixels(image, outScratch);
    }

    bool WriteGlbMaterialManifest(
        const fs::path& manifestPath,
        const fs::path& sourceRelativePath,
        const tinygltf::Model& model,
        const std::vector<TextureSemantic>& semantics,
        const std::vector<std::string>& textureOutputs)
    {
        std::ofstream output(manifestPath, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << "{\n";
        output << "  \"source\": \"" << JsonEscape(sourceRelativePath.generic_string()) << "\",\n";
        output << "  \"textures\": [\n";
        for (std::size_t i = 0; i < model.textures.size(); ++i)
        {
            const tinygltf::Texture& texture = model.textures[i];
            const std::string textureName = !texture.name.empty() ? texture.name : ("texture_" + std::to_string(i));
            output << "    {\n";
            output << "      \"index\": " << i << ",\n";
            output << "      \"name\": \"" << JsonEscape(textureName) << "\",\n";
            output << "      \"semantic\": \"" << SemanticToString(semantics[i]) << "\",\n";
            if (!textureOutputs[i].empty())
            {
                output << "      \"output\": \"" << JsonEscape(textureOutputs[i]) << "\"\n";
            }
            else
            {
                output << "      \"output\": null\n";
            }
            output << "    }" << (i + 1 == model.textures.size() ? "\n" : ",\n");
        }
        output << "  ],\n";
        output << "  \"materials\": [\n";

        for (std::size_t i = 0; i < model.materials.size(); ++i)
        {
            const tinygltf::Material& material = model.materials[i];
            auto writeTextureRef = [&](const char* key, int textureIndex, bool trailingComma)
            {
                output << "      \"" << key << "\": ";
                if (textureIndex >= 0 && static_cast<std::size_t>(textureIndex) < textureOutputs.size() && !textureOutputs[textureIndex].empty())
                {
                    output << "\"" << JsonEscape(textureOutputs[textureIndex]) << "\"";
                }
                else
                {
                    output << "null";
                }

                output << (trailingComma ? ",\n" : "\n");
            };

            output << "    {\n";
            output << "      \"index\": " << i << ",\n";
            output << "      \"name\": \"" << JsonEscape(material.name.empty() ? ("material_" + std::to_string(i)) : material.name) << "\",\n";
            writeTextureRef("baseColorTexture", material.pbrMetallicRoughness.baseColorTexture.index, true);
            writeTextureRef("normalTexture", material.normalTexture.index, true);
            writeTextureRef("metallicRoughnessTexture", material.pbrMetallicRoughness.metallicRoughnessTexture.index, true);
            writeTextureRef("occlusionTexture", material.occlusionTexture.index, true);
            writeTextureRef("emissiveTexture", material.emissiveTexture.index, false);
            output << "    }" << (i + 1 == model.materials.size() ? "\n" : ",\n");
        }

        output << "  ]\n";
        output << "}\n";

        return true;
    }

    std::string BuildGlbTextureTag(const fs::path& sourceRelativePath, std::size_t textureIndex)
    {
        std::ostringstream tag;
        tag << sourceRelativePath.generic_string() << "#texture/" << textureIndex;
        return tag.str();
    }

    fs::path BuildGlbTextureOutputRelativePath(const fs::path& sourceRelativePath, std::size_t textureIndex, const std::string& imageName)
    {
        fs::path modelRoot = sourceRelativePath;
        modelRoot.replace_extension();

        std::ostringstream filename;
        filename << "tex_" << std::setw(3) << std::setfill('0') << textureIndex;
        if (!imageName.empty())
        {
            filename << "_" << imageName;
        }
        filename << ".dds";

        return modelRoot / "Textures" / filename.str();
    }

    struct GlbLodLevelInfo
    {
        int level = 0;
        float ratio = 1.0f;
        float error = 0.0f;
        std::size_t indexCount = 0;
        DirectX::XMFLOAT3 min = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 max = { 0.0f, 0.0f, 0.0f };
        std::string output;
    };

    struct GlbPrimitiveLodInfo
    {
        std::size_t meshIndex = 0;
        std::size_t primitiveIndex = 0;
        std::size_t vertexCount = 0;
        std::size_t sourceIndexCount = 0;
        DirectX::XMFLOAT3 min = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 max = { 0.0f, 0.0f, 0.0f };
        bool skipped = false;
        std::string reason;
        std::vector<GlbLodLevelInfo> lods;
    };

    struct GlbManifestInfo
    {
        std::uint64_t sourceGlbHash = 0;
        std::string cookTimestampUtc;
        std::uint64_t chunkVerticesBytes = 0;
        std::uint64_t chunkIndicesLodBytes = 0;
        std::uint64_t chunkMeshesBytes = 0;
        std::uint64_t chunkNodesBytes = 0;
        std::uint64_t chunkMaterialsBytes = 0;
        std::uint64_t chunkStringsBytes = 0;
        std::uint64_t chunkAnimationsBytes = 0;
        std::uint64_t materialManifestBytes = 0;
        std::uint64_t lodManifestBytes = 0;
        std::uint64_t textureDdsBytes = 0;
        std::uint64_t lodIndexBytes = 0;
        std::vector<std::string> textureFiles;
    };

    struct Aabb
    {
        DirectX::XMFLOAT3 Min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        DirectX::XMFLOAT3 Max = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
    };

    struct CompiledLodData
    {
        std::uint32_t Level = 0;
        float Ratio = 1.0f;
        float Error = 0.0f;
        std::vector<std::uint32_t> Indices;
        Aabb Bounds;
    };

    struct CompiledPrimitiveData
    {
        std::uint32_t MeshIndex = 0;
        std::uint32_t PrimitiveIndex = 0;
        std::uint32_t MaterialIndex = 0;
        std::vector<DX12Engine::Vertex> Vertices;
        std::vector<CompiledLodData> Lods;
        Aabb Bounds;
    };

    struct CompiledMeshData
    {
        std::string Name;
        std::vector<std::uint32_t> PrimitiveGlobalIndices;
    };

    struct CompiledNodeData
    {
        std::string Name;
        std::int32_t ParentIndex = -1;
        std::int32_t MeshIndex = -1;
        std::array<float, 16> LocalTransform = { 1.0f, 0.0f, 0.0f, 0.0f,
                                                  0.0f, 1.0f, 0.0f, 0.0f,
                                                  0.0f, 0.0f, 1.0f, 0.0f,
                                                  0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct CompiledMaterialData
    {
        std::string Name;
        std::array<float, 4> BaseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float Metallic = 1.0f;
        float Roughness = 1.0f;
        std::array<float, 3> EmissiveFactor = { 0.0f, 0.0f, 0.0f };
        std::uint32_t AlphaMode = 0; // 0=OPAQUE,1=MASK,2=BLEND
        float AlphaCutoff = 0.5f;
        std::uint32_t DoubleSided = 0;
        std::array<std::uint32_t, 5> TextureRefIds = {
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max()
        };
    };

    struct CompiledAnimationSamplerData
    {
        std::uint32_t Interpolation = 0; // 0=Linear,1=Step,2=CubicSpline
        std::uint32_t Path = 0; // 0=Translation,1=Rotation,2=Scale
        std::vector<float> Times;
        std::vector<DirectX::XMFLOAT3> Translations;
        std::vector<DirectX::XMFLOAT4> Rotations;
        std::vector<DirectX::XMFLOAT3> Scales;
    };

    struct CompiledAnimationChannelData
    {
        std::uint32_t NodeIndex = 0;
        std::uint32_t Path = 0; // 0=Translation,1=Rotation,2=Scale
        std::uint32_t SamplerIndex = 0;
    };

    struct CompiledAnimationClipData
    {
        std::string Name;
        float Duration = 0.0f;
        std::vector<CompiledAnimationSamplerData> Samplers;
        std::vector<CompiledAnimationChannelData> Channels;
    };

    struct CompiledModelData
    {
        std::string ModelName;
        std::vector<CompiledPrimitiveData> Primitives;
        std::vector<CompiledMeshData> Meshes;
        std::vector<CompiledNodeData> Nodes;
        std::vector<CompiledMaterialData> Materials;
        std::vector<std::string> TextureRefPaths;
        std::vector<CompiledAnimationClipData> Animations;
    };

    struct ParsedGlbData
    {
        tinygltf::Model Model;
    };

    struct ModelCompilerWriteResult
    {
        std::uint64_t ChunkVerticesBytes = 0;
        std::uint64_t ChunkIndicesLodBytes = 0;
        std::uint64_t ChunkMeshesBytes = 0;
        std::uint64_t ChunkNodesBytes = 0;
        std::uint64_t ChunkMaterialsBytes = 0;
        std::uint64_t ChunkStringsBytes = 0;
        std::uint64_t ChunkAnimationsBytes = 0;
        std::uint64_t TotalVertexStreamBytes = 0;
        std::uint64_t TotalIndexStreamBytes = 0;
    };

    struct CookedVertexChunkHeader
    {
        std::uint32_t PrimitiveCount = 0;
        std::uint32_t VertexCount = 0;
        std::uint64_t PrimitiveTableOffset = 0;
        std::uint64_t VertexDataOffset = 0;
    };

    struct CookedVertexPrimitiveRecord
    {
        std::uint32_t PrimitiveIndex = 0;
        std::uint32_t VertexOffset = 0;
        std::uint32_t VertexCount = 0;
        std::uint32_t Reserved = 0;
    };

    struct CookedIndexChunkHeader
    {
        std::uint32_t LodRecordCount = 0;
        std::uint32_t TotalIndexCount = 0;
        std::uint64_t LodRecordOffset = 0;
        std::uint64_t IndexDataOffset = 0;
    };

    struct CookedIndexLodRecord
    {
        std::uint32_t PrimitiveIndex = 0;
        std::uint32_t LodLevel = 0;
        std::uint32_t IndexOffset = 0;
        std::uint32_t IndexCount = 0;
        float Ratio = 1.0f;
        float Error = 0.0f;
        DirectX::XMFLOAT3 Min = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Max = { 0.0f, 0.0f, 0.0f };
    };

    struct CookedMeshesChunkHeader
    {
        std::uint32_t MeshCount = 0;
        std::uint32_t PrimitiveCount = 0;
        std::uint64_t MeshTableOffset = 0;
        std::uint64_t PrimitiveTableOffset = 0;
    };

    struct CookedMeshRecord
    {
        std::uint32_t NameStringOffset = 0;
        std::uint32_t PrimitiveStart = 0;
        std::uint32_t PrimitiveCount = 0;
        std::uint32_t Reserved = 0;
    };

    struct CookedPrimitiveRecord
    {
        std::uint32_t MeshIndex = 0;
        std::uint32_t MaterialIndex = 0;
        std::uint32_t VertexOffset = 0;
        std::uint32_t VertexCount = 0;
        std::uint32_t LodStart = 0;
        std::uint32_t LodCount = 0;
        DirectX::XMFLOAT3 Min = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Max = { 0.0f, 0.0f, 0.0f };
    };

    struct CookedNodeRecord
    {
        std::uint32_t NameStringOffset = 0;
        std::int32_t ParentIndex = -1;
        std::int32_t MeshIndex = -1;
        std::uint32_t Reserved = 0;
        std::array<float, 16> LocalTransform = { 1.0f, 0.0f, 0.0f, 0.0f,
                                                  0.0f, 1.0f, 0.0f, 0.0f,
                                                  0.0f, 0.0f, 1.0f, 0.0f,
                                                  0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct CookedMaterialsChunkHeader
    {
        std::uint32_t MaterialCount = 0;
        std::uint32_t TextureRefCount = 0;
        std::uint64_t MaterialTableOffset = 0;
        std::uint64_t TextureRefTableOffset = 0;
    };

    struct CookedTextureRefRecord
    {
        std::uint32_t PathStringOffset = 0;
        std::uint32_t Reserved0 = 0;
        std::uint64_t Reserved1 = 0;
    };

    struct CookedMaterialRecord
    {
        std::uint32_t NameStringOffset = 0;
        std::uint32_t AlphaMode = 0;
        float AlphaCutoff = 0.5f;
        std::uint32_t DoubleSided = 0;
        std::array<float, 4> BaseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float Metallic = 1.0f;
        float Roughness = 1.0f;
        std::array<float, 3> EmissiveFactor = { 0.0f, 0.0f, 0.0f };
        std::uint32_t Padding = 0;
        std::array<std::uint32_t, 5> TextureRefIds = {
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max()
        };
    };

    struct CookedAnimationsChunkHeader
    {
        std::uint32_t ClipCount = 0;
        std::uint32_t SamplerCount = 0;
        std::uint32_t ChannelCount = 0;
        std::uint32_t TimeKeyCount = 0;
        std::uint32_t TranslationKeyCount = 0;
        std::uint32_t RotationKeyCount = 0;
        std::uint32_t ScaleKeyCount = 0;
        std::uint32_t Reserved = 0;
        std::uint64_t ClipTableOffset = 0;
        std::uint64_t SamplerTableOffset = 0;
        std::uint64_t ChannelTableOffset = 0;
        std::uint64_t TimeDataOffset = 0;
        std::uint64_t TranslationDataOffset = 0;
        std::uint64_t RotationDataOffset = 0;
        std::uint64_t ScaleDataOffset = 0;
    };

    struct CookedAnimationClipRecord
    {
        std::uint32_t NameStringOffset = 0;
        std::uint32_t SamplerStart = 0;
        std::uint32_t SamplerCount = 0;
        std::uint32_t ChannelStart = 0;
        std::uint32_t ChannelCount = 0;
        float Duration = 0.0f;
        std::uint32_t Reserved0 = 0;
        std::uint64_t Reserved1 = 0;
    };

    struct CookedAnimationSamplerRecord
    {
        std::uint32_t Interpolation = 0;
        std::uint32_t Path = 0;
        std::uint32_t KeyCount = 0;
        std::uint32_t Reserved0 = 0;
        std::uint32_t TimeOffset = 0;
        std::uint32_t ValueOffset = 0;
        std::uint64_t Reserved1 = 0;
    };

    struct CookedAnimationChannelRecord
    {
        std::uint32_t NodeIndex = 0;
        std::uint32_t Path = 0;
        std::uint32_t SamplerIndex = 0;
        std::uint32_t Reserved = 0;
    };

    const std::uint8_t* AccessorElementPointer(const tinygltf::Model& model, const tinygltf::Accessor& accessor, std::size_t index)
    {
        if (accessor.bufferView < 0 || static_cast<std::size_t>(accessor.bufferView) >= model.bufferViews.size())
        {
            return nullptr;
        }

        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        if (bufferView.buffer < 0 || static_cast<std::size_t>(bufferView.buffer) >= model.buffers.size())
        {
            return nullptr;
        }

        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        int stride = accessor.ByteStride(bufferView);
        if (stride <= 0)
        {
            stride = tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);
        }
        if (stride <= 0)
        {
            return nullptr;
        }

        const std::size_t byteOffset = static_cast<std::size_t>(bufferView.byteOffset) + static_cast<std::size_t>(accessor.byteOffset) + index * static_cast<std::size_t>(stride);
        if (byteOffset + static_cast<std::size_t>(stride) > buffer.data.size())
        {
            return nullptr;
        }

        return buffer.data.data() + byteOffset;
    }

    std::uint32_t ReadIndexScalar(const std::uint8_t* bytes, int componentType)
    {
        if (!bytes)
        {
            return 0;
        }

        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return *reinterpret_cast<const std::uint8_t*>(bytes);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return *reinterpret_cast<const std::uint16_t*>(bytes);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            return *reinterpret_cast<const std::uint32_t*>(bytes);
        default:
            return 0;
        }
    }

    float ReadScalarFloat(const std::uint8_t* bytes, int componentType, bool normalized)
    {
        if (!bytes)
        {
            return 0.0f;
        }

        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return *reinterpret_cast<const float*>(bytes);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return normalized ? static_cast<float>(*reinterpret_cast<const std::uint8_t*>(bytes)) / 255.0f : static_cast<float>(*reinterpret_cast<const std::uint8_t*>(bytes));
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return normalized ? static_cast<float>(*reinterpret_cast<const std::uint16_t*>(bytes)) / 65535.0f : static_cast<float>(*reinterpret_cast<const std::uint16_t*>(bytes));
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            return normalized ? std::max(-1.0f, static_cast<float>(*reinterpret_cast<const std::int8_t*>(bytes)) / 127.0f) : static_cast<float>(*reinterpret_cast<const std::int8_t*>(bytes));
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            return normalized ? std::max(-1.0f, static_cast<float>(*reinterpret_cast<const std::int16_t*>(bytes)) / 32767.0f) : static_cast<float>(*reinterpret_cast<const std::int16_t*>(bytes));
        default:
            return 0.0f;
        }
    }

    std::array<float, 3> ReadVec3(const std::uint8_t* bytes, int componentType, bool normalized)
    {
        if (!bytes)
        {
            return { 0.0f, 0.0f, 0.0f };
        }

        if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            const float* values = reinterpret_cast<const float*>(bytes);
            return { values[0], values[1], values[2] };
        }

        const int componentSize = tinygltf::GetComponentSizeInBytes(componentType);
        return {
            ReadScalarFloat(bytes, componentType, normalized),
            ReadScalarFloat(bytes + componentSize, componentType, normalized),
            ReadScalarFloat(bytes + componentSize * 2, componentType, normalized)
        };
    }

    std::array<float, 2> ReadVec2(const std::uint8_t* bytes, int componentType, bool normalized)
    {
        if (!bytes)
        {
            return { 0.0f, 0.0f };
        }

        if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            const float* values = reinterpret_cast<const float*>(bytes);
            return { values[0], values[1] };
        }

        const int componentSize = tinygltf::GetComponentSizeInBytes(componentType);
        return {
            ReadScalarFloat(bytes, componentType, normalized),
            ReadScalarFloat(bytes + componentSize, componentType, normalized)
        };
    }

    std::array<float, 4> ReadVec4(const std::uint8_t* bytes, int componentType, bool normalized)
    {
        if (!bytes)
        {
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }

        if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            const float* values = reinterpret_cast<const float*>(bytes);
            return { values[0], values[1], values[2], values[3] };
        }

        const int componentSize = tinygltf::GetComponentSizeInBytes(componentType);
        return {
            ReadScalarFloat(bytes, componentType, normalized),
            ReadScalarFloat(bytes + componentSize, componentType, normalized),
            ReadScalarFloat(bytes + componentSize * 2, componentType, normalized),
            ReadScalarFloat(bytes + componentSize * 3, componentType, normalized)
        };
    }

    void ExpandBounds(Aabb& bounds, const DirectX::XMFLOAT3& p)
    {
        bounds.Min.x = (std::min)(bounds.Min.x, p.x);
        bounds.Min.y = (std::min)(bounds.Min.y, p.y);
        bounds.Min.z = (std::min)(bounds.Min.z, p.z);
        bounds.Max.x = (std::max)(bounds.Max.x, p.x);
        bounds.Max.y = (std::max)(bounds.Max.y, p.y);
        bounds.Max.z = (std::max)(bounds.Max.z, p.z);
    }

    Aabb ComputeBoundsFromIndexedVertices(const std::vector<DX12Engine::Vertex>& vertices, const std::vector<std::uint32_t>& indices)
    {
        Aabb bounds;
        if (vertices.empty())
        {
            bounds.Min = { 0.0f, 0.0f, 0.0f };
            bounds.Max = { 0.0f, 0.0f, 0.0f };
            return bounds;
        }

        if (indices.empty())
        {
            for (const DX12Engine::Vertex& v : vertices)
            {
                ExpandBounds(bounds, v.Position);
            }
            return bounds;
        }

        for (std::uint32_t idx : indices)
        {
            if (idx < vertices.size())
            {
                ExpandBounds(bounds, vertices[idx].Position);
            }
        }
        return bounds;
    }

    void GenerateFlatNormals(std::vector<DX12Engine::Vertex>& vertices, const std::vector<std::uint32_t>& indices)
    {
        if (vertices.empty() || indices.size() < 3)
        {
            return;
        }

        for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const std::uint32_t i0 = indices[i + 0];
            const std::uint32_t i1 = indices[i + 1];
            const std::uint32_t i2 = indices[i + 2];
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            {
                continue;
            }

            DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&vertices[i0].Position);
            DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&vertices[i1].Position);
            DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&vertices[i2].Position);
            DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(
                DirectX::XMVector3Cross(
                    DirectX::XMVectorSubtract(p1, p0),
                    DirectX::XMVectorSubtract(p2, p0)));

            DirectX::XMFLOAT3 n;
            DirectX::XMStoreFloat3(&n, normal);
            vertices[i0].Normal = n;
            vertices[i1].Normal = n;
            vertices[i2].Normal = n;
        }
    }

    void ComputeTangents(std::vector<DX12Engine::Vertex>& vertices, const std::vector<std::uint32_t>& indices)
    {
        if (vertices.empty() || indices.size() < 3)
        {
            return;
        }

        std::vector<DirectX::XMFLOAT3> tan(vertices.size(), { 0.0f, 0.0f, 0.0f });

        for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const std::uint32_t i0 = indices[i + 0];
            const std::uint32_t i1 = indices[i + 1];
            const std::uint32_t i2 = indices[i + 2];
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            {
                continue;
            }

            const DirectX::XMFLOAT3& p0 = vertices[i0].Position;
            const DirectX::XMFLOAT3& p1 = vertices[i1].Position;
            const DirectX::XMFLOAT3& p2 = vertices[i2].Position;
            const DirectX::XMFLOAT2& uv0 = vertices[i0].TexCoord;
            const DirectX::XMFLOAT2& uv1 = vertices[i1].TexCoord;
            const DirectX::XMFLOAT2& uv2 = vertices[i2].TexCoord;

            const float x1 = p1.x - p0.x;
            const float y1 = p1.y - p0.y;
            const float z1 = p1.z - p0.z;
            const float x2 = p2.x - p0.x;
            const float y2 = p2.y - p0.y;
            const float z2 = p2.z - p0.z;

            const float s1 = uv1.x - uv0.x;
            const float t1 = uv1.y - uv0.y;
            const float s2 = uv2.x - uv0.x;
            const float t2 = uv2.y - uv0.y;

            const float denom = s1 * t2 - s2 * t1;
            if (std::abs(denom) <= 1e-8f)
            {
                continue;
            }

            const float r = 1.0f / denom;
            const DirectX::XMFLOAT3 sdir = {
                (t2 * x1 - t1 * x2) * r,
                (t2 * y1 - t1 * y2) * r,
                (t2 * z1 - t1 * z2) * r
            };

            tan[i0] = sdir;
            tan[i1] = sdir;
            tan[i2] = sdir;
        }

        for (std::size_t i = 0; i < vertices.size(); ++i)
        {
            const DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&vertices[i].Normal);
            const DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&tan[i]);
            const float ndott = DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, t));
            const DirectX::XMVECTOR ortho = DirectX::XMVectorSubtract(t, DirectX::XMVectorScale(n, ndott));

            DirectX::XMFLOAT3 tangent;
            DirectX::XMStoreFloat3(&tangent, DirectX::XMVector3Normalize(ortho));
            vertices[i].Tangent = { tangent.x, tangent.y, tangent.z, 1.0f };
        }
    }

    bool ParseGlbStage(const fs::path& sourcePath, ParsedGlbData& outParsed, std::string& outWarn, std::string& outErr)
    {
        tinygltf::TinyGLTF loader;
        outWarn.clear();
        outErr.clear();
        outParsed.Model = {};
        return loader.LoadBinaryFromFile(&outParsed.Model, &outErr, &outWarn, sourcePath.string());
    }

    std::uint32_t ToAlphaMode(const std::string& alphaMode)
    {
        if (alphaMode == "MASK")
        {
            return 1;
        }
        if (alphaMode == "BLEND")
        {
            return 2;
        }
        return 0;
    }

    std::uint32_t ToAnimationInterpolation(const std::string& interpolation)
    {
        if (interpolation == "STEP")
        {
            return 1;
        }
        if (interpolation == "CUBICSPLINE")
        {
            return 2;
        }
        return 0;
    }

    bool ExtractCompiledModelDataStage(
        const tinygltf::Model& model,
        const std::string& modelName,
        const std::vector<std::string>& textureOutputs,
        CompiledModelData& outData)
    {
        outData = {};
        outData.ModelName = modelName;

        std::unordered_map<std::string, std::uint32_t> textureRefMap;
        auto getTextureRefId = [&](int textureIndex) -> std::uint32_t
        {
            if (textureIndex < 0 || static_cast<std::size_t>(textureIndex) >= textureOutputs.size())
            {
                return std::numeric_limits<std::uint32_t>::max();
            }

            const std::string& path = textureOutputs[textureIndex];
            if (path.empty())
            {
                return std::numeric_limits<std::uint32_t>::max();
            }

            const auto it = textureRefMap.find(path);
            if (it != textureRefMap.end())
            {
                return it->second;
            }

            const std::uint32_t newId = static_cast<std::uint32_t>(outData.TextureRefPaths.size());
            outData.TextureRefPaths.push_back(path);
            textureRefMap.insert({ path, newId });
            return newId;
        };

        if (model.materials.empty())
        {
            CompiledMaterialData defaultMat;
            defaultMat.Name = "default_material";
            outData.Materials.push_back(std::move(defaultMat));
        }
        else
        {
            outData.Materials.reserve(model.materials.size());
            for (std::size_t mi = 0; mi < model.materials.size(); ++mi)
            {
                const tinygltf::Material& srcMat = model.materials[mi];
                CompiledMaterialData mat;
                mat.Name = srcMat.name.empty() ? ("material_" + std::to_string(mi)) : srcMat.name;
                mat.AlphaMode = ToAlphaMode(srcMat.alphaMode);
                mat.AlphaCutoff = static_cast<float>(srcMat.alphaCutoff);
                mat.DoubleSided = srcMat.doubleSided ? 1u : 0u;

                if (srcMat.pbrMetallicRoughness.baseColorFactor.size() == 4)
                {
                    mat.BaseColorFactor = {
                        static_cast<float>(srcMat.pbrMetallicRoughness.baseColorFactor[0]),
                        static_cast<float>(srcMat.pbrMetallicRoughness.baseColorFactor[1]),
                        static_cast<float>(srcMat.pbrMetallicRoughness.baseColorFactor[2]),
                        static_cast<float>(srcMat.pbrMetallicRoughness.baseColorFactor[3])
                    };
                }

                mat.Metallic = static_cast<float>(srcMat.pbrMetallicRoughness.metallicFactor);
                mat.Roughness = static_cast<float>(srcMat.pbrMetallicRoughness.roughnessFactor);
                if (srcMat.emissiveFactor.size() == 3)
                {
                    mat.EmissiveFactor = {
                        static_cast<float>(srcMat.emissiveFactor[0]),
                        static_cast<float>(srcMat.emissiveFactor[1]),
                        static_cast<float>(srcMat.emissiveFactor[2])
                    };
                }

                mat.TextureRefIds[0] = getTextureRefId(srcMat.pbrMetallicRoughness.baseColorTexture.index);
                mat.TextureRefIds[1] = getTextureRefId(srcMat.normalTexture.index);
                mat.TextureRefIds[2] = getTextureRefId(srcMat.pbrMetallicRoughness.metallicRoughnessTexture.index);
                mat.TextureRefIds[3] = getTextureRefId(srcMat.occlusionTexture.index);
                mat.TextureRefIds[4] = getTextureRefId(srcMat.emissiveTexture.index);

                outData.Materials.push_back(std::move(mat));
            }
        }

        outData.Meshes.reserve(model.meshes.size());
        for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
        {
            const tinygltf::Mesh& srcMesh = model.meshes[meshIndex];
            CompiledMeshData mesh;
            mesh.Name = srcMesh.name.empty() ? ("mesh_" + std::to_string(meshIndex)) : srcMesh.name;

            for (std::size_t primitiveIndex = 0; primitiveIndex < srcMesh.primitives.size(); ++primitiveIndex)
            {
                const tinygltf::Primitive& srcPrim = srcMesh.primitives[primitiveIndex];
                int mode = srcPrim.mode;
                if (mode < 0)
                {
                    mode = TINYGLTF_MODE_TRIANGLES;
                }
                if (mode != TINYGLTF_MODE_TRIANGLES)
                {
                    continue;
                }

                const auto posIt = srcPrim.attributes.find("POSITION");
                if (posIt == srcPrim.attributes.end() || posIt->second < 0 || static_cast<std::size_t>(posIt->second) >= model.accessors.size())
                {
                    continue;
                }

                const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
                if (tinygltf::GetNumComponentsInType(posAccessor.type) != 3 || posAccessor.count == 0)
                {
                    continue;
                }

                const tinygltf::Accessor* normAccessor = nullptr;
                const tinygltf::Accessor* uvAccessor = nullptr;
                const tinygltf::Accessor* tangentAccessor = nullptr;

                const auto normIt = srcPrim.attributes.find("NORMAL");
                if (normIt != srcPrim.attributes.end() && normIt->second >= 0 && static_cast<std::size_t>(normIt->second) < model.accessors.size())
                {
                    normAccessor = &model.accessors[normIt->second];
                }

                const auto uvIt = srcPrim.attributes.find("TEXCOORD_0");
                if (uvIt != srcPrim.attributes.end() && uvIt->second >= 0 && static_cast<std::size_t>(uvIt->second) < model.accessors.size())
                {
                    uvAccessor = &model.accessors[uvIt->second];
                }

                const auto tangentIt = srcPrim.attributes.find("TANGENT");
                if (tangentIt != srcPrim.attributes.end() && tangentIt->second >= 0 && static_cast<std::size_t>(tangentIt->second) < model.accessors.size())
                {
                    tangentAccessor = &model.accessors[tangentIt->second];
                }

                CompiledPrimitiveData prim;
                prim.MeshIndex = static_cast<std::uint32_t>(meshIndex);
                prim.PrimitiveIndex = static_cast<std::uint32_t>(primitiveIndex);
                prim.MaterialIndex = (srcPrim.material >= 0 && static_cast<std::size_t>(srcPrim.material) < outData.Materials.size())
                    ? static_cast<std::uint32_t>(srcPrim.material)
                    : 0u;

                prim.Vertices.resize(posAccessor.count);
                for (std::size_t vi = 0; vi < posAccessor.count; ++vi)
                {
                    const std::uint8_t* posBytes = AccessorElementPointer(model, posAccessor, vi);
                    if (!posBytes)
                    {
                        prim.Vertices.clear();
                        break;
                    }

                    const std::array<float, 3> p = ReadVec3(posBytes, posAccessor.componentType, posAccessor.normalized);
                    prim.Vertices[vi].Position = { p[0], p[1], -p[2] };
                    ExpandBounds(prim.Bounds, prim.Vertices[vi].Position);

                    prim.Vertices[vi].Normal = { 0.0f, 1.0f, 0.0f };
                    prim.Vertices[vi].TexCoord = { 0.0f, 0.0f };
                    prim.Vertices[vi].Tangent = { 1.0f, 0.0f, 0.0f, 1.0f };

                    if (normAccessor)
                    {
                        const std::uint8_t* nBytes = AccessorElementPointer(model, *normAccessor, vi);
                        const std::array<float, 3> n = ReadVec3(nBytes, normAccessor->componentType, normAccessor->normalized);
                        prim.Vertices[vi].Normal = { n[0], n[1], -n[2] };
                    }

                    if (uvAccessor)
                    {
                        const std::uint8_t* uvBytes = AccessorElementPointer(model, *uvAccessor, vi);
                        const std::array<float, 2> uv = ReadVec2(uvBytes, uvAccessor->componentType, uvAccessor->normalized);
                        prim.Vertices[vi].TexCoord = { uv[0], uv[1] };
                    }

                    if (tangentAccessor)
                    {
                        const std::uint8_t* tBytes = AccessorElementPointer(model, *tangentAccessor, vi);
                        const std::array<float, 4> t = ReadVec4(tBytes, tangentAccessor->componentType, tangentAccessor->normalized);
                        prim.Vertices[vi].Tangent = { t[0], t[1], -t[2], t[3] };
                    }
                }

                if (prim.Vertices.empty())
                {
                    continue;
                }

                std::vector<std::uint32_t> lod0Indices;
                if (srcPrim.indices >= 0)
                {
                    if (static_cast<std::size_t>(srcPrim.indices) >= model.accessors.size())
                    {
                        continue;
                    }

                    const tinygltf::Accessor& indexAccessor = model.accessors[srcPrim.indices];
                    lod0Indices.resize(indexAccessor.count);
                    for (std::size_t ii = 0; ii < indexAccessor.count; ++ii)
                    {
                        const std::uint8_t* idxBytes = AccessorElementPointer(model, indexAccessor, ii);
                        lod0Indices[ii] = ReadIndexScalar(idxBytes, indexAccessor.componentType);
                    }
                }
                else
                {
                    lod0Indices.resize(prim.Vertices.size());
                    for (std::size_t ii = 0; ii < prim.Vertices.size(); ++ii)
                    {
                        lod0Indices[ii] = static_cast<std::uint32_t>(ii);
                    }
                }

                if (lod0Indices.size() < 3 || (lod0Indices.size() % 3) != 0)
                {
                    continue;
                }

                for (std::size_t ii = 0; ii + 2 < lod0Indices.size(); ii += 3)
                {
                    std::swap(lod0Indices[ii + 1], lod0Indices[ii + 2]);
                }

                if (!normAccessor)
                {
                    GenerateFlatNormals(prim.Vertices, lod0Indices);
                }

                if (!tangentAccessor)
                {
                    ComputeTangents(prim.Vertices, lod0Indices);
                }

                CompiledLodData lod0;
                lod0.Level = 0;
                lod0.Ratio = 1.0f;
                lod0.Error = 0.0f;
                lod0.Indices = std::move(lod0Indices);
                lod0.Bounds = ComputeBoundsFromIndexedVertices(prim.Vertices, lod0.Indices);
                prim.Lods.push_back(std::move(lod0));
                prim.Bounds = prim.Lods[0].Bounds;

                const std::uint32_t globalPrimitiveIndex = static_cast<std::uint32_t>(outData.Primitives.size());
                outData.Primitives.push_back(std::move(prim));
                mesh.PrimitiveGlobalIndices.push_back(globalPrimitiveIndex);
            }

            outData.Meshes.push_back(std::move(mesh));
        }

        outData.Nodes.clear();
        std::vector<int> gltfToCompiledNode(model.nodes.size(), -1);
        if (!model.nodes.empty())
        {
            const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
            if (sceneIndex >= 0 && static_cast<std::size_t>(sceneIndex) < model.scenes.size())
            {
                std::function<void(int, int)> buildNode = [&](int gltfNodeIndex, int parentOutIndex)
                {
                    if (gltfNodeIndex < 0 || static_cast<std::size_t>(gltfNodeIndex) >= model.nodes.size())
                    {
                        return;
                    }

                    const tinygltf::Node& srcNode = model.nodes[gltfNodeIndex];
                    CompiledNodeData node;
                    node.Name = srcNode.name.empty() ? ("node_" + std::to_string(gltfNodeIndex)) : srcNode.name;
                    node.ParentIndex = parentOutIndex;
                    node.MeshIndex = srcNode.mesh;

                    DirectX::XMMATRIX local = DirectX::XMMatrixIdentity();
                    if (srcNode.matrix.size() == 16)
                    {
                        DirectX::XMMATRIX m = DirectX::XMMatrixSet(
                            static_cast<float>(srcNode.matrix[0]), static_cast<float>(srcNode.matrix[4]), static_cast<float>(srcNode.matrix[8]), static_cast<float>(srcNode.matrix[12]),
                            static_cast<float>(srcNode.matrix[1]), static_cast<float>(srcNode.matrix[5]), static_cast<float>(srcNode.matrix[9]), static_cast<float>(srcNode.matrix[13]),
                            static_cast<float>(srcNode.matrix[2]), static_cast<float>(srcNode.matrix[6]), static_cast<float>(srcNode.matrix[10]), static_cast<float>(srcNode.matrix[14]),
                            static_cast<float>(srcNode.matrix[3]), static_cast<float>(srcNode.matrix[7]), static_cast<float>(srcNode.matrix[11]), static_cast<float>(srcNode.matrix[15]));
                        const DirectX::XMMATRIX f = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);
                        local = f * m * f;
                    }
                    else
                    {
                        DirectX::XMMATRIX t = DirectX::XMMatrixIdentity();
                        DirectX::XMMATRIX r = DirectX::XMMatrixIdentity();
                        DirectX::XMMATRIX s = DirectX::XMMatrixIdentity();

                        if (srcNode.translation.size() == 3)
                        {
                            t = DirectX::XMMatrixTranslation(
                                static_cast<float>(srcNode.translation[0]),
                                static_cast<float>(srcNode.translation[1]),
                                -static_cast<float>(srcNode.translation[2]));
                        }

                        if (srcNode.rotation.size() == 4)
                        {
                            const DirectX::XMVECTOR q = DirectX::XMVectorSet(
                                -static_cast<float>(srcNode.rotation[0]),
                                -static_cast<float>(srcNode.rotation[1]),
                                static_cast<float>(srcNode.rotation[2]),
                                static_cast<float>(srcNode.rotation[3]));
                            r = DirectX::XMMatrixRotationQuaternion(q);
                        }

                        if (srcNode.scale.size() == 3)
                        {
                            s = DirectX::XMMatrixScaling(
                                static_cast<float>(srcNode.scale[0]),
                                static_cast<float>(srcNode.scale[1]),
                                static_cast<float>(srcNode.scale[2]));
                        }

                        local = s * r * t;
                    }

                    DirectX::XMFLOAT4X4 localF;
                    DirectX::XMStoreFloat4x4(&localF, local);
                    node.LocalTransform = {
                        localF._11, localF._12, localF._13, localF._14,
                        localF._21, localF._22, localF._23, localF._24,
                        localF._31, localF._32, localF._33, localF._34,
                        localF._41, localF._42, localF._43, localF._44
                    };

                    const int outNodeIndex = static_cast<int>(outData.Nodes.size());
                    outData.Nodes.push_back(std::move(node));
                    gltfToCompiledNode[gltfNodeIndex] = outNodeIndex;

                    for (int child : srcNode.children)
                    {
                        buildNode(child, outNodeIndex);
                    }
                };

                const tinygltf::Scene& scene = model.scenes[sceneIndex];
                for (int rootNode : scene.nodes)
                {
                    buildNode(rootNode, -1);
                }
            }
        }

        outData.Animations.clear();
        outData.Animations.reserve(model.animations.size());
        for (std::size_t animIndex = 0; animIndex < model.animations.size(); ++animIndex)
        {
            const tinygltf::Animation& srcAnim = model.animations[animIndex];
            CompiledAnimationClipData clip;
            clip.Name = srcAnim.name.empty() ? ("animation_" + std::to_string(animIndex)) : srcAnim.name;
            clip.Samplers.resize(srcAnim.samplers.size());

            std::vector<int> samplerPathHint(srcAnim.samplers.size(), -1); // 0=T,1=R,2=S
            for (const tinygltf::AnimationChannel& srcChannel : srcAnim.channels)
            {
                if (srcChannel.sampler < 0 || static_cast<std::size_t>(srcChannel.sampler) >= clip.Samplers.size())
                {
                    continue;
                }

                if (srcChannel.target_node < 0 || static_cast<std::size_t>(srcChannel.target_node) >= gltfToCompiledNode.size())
                {
                    continue;
                }

                const int nodeIndex = gltfToCompiledNode[srcChannel.target_node];
                if (nodeIndex < 0)
                {
                    continue;
                }

                std::uint32_t path = 0;
                if (srcChannel.target_path == "translation")
                {
                    path = 0;
                }
                else if (srcChannel.target_path == "rotation")
                {
                    path = 1;
                }
                else if (srcChannel.target_path == "scale")
                {
                    path = 2;
                }
                else
                {
                    continue;
                }

                int& samplerHint = samplerPathHint[srcChannel.sampler];
                if (samplerHint == -1)
                {
                    samplerHint = static_cast<int>(path);
                }
                else if (samplerHint != static_cast<int>(path))
                {
                    continue;
                }

                CompiledAnimationChannelData channel;
                channel.NodeIndex = static_cast<std::uint32_t>(nodeIndex);
                channel.Path = path;
                channel.SamplerIndex = static_cast<std::uint32_t>(srcChannel.sampler);
                clip.Channels.push_back(channel);
            }

            float duration = 0.0f;
            for (std::size_t samplerIndex = 0; samplerIndex < srcAnim.samplers.size(); ++samplerIndex)
            {
                const tinygltf::AnimationSampler& srcSampler = srcAnim.samplers[samplerIndex];
                CompiledAnimationSamplerData& sampler = clip.Samplers[samplerIndex];
                sampler.Interpolation = ToAnimationInterpolation(srcSampler.interpolation);

                if (srcSampler.input < 0 || static_cast<std::size_t>(srcSampler.input) >= model.accessors.size() ||
                    srcSampler.output < 0 || static_cast<std::size_t>(srcSampler.output) >= model.accessors.size())
                {
                    continue;
                }

                const tinygltf::Accessor& timeAccessor = model.accessors[srcSampler.input];
                const tinygltf::Accessor& valueAccessor = model.accessors[srcSampler.output];
                const std::size_t keyCount = timeAccessor.count;
                if (keyCount == 0)
                {
                    continue;
                }

                sampler.Times.resize(keyCount);
                for (std::size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                {
                    const std::uint8_t* timeBytes = AccessorElementPointer(model, timeAccessor, keyIndex);
                    const float t = ReadScalarFloat(timeBytes, timeAccessor.componentType, timeAccessor.normalized);
                    sampler.Times[keyIndex] = t;
                    duration = (std::max)(duration, t);
                }

                const bool cubicSpline = sampler.Interpolation == 2;
                const std::size_t expectedOutputCount = cubicSpline ? keyCount * 3 : keyCount;
                if (valueAccessor.count < expectedOutputCount)
                {
                    sampler.Times.clear();
                    continue;
                }

                const int pathHint = samplerPathHint[samplerIndex];
                sampler.Path = pathHint >= 0 ? static_cast<std::uint32_t>(pathHint) : 0u;

                if (pathHint == 0)
                {
                    sampler.Translations.resize(keyCount);
                    for (std::size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                    {
                        const std::size_t valueIndex = cubicSpline ? (keyIndex * 3 + 1) : keyIndex;
                        const std::uint8_t* valueBytes = AccessorElementPointer(model, valueAccessor, valueIndex);
                        const std::array<float, 3> t = ReadVec3(valueBytes, valueAccessor.componentType, valueAccessor.normalized);
                        sampler.Translations[keyIndex] = { t[0], t[1], -t[2] };
                    }
                }
                else if (pathHint == 1)
                {
                    sampler.Rotations.resize(keyCount);
                    for (std::size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                    {
                        const std::size_t valueIndex = cubicSpline ? (keyIndex * 3 + 1) : keyIndex;
                        const std::uint8_t* valueBytes = AccessorElementPointer(model, valueAccessor, valueIndex);
                        const std::array<float, 4> q = ReadVec4(valueBytes, valueAccessor.componentType, valueAccessor.normalized);
                        sampler.Rotations[keyIndex] = { -q[0], -q[1], q[2], q[3] };
                    }
                }
                else if (pathHint == 2)
                {
                    sampler.Scales.resize(keyCount);
                    for (std::size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                    {
                        const std::size_t valueIndex = cubicSpline ? (keyIndex * 3 + 1) : keyIndex;
                        const std::uint8_t* valueBytes = AccessorElementPointer(model, valueAccessor, valueIndex);
                        const std::array<float, 3> s = ReadVec3(valueBytes, valueAccessor.componentType, valueAccessor.normalized);
                        sampler.Scales[keyIndex] = { s[0], s[1], s[2] };
                    }
                }
                else
                {
                    sampler.Times.clear();
                }
            }

            if (!clip.Channels.empty())
            {
                clip.Duration = duration;
                outData.Animations.push_back(std::move(clip));
            }
        }

        return !outData.Primitives.empty();
    }

    fs::path BuildGlbLodOutputRelativePath(const fs::path& sourceRelativePath, std::size_t meshIndex, std::size_t primitiveIndex, int lodLevel)
    {
        fs::path modelRoot = sourceRelativePath;
        modelRoot.replace_extension();

        std::ostringstream filename;
        filename << "mesh_" << std::setw(3) << std::setfill('0') << meshIndex
                 << "_prim_" << std::setw(3) << std::setfill('0') << primitiveIndex
                 << "_lod" << lodLevel << ".indices.bin";
        return modelRoot / "LODs" / filename.str();
    }

    bool WriteIndicesBinary(const fs::path& outputPath, const std::vector<std::uint32_t>& indices)
    {
        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output.write(reinterpret_cast<const char*>(indices.data()), static_cast<std::streamsize>(indices.size() * sizeof(std::uint32_t)));
        return output.good();
    }

    bool WriteGlbLodManifest(const fs::path& manifestPath, const fs::path& sourceRelativePath, const std::vector<GlbPrimitiveLodInfo>& primitiveLods)
    {
        std::ofstream output(manifestPath, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << "{\n";
        output << "  \"source\": \"" << JsonEscape(sourceRelativePath.generic_string()) << "\",\n";
        output << "  \"primitives\": [\n";
        for (std::size_t i = 0; i < primitiveLods.size(); ++i)
        {
            const GlbPrimitiveLodInfo& prim = primitiveLods[i];
            output << "    {\n";
            output << "      \"meshIndex\": " << prim.meshIndex << ",\n";
            output << "      \"primitiveIndex\": " << prim.primitiveIndex << ",\n";
            output << "      \"vertexCount\": " << prim.vertexCount << ",\n";
            output << "      \"sourceIndexCount\": " << prim.sourceIndexCount << ",\n";
                 output << "      \"bounds\": { \"min\": [" << prim.min.x << ", " << prim.min.y << ", " << prim.min.z
                     << "], \"max\": [" << prim.max.x << ", " << prim.max.y << ", " << prim.max.z << "] },\n";
            output << "      \"skipped\": " << (prim.skipped ? "true" : "false") << ",\n";
            output << "      \"reason\": " << (prim.reason.empty() ? "null" : ("\"" + JsonEscape(prim.reason) + "\"")) << ",\n";
            output << "      \"lods\": [\n";
            for (std::size_t li = 0; li < prim.lods.size(); ++li)
            {
                const GlbLodLevelInfo& lod = prim.lods[li];
                output << "        {\n";
                output << "          \"level\": " << lod.level << ",\n";
                output << "          \"ratio\": " << lod.ratio << ",\n";
                output << "          \"error\": " << lod.error << ",\n";
                output << "          \"indexCount\": " << lod.indexCount << ",\n";
                  output << "          \"bounds\": { \"min\": [" << lod.min.x << ", " << lod.min.y << ", " << lod.min.z
                      << "], \"max\": [" << lod.max.x << ", " << lod.max.y << ", " << lod.max.z << "] },\n";
                output << "          \"output\": \"" << JsonEscape(lod.output) << "\"\n";
                output << "        }" << (li + 1 == prim.lods.size() ? "\n" : ",\n");
            }
            output << "      ]\n";
            output << "    }" << (i + 1 == primitiveLods.size() ? "\n" : ",\n");
        }
        output << "  ]\n";
        output << "}\n";

        return true;
    }

    bool WriteGlbCookManifest(const fs::path& manifestPath, const fs::path& sourceRelativePath, const GlbManifestInfo& info)
    {
        std::ofstream output(manifestPath, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << "{\n";
        output << "  \"source\": \"" << JsonEscape(sourceRelativePath.generic_string()) << "\",\n";
        output << "  \"sourceGlbHash\": \"" << Hex64(info.sourceGlbHash) << "\",\n";
        output << "  \"cookTimestampUtc\": \"" << JsonEscape(info.cookTimestampUtc) << "\",\n";
        output << "  \"runtimeFormat\": {\n";
        output << "    \"magic\": \"DXMD\",\n";
        output << "    \"versionMajor\": " << DX12Engine::kCookedModelVersionMajor << ",\n";
        output << "    \"versionMinor\": " << DX12Engine::kCookedModelVersionMinor << ",\n";
        output << "    \"endianness\": \"little\",\n";
        output << "    \"alignment\": " << DX12Engine::kCookedModelAlignment << "\n";
        output << "  },\n";
        output << "  \"chunkSizes\": {\n";
        output << "    \"vertices\": " << info.chunkVerticesBytes << ",\n";
        output << "    \"indicesLod\": " << info.chunkIndicesLodBytes << ",\n";
        output << "    \"meshes\": " << info.chunkMeshesBytes << ",\n";
        output << "    \"nodes\": " << info.chunkNodesBytes << ",\n";
        output << "    \"materials\": " << info.chunkMaterialsBytes << ",\n";
        output << "    \"strings\": " << info.chunkStringsBytes << ",\n";
        output << "    \"animations\": " << info.chunkAnimationsBytes << "\n";
        output << "  },\n";
        output << "  \"auxiliaryFileSizes\": {\n";
        output << "    \"materialsManifest\": " << info.materialManifestBytes << ",\n";
        output << "    \"lodManifest\": " << info.lodManifestBytes << ",\n";
        output << "    \"textureDdsTotal\": " << info.textureDdsBytes << "\n";
        output << "  },\n";
        output << "  \"textureFiles\": [\n";
        for (std::size_t i = 0; i < info.textureFiles.size(); ++i)
        {
            output << "    \"" << JsonEscape(info.textureFiles[i]) << "\"" << (i + 1 == info.textureFiles.size() ? "\n" : ",\n");
        }
        output << "  ]\n";
        output << "}\n";
        return true;
    }

    std::uint64_t AlignUp(std::uint64_t value, std::uint64_t alignment)
    {
        if (alignment == 0)
        {
            return value;
        }
        const std::uint64_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    void AppendRawBytes(std::vector<std::uint8_t>& out, const void* data, std::size_t size)
    {
        if (!data || size == 0)
        {
            return;
        }

        const std::size_t offset = out.size();
        out.resize(offset + size);
        std::memcpy(out.data() + offset, data, size);
    }

    template <typename T>
    void AppendPod(std::vector<std::uint8_t>& out, const T& value)
    {
        AppendRawBytes(out, &value, sizeof(T));
    }

    template <typename T>
    void AppendPodVector(std::vector<std::uint8_t>& out, const std::vector<T>& values)
    {
        if (values.empty())
        {
            return;
        }
        AppendRawBytes(out, values.data(), values.size() * sizeof(T));
    }

    std::uint32_t AddStringToTable(
        const std::string& text,
        std::vector<char>& table,
        std::unordered_map<std::string, std::uint32_t>& offsets)
    {
        if (text.empty())
        {
            return 0u;
        }

        const auto it = offsets.find(text);
        if (it != offsets.end())
        {
            return it->second;
        }

        const std::uint32_t offset = static_cast<std::uint32_t>(table.size());
        table.insert(table.end(), text.begin(), text.end());
        table.push_back('\0');
        offsets.emplace(text, offset);
        return offset;
    }

    bool BuildCompiledModelLodStage(CompiledModelData& ioData, std::vector<GlbPrimitiveLodInfo>& outPrimitiveLods)
    {
        outPrimitiveLods.clear();
        outPrimitiveLods.reserve(ioData.Primitives.size());

        for (CompiledPrimitiveData& prim : ioData.Primitives)
        {
            GlbPrimitiveLodInfo record;
            record.meshIndex = prim.MeshIndex;
            record.primitiveIndex = prim.PrimitiveIndex;
            record.vertexCount = prim.Vertices.size();

            if (prim.Vertices.empty() || prim.Lods.empty() || prim.Lods[0].Indices.empty())
            {
                record.skipped = true;
                record.reason = "missing base mesh data";
                outPrimitiveLods.push_back(std::move(record));
                continue;
            }

            // Copy base LOD indices before clearing prim.Lods.
            // Using a reference here would dangle after prim.Lods.clear().
            const std::vector<std::uint32_t> sourceIndices = prim.Lods[0].Indices;
            record.sourceIndexCount = sourceIndices.size();

            std::vector<float> positions;
            positions.resize(prim.Vertices.size() * 3);
            for (std::size_t vi = 0; vi < prim.Vertices.size(); ++vi)
            {
                positions[vi * 3 + 0] = prim.Vertices[vi].Position.x;
                positions[vi * 3 + 1] = prim.Vertices[vi].Position.y;
                positions[vi * 3 + 2] = prim.Vertices[vi].Position.z;
            }

            prim.Lods.clear();

            for (std::size_t lodLevel = 0; lodLevel < kCookLodRatios.size(); ++lodLevel)
            {
                const float ratio = kCookLodRatios[lodLevel];
                std::size_t targetIndexCount = static_cast<std::size_t>(static_cast<double>(sourceIndices.size()) * ratio);
                targetIndexCount = (std::max<std::size_t>)(targetIndexCount, 3u);
                targetIndexCount = (targetIndexCount / 3) * 3;
                targetIndexCount = (std::max<std::size_t>)(targetIndexCount, 3u);
                targetIndexCount = (std::min<std::size_t>)(targetIndexCount, sourceIndices.size());

                std::vector<std::uint32_t> lodIndices(sourceIndices.size());
                float lodError = 0.0f;
                std::size_t lodIndexCount = sourceIndices.size();

                if (lodLevel == 0)
                {
                    lodIndices = sourceIndices;
                    lodError = 0.0f;
                }
                else
                {
                    lodIndexCount = meshopt_simplify(
                        lodIndices.data(),
                        sourceIndices.data(),
                        sourceIndices.size(),
                        positions.data(),
                        prim.Vertices.size(),
                        sizeof(float) * 3,
                        targetIndexCount,
                        kCookLodTargetErrors[lodLevel],
                        0,
                        &lodError);

                    if (lodIndexCount < 3 || (lodIndexCount % 3) != 0)
                    {
                        lodIndices = sourceIndices;
                        lodError = 0.0f;
                    }
                    else
                    {
                        lodIndices.resize(lodIndexCount);
                    }
                }

                CompiledLodData lodData;
                lodData.Level = static_cast<std::uint32_t>(lodLevel);
                lodData.Ratio = ratio;
                lodData.Error = lodError;
                lodData.Indices = std::move(lodIndices);
                lodData.Bounds = ComputeBoundsFromIndexedVertices(prim.Vertices, lodData.Indices);
                prim.Lods.push_back(lodData);

                GlbLodLevelInfo lodInfo;
                lodInfo.level = static_cast<int>(lodLevel);
                lodInfo.ratio = ratio;
                lodInfo.error = lodError;
                lodInfo.indexCount = lodData.Indices.size();
                lodInfo.min = lodData.Bounds.Min;
                lodInfo.max = lodData.Bounds.Max;
                record.lods.push_back(std::move(lodInfo));
            }

            prim.Bounds = prim.Lods[0].Bounds;
            record.min = prim.Bounds.Min;
            record.max = prim.Bounds.Max;
            outPrimitiveLods.push_back(std::move(record));
        }

        return true;
    }

    bool WriteCompiledLodIndexBinaries(
        const fs::path& outputRoot,
        const fs::path& sourceRelativePath,
        const CompiledModelData& data,
        std::vector<GlbPrimitiveLodInfo>& ioPrimitiveLods,
        std::uint64_t& outTotalBytes)
    {
        outTotalBytes = 0;
        if (ioPrimitiveLods.size() != data.Primitives.size())
        {
            return false;
        }

        bool success = true;
        for (std::size_t pi = 0; pi < data.Primitives.size(); ++pi)
        {
            const CompiledPrimitiveData& prim = data.Primitives[pi];
            GlbPrimitiveLodInfo& record = ioPrimitiveLods[pi];
            if (record.skipped)
            {
                continue;
            }

            if (record.lods.size() != prim.Lods.size())
            {
                return false;
            }

            for (std::size_t li = 0; li < prim.Lods.size(); ++li)
            {
                const CompiledLodData& lod = prim.Lods[li];
                const fs::path lodRel = BuildGlbLodOutputRelativePath(
                    sourceRelativePath,
                    prim.MeshIndex,
                    prim.PrimitiveIndex,
                    static_cast<int>(lod.Level));
                const fs::path lodOut = outputRoot / lodRel;

                std::error_code ec;
                fs::create_directories(lodOut.parent_path(), ec);
                if (ec)
                {
                    std::wcerr << L"[ERROR] Failed to create LOD output directory: " << lodOut.parent_path() << L"\n";
                    success = false;
                    continue;
                }

                if (!WriteIndicesBinary(lodOut, lod.Indices))
                {
                    std::wcerr << L"[ERROR] Failed to write LOD index file: " << lodOut << L"\n";
                    success = false;
                    continue;
                }

                record.lods[li].output = lodRel.generic_string();
                outTotalBytes += static_cast<std::uint64_t>(lod.Indices.size()) * sizeof(std::uint32_t);
            }
        }

        return success;
    }

    bool WriteCookedModelBinaryAtomic(
        const fs::path& outputPath,
        const CompiledModelData& modelData,
        std::uint64_t sourceHash,
        ModelCompilerWriteResult& outResult)
    {
        outResult = {};

        std::vector<char> stringTable;
        stringTable.push_back('\0');
        std::unordered_map<std::string, std::uint32_t> stringOffsets;

        std::vector<CookedVertexPrimitiveRecord> vertexPrimRecords;
        std::vector<DX12Engine::Vertex> vertexData;
        std::vector<std::uint32_t> primitiveLodStart(modelData.Primitives.size(), 0u);
        std::vector<std::uint32_t> primitiveLodCount(modelData.Primitives.size(), 0u);
        std::vector<CookedIndexLodRecord> lodRecords;
        std::vector<std::uint32_t> lodIndexData;

        vertexPrimRecords.reserve(modelData.Primitives.size());
        for (std::size_t pi = 0; pi < modelData.Primitives.size(); ++pi)
        {
            const CompiledPrimitiveData& prim = modelData.Primitives[pi];

            CookedVertexPrimitiveRecord vp;
            vp.PrimitiveIndex = static_cast<std::uint32_t>(pi);
            vp.VertexOffset = static_cast<std::uint32_t>(vertexData.size());
            vp.VertexCount = static_cast<std::uint32_t>(prim.Vertices.size());
            vertexPrimRecords.push_back(vp);

            vertexData.insert(vertexData.end(), prim.Vertices.begin(), prim.Vertices.end());

            if (prim.Lods.empty() || prim.Lods[0].Level != 0 || prim.Lods[0].Indices.empty())
            {
                std::wcerr << L"[ERROR] Invalid compiled primitive LOD data for primitive " << pi
                           << L" (missing non-empty LOD0 indices)\n";
                return false;
            }

            primitiveLodStart[pi] = static_cast<std::uint32_t>(lodRecords.size());
            for (const CompiledLodData& lod : prim.Lods)
            {
                if (lod.Indices.empty())
                {
                    std::wcerr << L"[ERROR] Encountered empty LOD index list while writing model.dxmd"
                               << L" (primitive=" << pi << L", lod=" << lod.Level << L")\n";
                    return false;
                }

                CookedIndexLodRecord lr;
                lr.PrimitiveIndex = static_cast<std::uint32_t>(pi);
                lr.LodLevel = lod.Level;
                lr.IndexOffset = static_cast<std::uint32_t>(lodIndexData.size());
                lr.IndexCount = static_cast<std::uint32_t>(lod.Indices.size());
                lr.Ratio = lod.Ratio;
                lr.Error = lod.Error;
                lr.Min = lod.Bounds.Min;
                lr.Max = lod.Bounds.Max;

                lodRecords.push_back(lr);
                lodIndexData.insert(lodIndexData.end(), lod.Indices.begin(), lod.Indices.end());
            }
            primitiveLodCount[pi] = static_cast<std::uint32_t>(lodRecords.size()) - primitiveLodStart[pi];
        }

        if (!modelData.Primitives.empty() && lodIndexData.empty())
        {
            std::wcerr << L"[ERROR] Refusing to write model.dxmd with empty index stream.\n";
            return false;
        }

        std::vector<CookedMeshRecord> meshRecords;
        std::vector<CookedPrimitiveRecord> primitiveRecords;
        meshRecords.reserve(modelData.Meshes.size());

        for (std::size_t mi = 0; mi < modelData.Meshes.size(); ++mi)
        {
            const CompiledMeshData& mesh = modelData.Meshes[mi];

            CookedMeshRecord mr;
            mr.NameStringOffset = AddStringToTable(mesh.Name, stringTable, stringOffsets);
            mr.PrimitiveStart = static_cast<std::uint32_t>(primitiveRecords.size());

            for (std::uint32_t globalPrimIndex : mesh.PrimitiveGlobalIndices)
            {
                if (globalPrimIndex >= modelData.Primitives.size())
                {
                    continue;
                }

                const CompiledPrimitiveData& prim = modelData.Primitives[globalPrimIndex];
                CookedPrimitiveRecord pr;
                pr.MeshIndex = static_cast<std::uint32_t>(mi);
                pr.MaterialIndex = prim.MaterialIndex;
                pr.VertexOffset = vertexPrimRecords[globalPrimIndex].VertexOffset;
                pr.VertexCount = vertexPrimRecords[globalPrimIndex].VertexCount;
                pr.LodStart = primitiveLodStart[globalPrimIndex];
                pr.LodCount = primitiveLodCount[globalPrimIndex];
                pr.Min = prim.Bounds.Min;
                pr.Max = prim.Bounds.Max;
                primitiveRecords.push_back(pr);
            }

            mr.PrimitiveCount = static_cast<std::uint32_t>(primitiveRecords.size()) - mr.PrimitiveStart;
            meshRecords.push_back(mr);
        }

        std::vector<CookedNodeRecord> nodeRecords;
        nodeRecords.reserve(modelData.Nodes.size());
        for (const CompiledNodeData& node : modelData.Nodes)
        {
            CookedNodeRecord nr;
            nr.NameStringOffset = AddStringToTable(node.Name, stringTable, stringOffsets);
            nr.ParentIndex = node.ParentIndex;
            nr.MeshIndex = node.MeshIndex;
            nr.LocalTransform = node.LocalTransform;
            nodeRecords.push_back(nr);
        }

        std::vector<CookedTextureRefRecord> textureRefRecords;
        textureRefRecords.reserve(modelData.TextureRefPaths.size());
        for (const std::string& texturePath : modelData.TextureRefPaths)
        {
            CookedTextureRefRecord tr;
            tr.PathStringOffset = AddStringToTable(texturePath, stringTable, stringOffsets);
            textureRefRecords.push_back(tr);
        }

        std::vector<CookedMaterialRecord> materialRecords;
        materialRecords.reserve(modelData.Materials.size());
        for (const CompiledMaterialData& mat : modelData.Materials)
        {
            CookedMaterialRecord mr;
            mr.NameStringOffset = AddStringToTable(mat.Name, stringTable, stringOffsets);
            mr.AlphaMode = mat.AlphaMode;
            mr.AlphaCutoff = mat.AlphaCutoff;
            mr.DoubleSided = mat.DoubleSided;
            mr.BaseColorFactor = mat.BaseColorFactor;
            mr.Metallic = mat.Metallic;
            mr.Roughness = mat.Roughness;
            mr.EmissiveFactor = mat.EmissiveFactor;
            for (std::size_t ti = 0; ti < mr.TextureRefIds.size(); ++ti)
            {
                mr.TextureRefIds[ti] = mat.TextureRefIds[ti];
            }
            materialRecords.push_back(mr);
        }

        std::vector<CookedAnimationClipRecord> animationClipRecords;
        std::vector<CookedAnimationSamplerRecord> animationSamplerRecords;
        std::vector<CookedAnimationChannelRecord> animationChannelRecords;
        std::vector<float> animationTimes;
        std::vector<DirectX::XMFLOAT3> animationTranslations;
        std::vector<DirectX::XMFLOAT4> animationRotations;
        std::vector<DirectX::XMFLOAT3> animationScales;

        animationClipRecords.reserve(modelData.Animations.size());
        for (const CompiledAnimationClipData& clip : modelData.Animations)
        {
            const std::size_t clipTimesStart = animationTimes.size();
            const std::size_t clipTranslationsStart = animationTranslations.size();
            const std::size_t clipRotationsStart = animationRotations.size();
            const std::size_t clipScalesStart = animationScales.size();

            CookedAnimationClipRecord clipRecord;
            clipRecord.NameStringOffset = AddStringToTable(clip.Name, stringTable, stringOffsets);
            clipRecord.SamplerStart = static_cast<std::uint32_t>(animationSamplerRecords.size());
            clipRecord.ChannelStart = static_cast<std::uint32_t>(animationChannelRecords.size());
            clipRecord.Duration = clip.Duration;

            std::vector<std::int32_t> clipSamplerRemap(clip.Samplers.size(), -1);
            for (std::size_t samplerIndex = 0; samplerIndex < clip.Samplers.size(); ++samplerIndex)
            {
                const CompiledAnimationSamplerData& sampler = clip.Samplers[samplerIndex];
                if (sampler.Times.empty())
                {
                    continue;
                }

                if (sampler.Path == 0 && sampler.Translations.size() != sampler.Times.size())
                {
                    continue;
                }
                if (sampler.Path == 1 && sampler.Rotations.size() != sampler.Times.size())
                {
                    continue;
                }
                if (sampler.Path == 2 && sampler.Scales.size() != sampler.Times.size())
                {
                    continue;
                }
                if (sampler.Path > 2)
                {
                    continue;
                }

                CookedAnimationSamplerRecord samplerRecord;
                samplerRecord.Interpolation = sampler.Interpolation;
                samplerRecord.Path = sampler.Path;
                samplerRecord.KeyCount = static_cast<std::uint32_t>(sampler.Times.size());
                samplerRecord.TimeOffset = static_cast<std::uint32_t>(animationTimes.size());
                animationTimes.insert(animationTimes.end(), sampler.Times.begin(), sampler.Times.end());

                if (sampler.Path == 0)
                {
                    samplerRecord.ValueOffset = static_cast<std::uint32_t>(animationTranslations.size());
                    animationTranslations.insert(animationTranslations.end(), sampler.Translations.begin(), sampler.Translations.end());
                }
                else if (sampler.Path == 1)
                {
                    samplerRecord.ValueOffset = static_cast<std::uint32_t>(animationRotations.size());
                    animationRotations.insert(animationRotations.end(), sampler.Rotations.begin(), sampler.Rotations.end());
                }
                else if (sampler.Path == 2)
                {
                    samplerRecord.ValueOffset = static_cast<std::uint32_t>(animationScales.size());
                    animationScales.insert(animationScales.end(), sampler.Scales.begin(), sampler.Scales.end());
                }

                clipSamplerRemap[samplerIndex] = static_cast<std::int32_t>(animationSamplerRecords.size()) - static_cast<std::int32_t>(clipRecord.SamplerStart);
                animationSamplerRecords.push_back(samplerRecord);
            }

            const std::uint32_t clipSamplerCount = static_cast<std::uint32_t>(animationSamplerRecords.size()) - clipRecord.SamplerStart;
            if (clipSamplerCount == 0)
            {
                continue;
            }

            for (const CompiledAnimationChannelData& channel : clip.Channels)
            {
                if (channel.SamplerIndex >= clip.Samplers.size())
                {
                    continue;
                }

                const CompiledAnimationSamplerData& srcSampler = clip.Samplers[channel.SamplerIndex];
                const std::int32_t remappedSampler = clipSamplerRemap[channel.SamplerIndex];
                if (remappedSampler < 0 || srcSampler.Path != channel.Path)
                {
                    continue;
                }

                CookedAnimationChannelRecord channelRecord;
                channelRecord.NodeIndex = channel.NodeIndex;
                channelRecord.Path = channel.Path;
                channelRecord.SamplerIndex = static_cast<std::uint32_t>(remappedSampler);
                animationChannelRecords.push_back(channelRecord);
            }

            clipRecord.SamplerCount = clipSamplerCount;
            clipRecord.ChannelCount = static_cast<std::uint32_t>(animationChannelRecords.size()) - clipRecord.ChannelStart;
            if (clipRecord.ChannelCount == 0)
            {
                animationSamplerRecords.resize(clipRecord.SamplerStart);
                animationTimes.resize(clipTimesStart);
                animationTranslations.resize(clipTranslationsStart);
                animationRotations.resize(clipRotationsStart);
                animationScales.resize(clipScalesStart);
                continue;
            }

            animationClipRecords.push_back(clipRecord);
        }

        std::vector<std::uint8_t> verticesChunk;
        {
            CookedVertexChunkHeader header;
            header.PrimitiveCount = static_cast<std::uint32_t>(vertexPrimRecords.size());
            header.VertexCount = static_cast<std::uint32_t>(vertexData.size());
            header.PrimitiveTableOffset = sizeof(CookedVertexChunkHeader);
            header.VertexDataOffset = header.PrimitiveTableOffset + (vertexPrimRecords.size() * sizeof(CookedVertexPrimitiveRecord));

            AppendPod(verticesChunk, header);
            AppendPodVector(verticesChunk, vertexPrimRecords);
            AppendPodVector(verticesChunk, vertexData);
        }

        std::vector<std::uint8_t> indicesChunk;
        {
            CookedIndexChunkHeader header;
            header.LodRecordCount = static_cast<std::uint32_t>(lodRecords.size());
            header.TotalIndexCount = static_cast<std::uint32_t>(lodIndexData.size());
            header.LodRecordOffset = sizeof(CookedIndexChunkHeader);
            header.IndexDataOffset = header.LodRecordOffset + (lodRecords.size() * sizeof(CookedIndexLodRecord));

            AppendPod(indicesChunk, header);
            AppendPodVector(indicesChunk, lodRecords);
            AppendPodVector(indicesChunk, lodIndexData);
        }

        std::vector<std::uint8_t> meshesChunk;
        {
            CookedMeshesChunkHeader header;
            header.MeshCount = static_cast<std::uint32_t>(meshRecords.size());
            header.PrimitiveCount = static_cast<std::uint32_t>(primitiveRecords.size());
            header.MeshTableOffset = sizeof(CookedMeshesChunkHeader);
            header.PrimitiveTableOffset = header.MeshTableOffset + (meshRecords.size() * sizeof(CookedMeshRecord));

            AppendPod(meshesChunk, header);
            AppendPodVector(meshesChunk, meshRecords);
            AppendPodVector(meshesChunk, primitiveRecords);
        }

        std::vector<std::uint8_t> nodesChunk;
        AppendPodVector(nodesChunk, nodeRecords);

        std::vector<std::uint8_t> materialsChunk;
        {
            CookedMaterialsChunkHeader header;
            header.MaterialCount = static_cast<std::uint32_t>(materialRecords.size());
            header.TextureRefCount = static_cast<std::uint32_t>(textureRefRecords.size());
            header.MaterialTableOffset = sizeof(CookedMaterialsChunkHeader);
            header.TextureRefTableOffset = header.MaterialTableOffset + (materialRecords.size() * sizeof(CookedMaterialRecord));

            AppendPod(materialsChunk, header);
            AppendPodVector(materialsChunk, materialRecords);
            AppendPodVector(materialsChunk, textureRefRecords);
        }

        std::vector<std::uint8_t> stringsChunk;
        if (!stringTable.empty())
        {
            AppendRawBytes(stringsChunk, stringTable.data(), stringTable.size());
        }

        std::vector<std::uint8_t> animationsChunk;
        {
            CookedAnimationsChunkHeader header;
            header.ClipCount = static_cast<std::uint32_t>(animationClipRecords.size());
            header.SamplerCount = static_cast<std::uint32_t>(animationSamplerRecords.size());
            header.ChannelCount = static_cast<std::uint32_t>(animationChannelRecords.size());
            header.TimeKeyCount = static_cast<std::uint32_t>(animationTimes.size());
            header.TranslationKeyCount = static_cast<std::uint32_t>(animationTranslations.size());
            header.RotationKeyCount = static_cast<std::uint32_t>(animationRotations.size());
            header.ScaleKeyCount = static_cast<std::uint32_t>(animationScales.size());

            header.ClipTableOffset = sizeof(CookedAnimationsChunkHeader);
            header.SamplerTableOffset = header.ClipTableOffset + (animationClipRecords.size() * sizeof(CookedAnimationClipRecord));
            header.ChannelTableOffset = header.SamplerTableOffset + (animationSamplerRecords.size() * sizeof(CookedAnimationSamplerRecord));
            header.TimeDataOffset = header.ChannelTableOffset + (animationChannelRecords.size() * sizeof(CookedAnimationChannelRecord));
            header.TranslationDataOffset = header.TimeDataOffset + (animationTimes.size() * sizeof(float));
            header.RotationDataOffset = header.TranslationDataOffset + (animationTranslations.size() * sizeof(DirectX::XMFLOAT3));
            header.ScaleDataOffset = header.RotationDataOffset + (animationRotations.size() * sizeof(DirectX::XMFLOAT4));

            AppendPod(animationsChunk, header);
            AppendPodVector(animationsChunk, animationClipRecords);
            AppendPodVector(animationsChunk, animationSamplerRecords);
            AppendPodVector(animationsChunk, animationChannelRecords);
            AppendPodVector(animationsChunk, animationTimes);
            AppendPodVector(animationsChunk, animationTranslations);
            AppendPodVector(animationsChunk, animationRotations);
            AppendPodVector(animationsChunk, animationScales);
        }

        struct ChunkPayload
        {
            DX12Engine::CookedModelChunkType Type;
            std::vector<std::uint8_t>* Bytes;
            std::uint32_t ElementCount;
        };

        std::array<ChunkPayload, 7> payloads = {
            ChunkPayload { DX12Engine::CookedModelChunkType::Vertices, &verticesChunk, static_cast<std::uint32_t>(vertexData.size()) },
            ChunkPayload { DX12Engine::CookedModelChunkType::IndicesLOD, &indicesChunk, static_cast<std::uint32_t>(lodRecords.size()) },
            ChunkPayload { DX12Engine::CookedModelChunkType::Meshes, &meshesChunk, static_cast<std::uint32_t>(meshRecords.size()) },
            ChunkPayload { DX12Engine::CookedModelChunkType::Nodes, &nodesChunk, static_cast<std::uint32_t>(nodeRecords.size()) },
            ChunkPayload { DX12Engine::CookedModelChunkType::Materials, &materialsChunk, static_cast<std::uint32_t>(materialRecords.size()) },
            ChunkPayload { DX12Engine::CookedModelChunkType::Strings, &stringsChunk, static_cast<std::uint32_t>(stringTable.size()) },
            ChunkPayload { DX12Engine::CookedModelChunkType::Animations, &animationsChunk, static_cast<std::uint32_t>(animationClipRecords.size()) }
        };

        std::vector<DX12Engine::CookedModelChunkDesc> chunkDescs;
        chunkDescs.reserve(payloads.size());

        std::uint64_t cursor = sizeof(DX12Engine::CookedModelHeader);
        cursor = AlignUp(cursor, DX12Engine::kCookedModelAlignment);

        for (const ChunkPayload& payload : payloads)
        {
            DX12Engine::CookedModelChunkDesc desc;
            desc.Type = static_cast<std::uint32_t>(payload.Type);
            desc.Offset = cursor;
            desc.Size = static_cast<std::uint64_t>(payload.Bytes->size());
            desc.ElementCount = payload.ElementCount;
            chunkDescs.push_back(desc);

            cursor += desc.Size;
            cursor = AlignUp(cursor, DX12Engine::kCookedModelAlignment);
        }

        const std::uint64_t chunkTableOffset = cursor;
        const std::uint64_t chunkTableSize = static_cast<std::uint64_t>(chunkDescs.size() * sizeof(DX12Engine::CookedModelChunkDesc));
        cursor += chunkTableSize;

        std::vector<std::uint8_t> fileData(cursor, 0u);
        for (std::size_t ci = 0; ci < payloads.size(); ++ci)
        {
            const std::vector<std::uint8_t>& bytes = *payloads[ci].Bytes;
            if (!bytes.empty())
            {
                std::memcpy(fileData.data() + chunkDescs[ci].Offset, bytes.data(), bytes.size());
            }
        }

        if (chunkTableSize > 0)
        {
            std::memcpy(fileData.data() + chunkTableOffset, chunkDescs.data(), static_cast<std::size_t>(chunkTableSize));
        }

        DX12Engine::CookedModelHeader header;
        header.Magic = DX12Engine::kCookedModelMagic;
        header.VersionMajor = DX12Engine::kCookedModelVersionMajor;
        header.VersionMinor = DX12Engine::kCookedModelVersionMinor;
        header.Endianness = DX12Engine::kCookedModelEndianLittle;
        header.Alignment = DX12Engine::kCookedModelAlignment;
        header.HeaderSize = sizeof(DX12Engine::CookedModelHeader);
        header.FileSize = static_cast<std::uint64_t>(fileData.size());
        header.ChunkCount = static_cast<std::uint32_t>(chunkDescs.size());
        header.ChunkTableOffset = chunkTableOffset;
        header.RequiredChunkMask = DX12Engine::GetDefaultRequiredChunkMask();
        header.SourceContentHash = sourceHash;

        const std::uint64_t payloadStart = sizeof(DX12Engine::CookedModelHeader);
        header.PayloadChecksum = HashBytes(
            kFnvOffsetBasis,
            fileData.data() + payloadStart,
            static_cast<std::size_t>(fileData.size() - payloadStart));

        std::string layoutError;
        if (!DX12Engine::ValidateCookedModelLayout(header, chunkDescs, fileData.size(), &layoutError))
        {
            //std::wcerr << L"[ERROR] model.dxmd validation failed: " << Utf8ToWide(layoutError) << L"\n";
            return false;
        }

        std::memcpy(fileData.data(), &header, sizeof(header));

        std::error_code ec;
        fs::create_directories(outputPath.parent_path(), ec);
        if (ec)
        {
            std::wcerr << L"[ERROR] Failed to create cooked model output directory: " << outputPath.parent_path() << L"\n";
            return false;
        }

        const fs::path tempPath = outputPath.string() + ".tmp";
        {
            std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                std::wcerr << L"[ERROR] Failed to open temp cooked model file for write: " << tempPath << L"\n";
                return false;
            }

            out.write(reinterpret_cast<const char*>(fileData.data()), static_cast<std::streamsize>(fileData.size()));
            if (!out.good())
            {
                std::wcerr << L"[ERROR] Failed to write temp cooked model file: " << tempPath << L"\n";
                return false;
            }
        }

        if (fs::exists(outputPath))
        {
            fs::remove(outputPath, ec);
            if (ec)
            {
                std::wcerr << L"[ERROR] Failed to replace cooked model file: " << outputPath << L"\n";
                fs::remove(tempPath, ec);
                return false;
            }
        }

        fs::rename(tempPath, outputPath, ec);
        if (ec)
        {
            std::wcerr << L"[ERROR] Failed to finalize cooked model file: " << outputPath << L"\n";
            fs::remove(tempPath, ec);
            return false;
        }

        outResult.ChunkVerticesBytes = verticesChunk.size();
        outResult.ChunkIndicesLodBytes = indicesChunk.size();
        outResult.ChunkMeshesBytes = meshesChunk.size();
        outResult.ChunkNodesBytes = nodesChunk.size();
        outResult.ChunkMaterialsBytes = materialsChunk.size();
        outResult.ChunkStringsBytes = stringsChunk.size();
        outResult.ChunkAnimationsBytes = animationsChunk.size();
        outResult.TotalVertexStreamBytes = vertexData.size() * sizeof(DX12Engine::Vertex);
        outResult.TotalIndexStreamBytes = lodIndexData.size() * sizeof(std::uint32_t);
        return true;
    }

    bool CookGlbFile(const fs::path& sourcePath, const fs::path& outputRoot, const fs::path& sourceRelativePath)
    {
        const std::uint64_t sourceGlbHash = HashFileContents(sourcePath).value_or(0ull);

        ParsedGlbData parsed;
        std::string warn;
        std::string err;

        if (!ParseGlbStage(sourcePath, parsed, warn, err))
        {
            std::wcerr << L"[ERROR] Failed to parse glb: " << sourcePath << L"\n";
            if (!err.empty())
            {
                std::wcerr << L"[ERROR] tinygltf: " << err.c_str() << L"\n";
            }
            return false;
        }

        if (!warn.empty())
        {
            std::wcerr << L"[WARN] glb parse warning (" << sourcePath << L"): " << warn.c_str() << L"\n";
        }

        const tinygltf::Model& model = parsed.Model;

        std::vector<TextureSemantic> semantics = InferGlbTextureSemantics(model);
        std::vector<std::string> textureOutputs(model.textures.size());
        bool success = true;

        for (std::size_t textureIndex = 0; textureIndex < model.textures.size(); ++textureIndex)
        {
            const tinygltf::Texture& texture = model.textures[textureIndex];
            if (texture.source < 0 || static_cast<std::size_t>(texture.source) >= model.images.size())
            {
                continue;
            }

            const tinygltf::Image& image = model.images[texture.source];
            DirectX::ScratchImage decodedImage;
            if (!LoadGlbImage(model, image, sourcePath, decodedImage))
            {
                std::wcerr << L"[ERROR] Failed to decode glb texture #" << textureIndex << L" in " << sourcePath << L"\n";
                success = false;
                continue;
            }

            CookSettings settings;
            settings.semantic = semantics[textureIndex];
            settings.srgbHint = settings.semantic == TextureSemantic::Albedo || settings.semantic == TextureSemantic::Emissive;

            std::string imageName = image.name;
            if (imageName.empty())
            {
                imageName = texture.name;
            }
            imageName = SanitizeName(imageName);

            const fs::path outputRelativePath = BuildGlbTextureOutputRelativePath(sourceRelativePath, textureIndex, imageName);
            const fs::path outputDdsPath = outputRoot / outputRelativePath;
            fs::path metadataPath = outputDdsPath;
            metadataPath += L".meta";

            std::error_code ec;
            fs::create_directories(outputDdsPath.parent_path(), ec);
            if (ec)
            {
                std::wcerr << L"[ERROR] Failed to create directory: " << outputDdsPath.parent_path() << L"\n";
                success = false;
                continue;
            }

            const std::string sourceTag = BuildGlbTextureTag(sourceRelativePath, textureIndex);
            if (!SaveCookedTexture(decodedImage, outputDdsPath, metadataPath, sourceTag, settings))
            {
                success = false;
                continue;
            }

            textureOutputs[textureIndex] = outputRelativePath.generic_string();
            std::wcout << L"[COOKED] " << sourceRelativePath.c_str() << L" texture #" << textureIndex << L" -> " << outputRelativePath.c_str() << L"\n";
        }

        CompiledModelData compiledModel;
        const std::string modelName = sourcePath.stem().string();
        if (!ExtractCompiledModelDataStage(model, modelName, textureOutputs, compiledModel))
        {
            std::wcerr << L"[ERROR] Failed to extract runtime model data from glb: " << sourcePath << L"\n";
            return false;
        }

        std::vector<GlbPrimitiveLodInfo> primitiveLods;
        if (!BuildCompiledModelLodStage(compiledModel, primitiveLods))
        {
            std::wcerr << L"[ERROR] Failed to generate LOD stages for glb: " << sourcePath << L"\n";
            return false;
        }

        fs::path manifestRelative = sourceRelativePath;
        manifestRelative.replace_extension();
        manifestRelative /= "materials.json";
        const fs::path manifestPath = outputRoot / manifestRelative;
        std::error_code ec;
        fs::create_directories(manifestPath.parent_path(), ec);
        if (ec)
        {
            std::wcerr << L"[ERROR] Failed to create manifest directory: " << manifestPath.parent_path() << L"\n";
            return false;
        }

        if (!WriteGlbMaterialManifest(manifestPath, sourceRelativePath, model, semantics, textureOutputs))
        {
            std::wcerr << L"[ERROR] Failed to write glb material manifest: " << manifestPath << L"\n";
            return false;
        }

        std::uint64_t lodIndexBytes = 0;
        const bool lodCookSuccess = WriteCompiledLodIndexBinaries(outputRoot, sourceRelativePath, compiledModel, primitiveLods, lodIndexBytes);

        fs::path lodManifestRelative = sourceRelativePath;
        lodManifestRelative.replace_extension();
        lodManifestRelative /= "lods.json";
        const fs::path lodManifestPath = outputRoot / lodManifestRelative;

        if (!WriteGlbLodManifest(lodManifestPath, sourceRelativePath, primitiveLods))
        {
            std::wcerr << L"[ERROR] Failed to write glb LOD manifest: " << lodManifestPath << L"\n";
            return false;
        }

        fs::path modelBinaryRelative = sourceRelativePath;
        modelBinaryRelative.replace_extension();
        modelBinaryRelative /= "model.dxmd";
        const fs::path modelBinaryPath = outputRoot / modelBinaryRelative;

        ModelCompilerWriteResult modelWriteResult;
        if (!WriteCookedModelBinaryAtomic(modelBinaryPath, compiledModel, sourceGlbHash, modelWriteResult))
        {
            std::wcerr << L"[ERROR] Failed to write cooked runtime model: " << modelBinaryPath << L"\n";
            return false;
        }

        fs::path cookedManifestRelative = sourceRelativePath;
        cookedManifestRelative.replace_extension();
        cookedManifestRelative /= "manifest.json";
        const fs::path cookedManifestPath = outputRoot / cookedManifestRelative;

        GlbManifestInfo manifestInfo;
        manifestInfo.sourceGlbHash = sourceGlbHash;
        manifestInfo.cookTimestampUtc = FormatUtcTimestampIso8601();
        manifestInfo.chunkVerticesBytes = modelWriteResult.ChunkVerticesBytes;
        manifestInfo.chunkIndicesLodBytes = modelWriteResult.ChunkIndicesLodBytes;
        manifestInfo.chunkMeshesBytes = modelWriteResult.ChunkMeshesBytes;
        manifestInfo.chunkNodesBytes = modelWriteResult.ChunkNodesBytes;
        manifestInfo.chunkMaterialsBytes = modelWriteResult.ChunkMaterialsBytes;
        manifestInfo.chunkStringsBytes = modelWriteResult.ChunkStringsBytes;
        manifestInfo.chunkAnimationsBytes = modelWriteResult.ChunkAnimationsBytes;
        manifestInfo.materialManifestBytes = GetFileSizeSafe(manifestPath);
        manifestInfo.lodManifestBytes = GetFileSizeSafe(lodManifestPath);
        manifestInfo.lodIndexBytes = lodIndexBytes;

        for (const std::string& textureOutput : textureOutputs)
        {
            if (textureOutput.empty())
            {
                continue;
            }

            manifestInfo.textureFiles.push_back(textureOutput);
            manifestInfo.textureDdsBytes += GetFileSizeSafe(outputRoot / fs::path(textureOutput));
        }

        if (!WriteGlbCookManifest(cookedManifestPath, sourceRelativePath, manifestInfo))
        {
            std::wcerr << L"[ERROR] Failed to write glb cook manifest: " << cookedManifestPath << L"\n";
            return false;
        }

        success = success && lodCookSuccess;
        return success;
    }
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    bool shouldUninitializeCom = false;
    {
        const HRESULT comInitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(comInitHr))
        {
            shouldUninitializeCom = true;
        }
        else if (comInitHr != RPC_E_CHANGED_MODE)
        {
            std::wcerr << L"Failed to initialize COM/WIC (HRESULT=0x" << std::hex << static_cast<unsigned long>(comInitHr) << std::dec << L")\n";
            return 1;
        }
    }

    Args args;
    if (!ParseArgs(argc, argv, args))
    {
        PrintUsage();
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return 1;
    }

    const fs::path inputRoot = fs::absolute(args.inputRoot).lexically_normal();
    const fs::path outputRoot = fs::absolute(args.outputRoot).lexically_normal();
    const fs::path cacheFile = outputRoot / "asset_cooker.cache";

    if (!fs::exists(inputRoot) || !fs::is_directory(inputRoot))
    {
        std::wcerr << L"Input directory does not exist: " << inputRoot << L"\n";
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return 1;
    }

    std::error_code ec;
    fs::create_directories(outputRoot, ec);
    if (ec)
    {
        std::wcerr << L"Failed to create output directory: " << outputRoot << L"\n";
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return 1;
    }

    CacheTable existingCache;
    if (!args.cleanCache)
    {
        existingCache = LoadCache(cacheFile);
    }
    CacheTable newCache;

    CookStats stats;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(inputRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const fs::path sourcePath = entry.path();
        const bool isTextureSource = IsSupportedTextureExtension(sourcePath);
        const bool isGlbSource = IsSupportedGlbExtension(sourcePath);
        if (!isTextureSource && !isGlbSource)
        {
            continue;
        }

        ++stats.scanned;
        const fs::path relativePath = fs::relative(sourcePath, inputRoot).lexically_normal();
        const std::string cacheKey = relativePath.generic_string();

        std::optional<std::uint64_t> sourceHash;
        if (isGlbSource)
        {
            sourceHash = HashGlbForCache(sourcePath);
        }
        else
        {
            const CookSettings hashSettings = InferCookSettings(sourcePath);
            sourceHash = HashFileWithSettings(sourcePath, hashSettings);
        }
        if (!sourceHash.has_value())
        {
            ++stats.failed;
            std::wcerr << L"[ERROR] Failed to hash: " << sourcePath << L"\n";
            continue;
        }

        bool upToDate = false;
        if (!args.force)
        {
            const auto cacheIt = existingCache.find(cacheKey);
            if (cacheIt != existingCache.end() && cacheIt->second == *sourceHash)
            {
                if (isTextureSource)
                {
                    fs::path outputDdsPath = outputRoot / relativePath;
                    outputDdsPath.replace_extension(".dds");
                    fs::path metadataPath = outputDdsPath;
                    metadataPath += L".meta";
                    upToDate = fs::exists(outputDdsPath) && fs::exists(metadataPath);
                }
                else
                {
                    fs::path modelRootPath = outputRoot / relativePath;
                    modelRootPath.replace_extension();

                    const fs::path materialManifestPath = modelRootPath / "materials.json";
                    const fs::path lodManifestPath = modelRootPath / "lods.json";
                    const fs::path cookedManifestPath = modelRootPath / "manifest.json";
                    const fs::path cookedModelPath = modelRootPath / "model.dxmd";
                    upToDate = fs::exists(materialManifestPath) && fs::exists(lodManifestPath) && fs::exists(cookedManifestPath) && fs::exists(cookedModelPath);
                }
            }
        }

        if (upToDate)
        {
            ++stats.skipped;
            newCache[cacheKey] = *sourceHash;
            continue;
        }

        bool cookOk = false;
        if (isTextureSource)
        {
            const CookSettings settings = InferCookSettings(sourcePath);
            fs::path outputDdsPath = outputRoot / relativePath;
            outputDdsPath.replace_extension(".dds");
            fs::path metadataPath = outputDdsPath;
            metadataPath += L".meta";

            fs::create_directories(outputDdsPath.parent_path(), ec);
            if (ec)
            {
                std::wcerr << L"[ERROR] Failed to create directory: " << outputDdsPath.parent_path() << L"\n"; 
                ec.clear();
                ++stats.failed;
                continue;
            }

            cookOk = CookTextureFile(sourcePath, outputDdsPath, metadataPath, relativePath, settings);
            if (cookOk)
            {
                std::wcout << L"[COOKED] " << relativePath.c_str() << L" -> " << fs::relative(outputDdsPath, outputRoot).c_str() << L"\n";
            }
        }
        else
        {
            cookOk = CookGlbFile(sourcePath, outputRoot, relativePath);
        }

        if (cookOk)
        {
            ++stats.cooked;
            newCache[cacheKey] = *sourceHash;
        }
        else
        {
            ++stats.failed;
        }
    }

    if (!SaveCache(cacheFile, newCache))
    {
        std::wcerr << L"[WARN] Failed to save cache file: " << cacheFile << L"\n";
    }

    std::wcout << L"\nAssetCooker summary:\n";
    std::wcout << L"  Scanned: " << stats.scanned << L"\n";
    std::wcout << L"  Cooked:  " << stats.cooked << L"\n";
    std::wcout << L"  Skipped: " << stats.skipped << L"\n";
    std::wcout << L"  Failed:  " << stats.failed << L"\n";

    if (shouldUninitializeCom)
    {
        CoUninitialize();
    }

    return stats.failed > 0 ? 1 : 0;
}
