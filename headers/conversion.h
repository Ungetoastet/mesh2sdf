#include <vector>
#include <iomanip>
#include "shader.h"

namespace sdf_conversion
{
    // General settings, apply to all
    static int sdfSize = 128;
    const int topologyChunksSize = 128 * 3;

    Shader *shaderSDFSignDet;

    Shader *shaderSDFNaive;

    Shader *shaderJFSeed;
    Shader *shaderJFFlood;
    Shader *shaderJFDistance;

    Shader *shaderBattyInit;
    Shader *shaderBattySweep;
    Shader *shaderBattySignDet;

    Shader *shaderRMIntersectCount;
    Shader *shaderRMBlellochScan;
    Shader *shaderRMBlellochOffset;
    Shader *shaderRMIndexing;
    Shader *shaderRMDistInit;
    Shader *shaderRMDistTransform;
    Shader *shaderRMBuffer2SDF;
    Shader *shaderRMDistClean;
    Shader *shaderRMLocalIntersect;
    Shader *shaderRMIntersectAccumulate;
    Shader *shaderRMInitCuts;
    Shader *shaderRMSignDet;

    static void printProgress(unsigned int current, unsigned int total, const char *title, GLFWwindow *window)
    {
        float progress = (float)current / total;
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << "\r" << title << " | " << (progress * 100) << "%                                                               ";
        std::cout << ss.str() << std::flush;
        glfwSetWindowTitle(window, ss.str().c_str());
        glfwPollEvents();
    }
    static void clearProgress(GLFWwindow *window)
    {
        std::cout << "\r                                                                                   " << std::flush;
        glfwSetWindowTitle(window, "Toast Model Viewer");
    }

    // Tries to activate the shader. If the shader isnt compiled yet, compile it
    static void CompileConversionShaders()
    {
        shaderSDFSignDet = new Shader("./shaders/signdet/gwnsigndet.comp");

        shaderSDFNaive = new Shader("./shaders/naive_gen.comp");

        shaderJFSeed = new Shader("./shaders/jumpflood/seeding.comp");
        shaderJFFlood = new Shader("./shaders/jumpflood/flooding.comp");
        shaderJFDistance = new Shader("./shaders/jumpflood/distance.comp");

        shaderBattyInit = new Shader("./shaders/batty/initdist.comp");
        shaderBattySweep = new Shader("./shaders/batty/sweep.comp");
        shaderBattySignDet = new Shader("./shaders/batty/sign.comp");

        shaderRMIntersectCount = new Shader("./shaders/raymaps/01_intersect_count.comp");
        shaderRMBlellochScan = new Shader("./shaders/raymaps/02_blelloch_scan.comp");
        shaderRMBlellochOffset = new Shader("./shaders/raymaps/03_blelloch_offset.comp");
        shaderRMIndexing = new Shader("./shaders/raymaps/04_index_write.comp");
        shaderRMDistInit = new Shader("./shaders/raymaps/05_dist_init.comp");
        shaderRMDistTransform = new Shader("./shaders/raymaps/06_dist_transform.comp");
        shaderRMBuffer2SDF = new Shader("./shaders/raymaps/07_buffertosdf.comp");
        shaderRMDistClean = new Shader("./shaders/raymaps/08_dist_cleanup.comp");
        shaderRMLocalIntersect = new Shader("./shaders/raymaps/09_local_intersect.comp");
        shaderRMIntersectAccumulate = new Shader("./shaders/raymaps/10_intersect_acc.comp");
        shaderRMInitCuts = new Shader("./shaders/raymaps/11_initial_cuts.comp");
        shaderRMSignDet = new Shader("./shaders/raymaps/12_vote_sign.comp");
    }

    static GLuint InitSDF()
    {
        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_3D, t);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, sdfSize, sdfSize, sdfSize, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        const GLfloat clearValue[4] = {FLT_MAX, 0.0f, 0.0f, -1.0f};
        glClearTexImage(t, 0, GL_RGBA, GL_FLOAT, clearValue);
        return t;
    }

    static GLuint InitUintTex()
    {
        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_3D, t);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R32UI, sdfSize, sdfSize, sdfSize, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        const GLuint clearvalue[4] = {0u, 0u, 0u, 0u};
        glClearTexImage(t, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, clearvalue);
        return t;
    }

    static void SetupConversionShader(Shader *shader, glm::mat2x3 &boundingBox, bool textureUInt = false)
    {
        shader->use();
        if (textureUInt)
            shader->setUInt("textureSize", sdfSize);
        else
            shader->setInt("textureSize", sdfSize);
        shader->SetMat2x3("boundingBox", boundingBox);
    }

    static void SetupConversionShader(Shader *shader, unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &boundingBox, unsigned int topologyLength, bool textureUInt = false)
    {
        SetupConversionShader(shader, boundingBox, textureUInt);
        shader->setUInt("topologyLength", topologyLength);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_vertex);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_index);
    }

    static void DispatchAndWait(GLuint x, GLuint y, GLuint z, bool hardWait = true)
    {
        glDispatchCompute(x, y, z);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
        if (hardWait)
            glFinish(); // Needed so that the OS has time to use GPU
    }

    /// @brief Determine the signs of the sdf in place
    static void DetermineSDFSign(GLuint sdf, unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &boundingBox, unsigned int topologyLength, GLFWwindow *window)
    {
        SetupConversionShader(shaderSDFSignDet, ssbo_vertex, ssbo_index, boundingBox, topologyLength);

        glBindImageTexture(0, sdf, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);

        GLuint numGroups = (sdfSize + 7) / 8;

        for (unsigned int topoIndexStart = 0; topoIndexStart < topologyLength; topoIndexStart += topologyChunksSize)
        {
            unsigned int topoend = std::min(topoIndexStart + topologyChunksSize, topologyLength);
            shaderSDFSignDet->setUInt("topologyIndexStart", topoIndexStart);
            shaderSDFSignDet->setUInt("topologyIndexEnd", topoend);
            DispatchAndWait(numGroups, numGroups, numGroups);
            printProgress(topoend, topologyLength, "Sign determination", window);
        }

        return;
    }

    /// @brief Generates the SDF in chunks and merges them. Returns the final, merged sdf
    /// @return gl texture id of the generated texture
    static unsigned int GenerateSDF_Naive(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &boundingBox, unsigned int topologyLength, GLFWwindow *window)
    {
        SetupConversionShader(shaderSDFNaive, ssbo_vertex, ssbo_index, boundingBox, topologyLength);

        GLuint sdf = InitSDF();

        glBindImageTexture(0, sdf, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);

        // Calculate the number of workgroups needed in each dimension
        int localSize = 8;
        GLuint numGroups = (sdfSize + localSize - 1) / localSize;

        for (unsigned int topoIndexStart = 0; topoIndexStart < topologyLength; topoIndexStart += topologyChunksSize)
        {
            shaderSDFNaive->setUInt("topologyIndexStart", topoIndexStart);
            shaderSDFNaive->setUInt("topologyIndexEnd", std::min(topoIndexStart + topologyChunksSize, topologyLength));
            shaderSDFNaive->setUInt("topologyLength", topologyLength);

            DispatchAndWait(numGroups, numGroups, numGroups);
            printProgress(topoIndexStart, topologyLength, "Distance calculation", window);
        }
        clearProgress(window);
        return sdf;
    }

    static unsigned int GenerateSDF_Jumpflood(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &boundingBox, unsigned int topologyLength, GLFWwindow *window)
    {
        GLuint sdfA = InitSDF();
        GLuint sdfB = InitSDF();

        // SEEDING
        SetupConversionShader(shaderJFSeed, ssbo_vertex, ssbo_index, boundingBox, topologyLength);

        glBindImageTexture(0, sdfA, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        GLuint numGroups = (topologyLength / 256) + 1;

        for (unsigned int topoIndexStart = 0; topoIndexStart < topologyLength; topoIndexStart += topologyChunksSize)
        {
            shaderJFSeed->setUInt("topoIndexStart", topoIndexStart);
            shaderJFSeed->setUInt("topoIndexEnd", std::min(topoIndexStart + topologyChunksSize, topologyLength));

            DispatchAndWait(numGroups, 1, 1);
            printProgress(topoIndexStart, topologyLength, "Seeding", window);
        }

        // FLOODING
        SetupConversionShader(shaderJFFlood, boundingBox);

        numGroups = (sdfSize + 7) / 8;
        bool ping = true;

        for (int jumpDist = sdfSize / 2; jumpDist > 0; jumpDist >>= 1)
        {
            shaderJFFlood->setInt("jump", jumpDist);
            printProgress(sdfSize - jumpDist, sdfSize, "Flooding", window);

            GLuint src = ping ? sdfA : sdfB;
            GLuint dst = ping ? sdfB : sdfA;

            glBindImageTexture(0, src, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
            glBindImageTexture(1, dst, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            DispatchAndWait(numGroups, numGroups, numGroups, false);

            ping = !ping;
        }

        // DISTANCE EVALUATION
        GLuint sdf = ping ? sdfA : sdfB;

        printProgress(0, 1, "Distance", window);
        SetupConversionShader(shaderJFDistance, ssbo_vertex, ssbo_index, boundingBox, topologyLength);

        glBindImageTexture(0, sdf, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
        DispatchAndWait(numGroups, numGroups, numGroups);

        GLuint unused = ping ? sdfB : sdfA;
        glDeleteTextures(1, &unused);

        DetermineSDFSign(sdf, ssbo_vertex, ssbo_index, boundingBox, topologyLength, window);
        clearProgress(window);
        return sdf;
    }

    static unsigned int GenerateSDF_Batty(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &boundingBox, unsigned int topologyLength, GLFWwindow *window)
    {
        GLuint sdf = InitSDF();
        GLuint intersectCounter = InitUintTex();

        // Exact distance calc - Initialisation
        SetupConversionShader(shaderBattyInit, ssbo_vertex, ssbo_index, boundingBox, topologyLength);

        glBindImageTexture(0, sdf, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
        glBindImageTexture(3, intersectCounter, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);

        GLuint numGroups = (topologyLength / 256) + 1;

        for (unsigned int topoIndexStart = 0; topoIndexStart < topologyLength; topoIndexStart += topologyChunksSize)
        {
            shaderBattyInit->setUInt("topoIndexStart", topoIndexStart);
            shaderBattyInit->setUInt("topoIndexEnd", std::min(topoIndexStart + topologyChunksSize, topologyLength));

            DispatchAndWait(numGroups, 1, 1);

            printProgress(topoIndexStart, topologyLength, "Initializing", window);
        }

        // Sweeping passes
        GLuint sdf_alt = InitSDF();

        SetupConversionShader(shaderBattySweep, boundingBox);

        numGroups = (sdfSize + 7) / 8;

        unsigned int maxPass = sdfSize * 2;

        bool ping = false; // Used to switch between read and write SDFs
        GLuint readSDF, writeSDF;
        for (unsigned int pass = 0; pass < maxPass; ++pass)
        {
            readSDF = (ping) ? sdf_alt : sdf;
            writeSDF = (!ping) ? sdf_alt : sdf;
            glBindImageTexture(0, readSDF, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
            glBindImageTexture(1, writeSDF, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

            DispatchAndWait(numGroups, numGroups, numGroups);
            printProgress(pass, maxPass, "Sweeping", window);
            ping = !ping;
        }
        glDeleteTextures(1, &readSDF);
        sdf = writeSDF;

        // Sign fixing
        shaderBattySignDet->use();
        shaderBattySignDet->setInt("textureSize", sdfSize);
        glBindImageTexture(0, sdf, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
        glBindImageTexture(1, intersectCounter, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI);
        printProgress(0, 1, "Sign determination", window);
        DispatchAndWait(1, numGroups, numGroups);

        glDeleteTextures(1, &intersectCounter);

        clearProgress(window);
        return writeSDF;
    }

    static unsigned int GenerateSDF_Raymap(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &boundingBox, unsigned int topologyLength, GLFWwindow *window)
    {
        const uint totalSteps = 16;

        const unsigned long int numElements = sdfSize * sdfSize * sdfSize;

        // --- 1) Voxelisation - Count Intersections

        GLuint intersectBuffer;
        glGenBuffers(1, &intersectBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, intersectBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numElements * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
        glClearBufferData(
            GL_SHADER_STORAGE_BUFFER,
            GL_R32UI,
            GL_RED_INTEGER,
            GL_UNSIGNED_INT,
            nullptr);

        SetupConversionShader(shaderRMIntersectCount, ssbo_vertex, ssbo_index, boundingBox, topologyLength, true);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, intersectBuffer);

        GLuint numGroups = (topologyLength / 64) + 1;

        printProgress(0, totalSteps, "Voxelization", window);
        DispatchAndWait(numGroups, 1, 1);

        // --- 2) Blelloch scan full
        const unsigned int groupSize = 1024;
        const unsigned int blockSize = groupSize * 2; // 2048

        unsigned long int n0 = numElements;
        unsigned long int n1 = (n0 + blockSize - 1) / blockSize;
        unsigned long int n2 = (n1 + blockSize - 1) / blockSize;

        // 1. Create Level 1 and Level 2 buffers
        GLuint blockSumL1, blockSumL2;
        glGenBuffers(1, &blockSumL1);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockSumL1);
        glBufferData(GL_SHADER_STORAGE_BUFFER, n1 * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &blockSumL2);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockSumL2);
        glBufferData(GL_SHADER_STORAGE_BUFFER, n2 * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        // 2. Clear buffers (Crucial for safety)
        GLuint zero = 0;
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockSumL1);
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

        // --- PASS 1: Scan L0 -> Store block sums in L1 ---
        shaderRMBlellochScan->use();
        shaderRMBlellochScan->setBool("writeBlockSums", true);
        shaderRMBlellochScan->setUInt("totalSize", n0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, intersectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, blockSumL1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        DispatchAndWait(n1, 1, 1);

        // --- PASS 2: Scan L1 -> Store block sums in L2 ---
        shaderRMBlellochScan->setUInt("totalSize", n1);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, blockSumL1);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, blockSumL2);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        DispatchAndWait(n2, 1, 1);

        // --- PASS 3: Final Scan of L2 ---
        // Since n2 is small (32 for 512^3), we only need 1 group.
        // We don't need to write block sums here.
        shaderRMBlellochScan->setBool("writeBlockSums", false);
        shaderRMBlellochScan->setUInt("totalSize", n2);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, blockSumL2);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        DispatchAndWait(1, 1, 1);

        // --- PASS 4: Add L2 offsets into L1 ---
        shaderRMBlellochOffset->use();
        shaderRMBlellochOffset->setUInt("totalSize", n1);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, blockSumL1); // Target
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, blockSumL2); // Source
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        // We need to dispatch enough groups to cover all elements in n1
        DispatchAndWait((n1 + 1023) / 1024, 1, 1);

        // --- PASS 5: Add L1 offsets into L0 ---
        shaderRMBlellochOffset->setUInt("totalSize", n0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, intersectBuffer); // Target
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, blockSumL1);      // Source
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        // We need to dispatch enough groups to cover all elements in n0
        DispatchAndWait((n0 + 1023) / 1024, 1, 1);

        glDeleteBuffers(1, &blockSumL1);
        glDeleteBuffers(1, &blockSumL2);

        // --- 5) Find required size and reserve space

        printProgress(4, totalSteps, "Get size", window);
        uint32_t reqSize;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, intersectBuffer);
        glGetBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            (numElements - 1) * sizeof(uint32_t), // offset
            sizeof(uint32_t),
            &reqSize);

        GLuint indexBuffer;
        glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, reqSize * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        // --- 6) Intersect with indeces

        SetupConversionShader(shaderRMIndexing, ssbo_vertex, ssbo_index, boundingBox, topologyLength, true);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, intersectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, indexBuffer);
        numGroups = (topologyLength + 63) / 64;
        printProgress(5, totalSteps, "Voxelization 2", window);
        DispatchAndWait(numGroups, 1, 1);

        // --- 7) Initialize Distances

        printProgress(6, totalSteps, "Close distances", window);
        GLuint distanceBuffer;
        glGenBuffers(1, &distanceBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, distanceBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numElements * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        GLuint faceIDBuffer;
        glGenBuffers(1, &faceIDBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, faceIDBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numElements * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        SetupConversionShader(shaderRMDistInit, ssbo_vertex, ssbo_index, boundingBox, topologyLength, true);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, intersectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, indexBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, distanceBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, faceIDBuffer);

        numGroups = (sdfSize + 7) / 8;
        DispatchAndWait(numGroups, numGroups, numGroups);

        // --- 8) Distance Transform passes

        glm::vec3 voxelSize = (boundingBox[1] - boundingBox[0]) / (float)sdfSize;
        glm::vec3 sqVoxelSize = voxelSize * voxelSize;

        GLuint faceIDBufferOut;
        glGenBuffers(1, &faceIDBufferOut);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, faceIDBufferOut);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numElements * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        shaderRMDistTransform->use();
        shaderRMDistTransform->setUInt("textureSize", sdfSize);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, distanceBuffer);

        shaderRMDistTransform->setFloat("invVoxelSizeSq", 1.0 / sqVoxelSize.x);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, faceIDBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, faceIDBufferOut);
        shaderRMDistTransform->setUInt("sweepDimension", 0);
        printProgress(7, totalSteps, "Distance Transform X", window);
        DispatchAndWait(sdfSize, sdfSize, 1); // X sweep

        shaderRMDistTransform->setFloat("invVoxelSizeSq", 1.0 / sqVoxelSize.y);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, faceIDBufferOut);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, faceIDBuffer);
        shaderRMDistTransform->setUInt("sweepDimension", 1);
        printProgress(8, totalSteps, "Distance Transform Y", window);
        DispatchAndWait(sdfSize, sdfSize, 1); // Y sweep

        shaderRMDistTransform->setFloat("invVoxelSizeSq", 1.0 / sqVoxelSize.z);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, faceIDBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, faceIDBufferOut);
        shaderRMDistTransform->setUInt("sweepDimension", 2);
        printProgress(9, totalSteps, "Distance Transform Z", window);
        DispatchAndWait(sdfSize, sdfSize, 1); // Z sweep

        // --- 9) Transform buffers into 3D tex, also pulls sqrt from the distances

        printProgress(10, totalSteps, "Buffer to Texture", window);
        shaderRMBuffer2SDF->use();
        shaderRMBuffer2SDF->setUInt("textureSize", sdfSize);
        GLuint sdf = InitSDF();
        glBindImageTexture(0, sdf, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, distanceBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, faceIDBufferOut);

        numGroups = (sdfSize + 7) / 8;
        DispatchAndWait(numGroups, numGroups, numGroups);

        // --- 10) Distance cleanup

        printProgress(11, totalSteps, "Distance Cleanup", window);
        SetupConversionShader(shaderRMDistClean, ssbo_vertex, ssbo_index, boundingBox, topologyLength, false);
        DispatchAndWait(numGroups, numGroups, numGroups);

        // --- 11) raymap local intersects

        printProgress(12, totalSteps, "Local Raycasts", window);
        GLuint raymapLocal;
        glGenTextures(1, &raymapLocal);
        glBindTexture(GL_TEXTURE_3D, raymapLocal);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32I, sdfSize, sdfSize, sdfSize, 0, GL_RGBA_INTEGER, GL_INT, NULL);
        const GLint clearValue[4] = {0, 0, 0, 0};
        glClearTexImage(raymapLocal, 0, GL_RGBA_INTEGER, GL_INT, clearValue);

        SetupConversionShader(shaderRMLocalIntersect, ssbo_vertex, ssbo_index, boundingBox, topologyLength, true);
        shaderRMLocalIntersect->setVec3("voxelSize", voxelSize);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, intersectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, indexBuffer);
        glBindImageTexture(4, raymapLocal, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32I);

        for (int x = 0; x <= 1; x++)
        {
            for (int y = 0; y <= 1; y++)
            {
                for (int z = 0; z <= 1; z++)
                {
                    glm::ivec3 offset = glm::ivec3(x, y, z);
                    shaderRMLocalIntersect->setIVec3("offset", offset);
                    DispatchAndWait(numGroups / 2, numGroups / 2, numGroups / 2);
                }
            }
        }

        // --- 12) Raymap accumulate

        printProgress(13, totalSteps, "Building Raymap", window);
        shaderRMIntersectAccumulate->use();
        shaderRMIntersectAccumulate->setInt("textureSize", sdfSize);
        glBindImageTexture(0, raymapLocal, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32I);

        for (uint dir = 0; dir <= 2; dir++)
        {
            shaderRMIntersectAccumulate->setUInt("accAxis", dir);
            DispatchAndWait(numGroups, numGroups, 1);
        }

        // --- 13) Raymap initial cut count

        printProgress(14, totalSteps, "Sign Determination Preparation", window);
        shaderRMInitCuts->use();
        glBindImageTexture(0, raymapLocal, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32I);
        glBindImageTexture(1, sdf, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
        DispatchAndWait(numGroups, numGroups, numGroups);

        // --- 14) Sign determination vote

        printProgress(15, totalSteps, "Sign Determination Vote", window);
        shaderRMSignDet->use();
        glBindImageTexture(0, raymapLocal, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32I);
        glBindImageTexture(1, sdf, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
        DispatchAndWait(numGroups, numGroups, numGroups);

        // --- 15) Clean up

        printProgress(16, totalSteps, "Cleanup", window);
        glDeleteTextures(1, &raymapLocal);
        glDeleteBuffers(1, &intersectBuffer);
        glDeleteBuffers(1, &indexBuffer);
        glDeleteBuffers(1, &distanceBuffer);
        glDeleteBuffers(1, &faceIDBuffer);
        glDeleteBuffers(1, &faceIDBufferOut);
        clearProgress(window);

        return sdf;
    }

    namespace metric
    {
        // Metric calculation shaders
        Shader *shaderMetricCompute;
        Shader *shaderMetricCollapse3D;
        Shader *shaderMetricCollapse2D;

        /// @brief
        /// @param trueSDF The texture ID of the SDF that is assumed to be correct
        /// @param checkSDF The texture ID of the SDF of which the metrics should be calculated
        /// @return r = squared error, g = maximum absolute error, b = sign accuracy, a = face accuracy
        static glm::vec4 ComputeMetrics(GLuint trueSDF, GLuint checkSDF)
        {
            GLuint numGroups = (sdfSize + 7) / 8;
            GLuint metricTensor = InitSDF();

            // Calculate metrics for each voxel
            shaderMetricCompute->use();
            shaderMetricCompute->setInt("textureSize", sdfSize);
            glBindImageTexture(0, trueSDF, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
            glBindImageTexture(1, checkSDF, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
            glBindImageTexture(2, metricTensor, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            DispatchAndWait(numGroups, numGroups, numGroups, false);

            // Collapse calculated metrics onto x-y plane | z=0
            shaderMetricCollapse3D->use();
            shaderMetricCollapse3D->setInt("textureSize", sdfSize);
            glBindImageTexture(0, metricTensor, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
            DispatchAndWait(numGroups, numGroups, 1, false);

            // Collapse into x-Axis | y=0, z=0
            numGroups = (sdfSize + 7) / 32;
            shaderMetricCollapse2D->use();
            shaderMetricCollapse2D->setInt("textureSize", sdfSize);
            glBindImageTexture(0, metricTensor, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
            DispatchAndWait(numGroups, 1, 1, false);

            // Copy x-Axis to CPU memory
            std::vector<glm::vec4> xSlice(sdfSize);

            glGetTextureSubImage(
                metricTensor,
                0,
                0, 0, 0,       // x,y,z offset
                sdfSize, 1, 1, // width,height,depth
                GL_RGBA,
                GL_FLOAT,
                sizeof(glm::vec4) * sdfSize,
                xSlice.data());

            glDeleteTextures(1, &metricTensor);

            // Iterate over x-Axis to get the final value
            glm::vec4 finalMetric = glm::vec4(0.0);
            for (int x = 0; x < sdfSize; x++)
            {
                finalMetric.r += xSlice[x].r;
                finalMetric.g = std::max(xSlice[x].g, finalMetric.g);
                finalMetric.b += xSlice[x].b;
                finalMetric.a += xSlice[x].a;
            }
            int voxelCount = sdfSize * sdfSize * sdfSize;
            finalMetric.r /= voxelCount;
            finalMetric.r = sqrt(std::max(finalMetric.r, 0.0f));
            finalMetric.b /= voxelCount;
            finalMetric.a /= voxelCount;
            return finalMetric;
        }

        static void CompileMetricShaders()
        {
            shaderMetricCompute = new Shader("./shaders/metric/calculate.comp");
            shaderMetricCollapse3D = new Shader("./shaders/metric/collapse3D.comp");
            shaderMetricCollapse2D = new Shader("./shaders/metric/collapse2D.comp");
        }
    }
}
