#include "conversion.h"

namespace sdf_conversion
{
    class ConversionMethod
    {
    protected:
        unsigned int ssbo_vertex, ssbo_index;
        glm::mat2x3 bounding_box;
        unsigned int topology_length;
        GLFWwindow *window;
        std::string name, modelname;

        void TryLoadSDF()
        {
            std::string path = "./cache/" + this->name + "/" + this->modelname + ".sdf";
            this->sdf = SDFCache::load_from_file(path.c_str(), sdf_conversion::sdfSize);
            this->sdf_ready = this->sdf != 0;
        }

    public:
        bool sdf_ready;
        unsigned int sdf;
        ConversionMethod(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &bounding_box, unsigned int topology_length, GLFWwindow *window, std::string methodname, std::string modelname)
        {
            this->ssbo_vertex = ssbo_vertex;
            this->ssbo_index = ssbo_index;
            this->bounding_box = bounding_box;
            this->window = window;
            this->topology_length = topology_length;
            this->name = methodname;
            this->modelname = modelname;
            TryLoadSDF();
        }

        virtual ~ConversionMethod() = default;

        void PrepareSDF(bool save_to_cache)
        {
            if (sdf_ready)
                glDeleteTextures(1, &sdf);
            this->GenerateSDF();
            sdf_ready = true;
            if (save_to_cache)
            {
                std::string path = "./cache/" + this->name + "/" + this->modelname + ".sdf";
                SDFCache::save_to_file(path.c_str(), sdf);
            }
        }

        virtual void GenerateSDF() {};
    };

    class NaiveConversion : public ConversionMethod
    {
    public:
        NaiveConversion(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &bounding_box, unsigned int topology_length, GLFWwindow *window, std::string &modelname) : ConversionMethod(ssbo_vertex, ssbo_index, bounding_box, topology_length, window, "naive", modelname) {};
        glm::mat4 model;
        void GenerateSDF() override
        {
            this->sdf = sdf_conversion::GenerateSDF_Naive(ssbo_vertex, ssbo_index, bounding_box, topology_length, window, model);
        }
    };

    class JFConversion : public ConversionMethod
    {
    public:
        JFConversion(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &bounding_box, unsigned int topology_length, GLFWwindow *window, std::string &modelname) : ConversionMethod(ssbo_vertex, ssbo_index, bounding_box, topology_length, window, "jumpflood", modelname) {};
        void GenerateSDF() override
        {
            this->sdf = GenerateSDF_Jumpflood(ssbo_vertex, ssbo_index, bounding_box, topology_length, window);
        }
    };

    class BattyConversion : public ConversionMethod
    {
    public:
        BattyConversion(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &bounding_box, unsigned int topology_length, GLFWwindow *window, std::string &modelname) : ConversionMethod(ssbo_vertex, ssbo_index, bounding_box, topology_length, window, "batty", modelname) {};
        void GenerateSDF() override
        {
            this->sdf = GenerateSDF_Batty(ssbo_vertex, ssbo_index, bounding_box, topology_length, window);
        }
    };

    class RaymapConversion : public ConversionMethod
    {
    public:
        RaymapConversion(unsigned int ssbo_vertex, unsigned int ssbo_index, glm::mat2x3 &bounding_box, unsigned int topology_length, GLFWwindow *window, std::string &modelname) : ConversionMethod(ssbo_vertex, ssbo_index, bounding_box, topology_length, window, "raymap", modelname) {};
        glm::mat4 model;
        void GenerateSDF() override
        {
            this->sdf = GenerateSDF_Raymap(ssbo_vertex, ssbo_index, bounding_box, topology_length, window, model);
        }
    };
}