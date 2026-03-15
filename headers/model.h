#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mesh.h"
#include "shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

class Model
{
public:
    // model data
    vector<Texture> textures_loaded; // stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh> meshes;
    string directory;
    bool gammaCorrection;
    glm::mat2x3 bounds;
    GLFWwindow *window;

    // True if there were any errors importing the model
    bool errors;

    float maxExtends;

    map<std::string, sdf_conversion::ConversionMethod *> methods;

    // constructor, expects a filepath to a 3D model.
    Model(string const &path, GLFWwindow *window, std::vector<string> &methodNames, string modelname, float boundsPadding = 0.1f, bool gamma = false) : gammaCorrection(gamma)
    {
        this->errors = !loadModel(path);
        if (this->errors)
            return;
        this->boundsPadding = boundsPadding;
        this->window = window;
        bounds = calculateBoundingBox(boundsPadding);
        prepareSSBOs();

        methods = {
            {methodNames[1], new sdf_conversion::NaiveConversion(SSBO_VERTEX, SSBO_INDEX, bounds, topologyLength, window, modelname)},
            {methodNames[2], new sdf_conversion::JFConversion(SSBO_VERTEX, SSBO_INDEX, bounds, topologyLength, window, modelname)},
            {methodNames[3], new sdf_conversion::BattyConversion(SSBO_VERTEX, SSBO_INDEX, bounds, topologyLength, window, modelname)},
            {methodNames[4], new sdf_conversion::RaymapConversion(SSBO_VERTEX, SSBO_INDEX, bounds, topologyLength, window, modelname)},
        };
    }

    ~Model()
    {
        // Delete SDF for each method
        for (auto &[name, method] : methods)
        {
            glDeleteTextures(1, &method->sdf);
            delete method;
        }
        glDeleteBuffers(1, &SSBO_VERTEX);
        glDeleteBuffers(1, &SSBO_INDEX);
        for (auto &tex : textures_loaded)
            glDeleteTextures(1, &tex.id);
    }

    /// @brief Runs the given conversion
    /// @param shadername Name of the conversion method
    /// @return CPU Wall time for the conversion, -1 if something went wrong
    double BenchmarkShader(const std::string &shadername)
    {
        double starttime = glfwGetTime();
        methods[shadername]->PrepareSDF(false);
        return glfwGetTime() - starttime;
    }

    unsigned int GetSDFByName(const std::string &shadername)
    {
        return methods[shadername]->sdf;
    }

    void TryLoadSDF(string const &path, unsigned int &texInt, bool &preparedFlag)
    {
        texInt = SDFCache::load_from_file(path.c_str(), sdf_conversion::sdfSize);
        preparedFlag = texInt != 0;
    }

    // draws the model, and thus all its meshes
    void Draw(Shader &shader)
    {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

    // Render model using naive SDF data
    void DrawUsingSDF(const std::string &conversionMethod, Shader &drawshader, Model *flatscreen, bool realtime)
    {
        sdf_conversion::ConversionMethod *m = methods[conversionMethod];
        if (!m->sdf_ready || realtime)
        {
            m->PrepareSDF(!realtime);
        }
        drawshader.use();

        LoadSDFIntoShader(m->sdf, drawshader);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, SSBO_VERTEX);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, SSBO_INDEX);

        flatscreen->Draw(drawshader);
    }

    void LoadSDFIntoShader(unsigned int sdfTexID, Shader &shader, bool debug = false)
    {
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_3D, sdfTexID);
        glUniform1i(glGetUniformLocation(shader.ID, "sdf"), 15);

        if (debug)
        {
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        else
        {
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glUniform1f(glGetUniformLocation(shader.ID, "bounding_box_padding"), boundsPadding);
        glUniform1i(glGetUniformLocation(shader.ID, "sdfSize"), sdf_conversion::sdfSize);
        glUniformMatrix2x3fv(glGetUniformLocation(shader.ID, "boundingBox"), 1, false, &bounds[0][0]);

        glBindImageTexture(3, sdfTexID, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    }

    int GetModelVertices()
    {
        int vertCount = 0;
        for (int i = 0; i < (int)meshes.size(); i++)
        {
            vertCount += meshes[i].vertices.size();
        }
        return vertCount;
    }

    int GetModelFaces()
    {
        int faceCount = 0;
        for (int i = 0; i < (int)meshes.size(); i++)
        {
            faceCount += meshes[i].indices.size() / 3;
        }
        return faceCount;
    }

private:
    unsigned int SSBO_VERTEX, SSBO_INDEX, topologyLength;

    // Combines the data of all meshes into a single SSBO
    void prepareSSBOs()
    {
        int vertexCounts = 0, indexCounts = 0;
        for (unsigned int i = 0; i < meshes.size(); i++)
        {
            vertexCounts += meshes[i].vertices.size();
            indexCounts += meshes[i].indices.size();
        }

        topologyLength = indexCounts;

        std::vector<glm::vec4> positions;
        positions.reserve(vertexCounts);
        std::vector<unsigned int> indices;
        indices.reserve(indexCounts);
        unsigned int indexOffset = 0;

        for (unsigned int i = 0; i < meshes.size(); i++)
        {
            for (Vertex &v : meshes[i].vertices)
            {
                positions.push_back(glm::vec4(v.Position, 1.0));
            }
            for (unsigned int index : meshes[i].indices)
            {
                indices.push_back(index + indexOffset);
            }
            indexOffset += meshes[i].vertices.size();
        }

        glGenBuffers(1, &SSBO_VERTEX);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO_VERTEX);
        glBufferData(GL_SHADER_STORAGE_BUFFER, positions.size() * sizeof(glm::vec4), &positions[0], GL_STATIC_DRAW);

        glGenBuffers(1, &SSBO_INDEX);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO_INDEX);
        glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    }

    float boundsPadding;

    /// @brief
    /// @param boundsPadding by what ratio to stretch the bounding box
    /// @return
    glm::mat2x3 calculateBoundingBox(float boundsPadding)
    {
        float *b = new float[6];
        float *lower = b;
        float *upper = b + 3;

        memcpy(lower, &meshes[0].vertices[0].Position, sizeof(glm::vec3));
        memcpy(upper, &meshes[0].vertices[0].Position, sizeof(glm::vec3));

        for (int i = 0; i < (int)meshes.size(); i++)
        {
            glm::mat2x3 meshBounds = meshes[i].calculateBoundingBox();
            lower[0] = min(lower[0], meshBounds[0][0]);
            lower[1] = min(lower[1], meshBounds[0][1]);
            lower[2] = min(lower[2], meshBounds[0][2]);
            upper[0] = max(upper[0], meshBounds[1][0]);
            upper[1] = max(upper[1], meshBounds[1][1]);
            upper[2] = max(upper[2], meshBounds[1][2]);
        }

        glm::vec3 stretch = (glm::vec3(upper[0], upper[1], upper[2]) - glm::vec3(lower[0], lower[1], lower[2]));
        this->maxExtends = max(stretch[0], max(stretch[1], stretch[2]));
        stretch *= boundsPadding / 2;

        lower[0] -= stretch[0];
        lower[1] -= stretch[1];
        lower[2] -= stretch[2];
        upper[0] += stretch[0];
        upper[1] += stretch[1];
        upper[2] += stretch[2];

        glm::mat2x3 boundingBox = glm::make_mat2x3(b);
        return boundingBox;
    }

    /// @summary loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    /// @returns has model been loaded successfully?
    bool loadModel(string const &path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // check for file errors
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
            return false;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('/'));

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene);

        if (this->meshes.size() == 0)
        {
            std::cout << "ERROR::LOADMODEL::EMPTY" << std::endl;
            return false;
        }

        return true;
    }

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene)
    {
        // process each mesh located at the current node
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene.
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        // data to fill
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // walk through each vertex
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vec;

            // positions
            vec.x = mesh->mVertices[i].x;
            vec.y = mesh->mVertices[i].y;
            vec.z = mesh->mVertices[i].z;
            vertex.Position = vec;

            // normals
            if (mesh->HasNormals())
            {
                vec.x = mesh->mNormals[i].x;
                vec.y = mesh->mNormals[i].y;
                vec.z = mesh->mNormals[i].z;
                vertex.Normal = vec;
            }

            // texture coordinates
            if (mesh->mTextureCoords[0])
            {
                glm::vec2 tex;
                tex.x = mesh->mTextureCoords[0][i].x;
                tex.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = tex;

                // tangent & bitangent
                vec.x = mesh->mTangents[i].x;
                vec.y = mesh->mTangents[i].y;
                vec.z = mesh->mTangents[i].z;
                vertex.Tangent = vec;

                vec.x = mesh->mBitangents[i].x;
                vec.y = mesh->mBitangents[i].y;
                vec.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vec;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f);

            vertices.push_back(vertex);
        }

        // indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        // load textures
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

        vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

        vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

        // load material properties
        Material meshMaterial; // make sure Mesh has a MaterialProps member
        aiColor3D color;
        float f;
        int illum;

        if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
            meshMaterial.Ka = glm::vec3(color.r, color.g, color.b);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
            meshMaterial.Kd = glm::vec3(color.r, color.g, color.b);
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
            meshMaterial.Ks = glm::vec3(color.r, color.g, color.b);
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
            meshMaterial.Ke = glm::vec3(color.r, color.g, color.b);

        if (material->Get(AI_MATKEY_SHININESS, f) == AI_SUCCESS)
            meshMaterial.Ns = f;
        if (material->Get(AI_MATKEY_OPACITY, f) == AI_SUCCESS)
            meshMaterial.d = f;
        if (material->Get(AI_MATKEY_SHADING_MODEL, illum) == AI_SUCCESS)
            meshMaterial.illum = illum;

        // return mesh with textures and material
        return Mesh(vertices, indices, textures, meshMaterial); // Make sure Mesh constructor supports MaterialProps
    }

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;

            mat->GetTexture(type, i, &str);

            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                    break;
                }
            }
            if (!skip)
            { // if texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture); // store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
            }
        }
        return textures;
    }
};

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path when loading model: " << filename << std::endl;
        std::cout << "Dir: " << directory << ", Path: " << string(path) << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
#endif
