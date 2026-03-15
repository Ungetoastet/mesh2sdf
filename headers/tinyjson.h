#include <string>
#include <sstream>
#include <fstream>
#include <type_traits>

namespace tinyjson
{
    struct node
    {
        std::string key;
        node *next = nullptr;

        virtual ~node() = default;
        virtual std::string ToString(int indent = 0) const = 0;

    protected:
        static std::string Indent(int level)
        {
            return std::string(level * 4, ' ');
        }
    };

    struct objectnode : public node
    {
        node *child = nullptr;

        objectnode() = default;
        objectnode(const std::string &k)
        {
            key = k;
        }

        std::string ToString(int indent = 0) const override
        {
            std::ostringstream ss;

            // Root object (no key)
            if (key.empty())
            {
                const node *current = child;
                while (current)
                {
                    ss << Indent(indent)
                       << current->ToString(indent);

                    if (current->next)
                        ss << ",";

                    ss << "\n";
                    current = current->next;
                }
                return ss.str();
            }

            // Named object
            ss << "\"" << key << "\": {\n";

            const node *current = child;
            while (current)
            {
                ss << Indent(indent + 1)
                   << current->ToString(indent + 1);

                if (current->next)
                    ss << ",";

                ss << "\n";
                current = current->next;
            }

            ss << Indent(indent) << "}";

            return ss.str();
        }
    };

    template <typename T>
    struct valuenode : public node
    {
        T value;

        valuenode(const std::string &k, const T &v)
        {
            key = k;
            value = v;
        }

        std::string ToString(int indent = 0) const override
        {
            std::ostringstream ss;

            ss << "\"" << key << "\": ";

            if constexpr (std::is_same_v<T, std::string>)
                ss << "\"" << value << "\"";
            else
                ss << value;

            return ss.str();
        }
    };

    inline void WriteToFile(const node &root, const std::string &filename)
    {
        std::ofstream file(filename);
        file << "{\n";
        file << root.ToString(1);
        file << "}\n";
    }
}