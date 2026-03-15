#include <filesystem>

namespace benchmarking
{
    using namespace tinyjson;

    void ProcessModel(const std::string &name, Model *model, std::vector<std::string> shaderNames, objectnode *benchmarkroot, objectnode *&previousModelNode, double &totalTime)
    {
        auto *modelnode = new objectnode(name);

        auto *vertsnode = new valuenode<int>("vertices", model->GetModelVertices());
        modelnode->child = vertsnode;
        auto *facesnode = new valuenode<int>("faces", model->GetModelFaces());
        vertsnode->next = facesnode;

        float maxDimension = model->maxExtends;

        const std::vector<unsigned int> testSizes = {32, 64, 128, 256};

        node *previousSizeNode = facesnode;
        for (int size : testSizes)
        {
            auto *sizenode = new objectnode("sdf " + std::to_string(size));

            sdf_conversion::sdfSize = size;

            node *previousShaderNode = sizenode;
            for (string shader : shaderNames)
            {
                if (shader == shaderNames[0])
                    continue;

                auto *shadernode = new objectnode(shader);

                double executionTime = model->BenchmarkShader(shader);
                totalTime += executionTime;

                GLuint newSDF = model->GetSDFByName(shader);
                GLuint baselineSDF = model->GetSDFByName("NAIVE");

                glm::vec4 metrics = sdf_conversion::metric::ComputeMetrics(baselineSDF, newSDF);

                auto *metric_mse = new valuenode<float>("root mean squared error", metrics.r / maxDimension);
                auto *metric_me = new valuenode<float>("maximum absolute error", metrics.g / maxDimension);
                auto *metric_sa = new valuenode<float>("sign accuracy", metrics.b);
                auto *metric_fa = new valuenode<float>("face accuracy", metrics.a);
                auto *metric_et = new valuenode<float>("wall time", executionTime);

                shadernode->child = metric_mse;
                metric_mse->next = metric_me;
                metric_me->next = metric_sa;
                metric_sa->next = metric_fa;
                metric_fa->next = metric_et;

                if (previousShaderNode == sizenode)
                    sizenode->child = shadernode;
                else
                    previousShaderNode->next = shadernode;

                previousShaderNode = shadernode;
            }

            previousSizeNode->next = sizenode;
            previousSizeNode = sizenode;
        }

        if (previousModelNode == benchmarkroot)
            benchmarkroot->child = modelnode;
        else
            previousModelNode->next = modelnode;

        previousModelNode = modelnode;
    }

    void DefaultIterator(std::map<std::string, Model *> modelMap, std::vector<std::string> shaderNames, objectnode *benchmarkroot, double &totalTime)
    {
        auto *previousModelNode = benchmarkroot;

        for (const auto &[name, model] : modelMap)
        {
            if (name == "FLATSCREEN")
                continue;

            ProcessModel(name, model, shaderNames, benchmarkroot, previousModelNode, totalTime);
        }
    }

    /// @returns How many files have been skipped due to errors
    int FileIterator(std::vector<std::string> shaderNames, objectnode *benchmarkroot, GLFWwindow *window, double &totalTime, int count, int start)
    {
        const std::filesystem::path thingy10k{"./Objs/_thingy10k/raw_meshes"};
        auto *previousModelNode = benchmarkroot;

        int skipped = 0;
        int current = 0;
        for (const auto &file : std::filesystem::directory_iterator{thingy10k})
        {
            if (current >= count)
                break;

            if (current < start) {
                current++;
                continue;
            }

            std::string ext = file.path().extension();

            if (ext != ".stl" && ext != ".ply" && ext != ".obj")
            {
                std::cout << "Skipped file with unsupported file extension " << ext << std::endl;
                skipped++;
                current++;
                continue;
            }

            std::string modelPath = file.path();
            std::string name = file.path().stem().string();

            std::cout << "\r" << ++current << "/" << count << " - Now processing " << modelPath << std::endl;

            Model *model = new Model(modelPath, window, shaderNames, name);

            if (model->errors)
            {
                std::cout << "Error loading model, skipping... " << std::endl;
                skipped++;
                delete model;
                continue;
            }

            ProcessModel(name, model, shaderNames, benchmarkroot, previousModelNode, totalTime);
            delete model;
        }
        return skipped;
    }
}