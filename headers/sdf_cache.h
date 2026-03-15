#include <fstream>
#include <vector>
#include <cstdint>
#include <iostream>

namespace SDFCache
{
    struct rgba32f
    {
        float r, g, b, a;
    };

    struct ra32f
    {
        float r, a;
    };

    /// @brief Load a SDF cache file from the specified path into a 3D Texture
    /// @param path Path to .sdf cache file
    /// @return GL texture id or 0 if error
    static unsigned int load_from_file(const char *path, uint16_t targetSdfSize)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return 0;

        uint16_t sdfWidth;
        f.read(reinterpret_cast<char *>(&sdfWidth), sizeof(uint16_t));

        if (sdfWidth != targetSdfSize)
            return 0;

        size_t count = static_cast<size_t>(sdfWidth) * sdfWidth * sdfWidth;

        std::vector<ra32f> fileData(count);
        f.read(reinterpret_cast<char *>(fileData.data()), count * sizeof(ra32f));
        f.close();

        std::vector<rgba32f> glData(count);
        for (size_t i = 0; i < count; ++i)
        {
            glData[i].r = fileData[i].r;
            glData[i].g = 0.0f; // Empty
            glData[i].b = 0.0f; // Empty
            glData[i].a = fileData[i].a;
        }

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_3D, tex);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glTexImage3D(
            GL_TEXTURE_3D,
            0,
            GL_RGBA32F,
            sdfWidth, sdfWidth, sdfWidth,
            0,
            GL_RGBA,
            GL_FLOAT,
            glData.data());

        glBindTexture(GL_TEXTURE_3D, 0);
        return tex;
    }

    /// @brief Saves the given SDF to a SDF cache file
    /// @param path Path to the .sdf file, overrides if it exists
    /// @param glTexID The texture id which should be saved
    static void save_to_file(const char *path, const unsigned int glTexID)
    {
        glBindTexture(GL_TEXTURE_3D, glTexID);

        int sdfWidth;
        glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_WIDTH, &sdfWidth);

        size_t count = static_cast<size_t>(sdfWidth) * sdfWidth * sdfWidth;

        std::vector<rgba32f> glData(count);
        glGetTexImage(
            GL_TEXTURE_3D,
            0,
            GL_RGBA,
            GL_FLOAT,
            glData.data());

        glBindTexture(GL_TEXTURE_3D, 0);

        std::vector<ra32f> fileData(count);
        for (size_t i = 0; i < count; ++i)
        {
            fileData[i].r = glData[i].r;
            fileData[i].a = glData[i].a;
        }

        std::ofstream f(path, std::ios::binary);
        if (!f)
        {
            std::cerr << "Error writing cache to file: " << path << std::endl;
            return;
        }

        uint16_t w16 = static_cast<uint16_t>(sdfWidth);
        f.write(reinterpret_cast<char *>(&w16), sizeof(uint16_t));
        f.write(reinterpret_cast<char *>(fileData.data()), count * sizeof(ra32f));
        f.close();
    }
}