#pragma once

typedef unsigned int UINT;
typedef unsigned char BYTE;

#include <assimp\Importer.hpp>
#include <vector>

/*holds all data for a skinned vertex*/
struct Vertex
{
    Vertex() : Position(), Texture(), Normal(), TangentU() {}
    Vertex(aiVector3D p, aiVector3D t, aiVector3D n, aiVector3D tU) : Position(p), Texture(t), Normal(n), TangentU(tU) {}
    aiVector3D Position;
    aiVector3D Texture;
    aiVector3D Normal;
    aiVector3D TangentU;
    std::vector<float> BlendWeights;
    std::vector<UINT> BlendIndices;
};

/**/
struct UnifiedMesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;
    aiMatrix4x4 rootTransform;
    std::string materialName;
};

struct Node
{
    std::string name = "";
    aiMatrix4x4 transform;
    Node* parent = nullptr;
    std::vector<Node> children;
};

struct Bone
{
    std::string name;
    int index = -1;
    aiBone* bone = nullptr;
    std::string parentName;
    int parentIndex = -1;
    aiMatrix4x4 nodeTransform;
    bool used = false;
};

struct KeyFrame
{
    bool isEmpty = false;
    bool saveToFile = true;

    KeyFrame()
    {
        timeStamp = 0.0f;
        translation = aiVector3D();
        scale = aiVector3D(1, 1, 1);
        rotationQuat = aiQuaternion();
    }

    float timeStamp;
    aiVector3D translation;
    aiVector3D scale;
    aiQuaternion rotationQuat;
};

struct Animation
{
    std::string name;
    std::vector<std::vector<KeyFrame>> keyframes;
};

struct UnifiedModel
{
    std::string name;
    std::string fileName;
    std::vector<UnifiedMesh> meshes;
    std::vector<Bone> bones;
    std::vector<std::pair<int, int>> boneHierarchy;
    std::vector<Animation> animations;
    aiNode* rootNode = nullptr;

    bool isRigged = false;
};

struct InitData
{
    std::string fileName = "";
    float scaleFactor = 1.0f;
    int centerEnabled = 1;
    std::string prefix = "";
    bool forceStatic = false;
    bool forceTransform = false;

    friend std::ostream& operator<<(std::ostream& os, const InitData& id)
    {
        os << "File:\t\t" << id.fileName << "\nScale:\t\t" << id.scaleFactor << "\nCentering:\t" << (id.centerEnabled ? "On" : "Off") <<
            "\nForce static:\t" << (id.forceStatic ? "On" : "Off") <<
            "\nForce transform:\t" << (id.forceTransform ? "On" : "Off") <<
            "\nPrefix:\t\t" << (id.prefix.empty() ? "None" : id.prefix) << "\n";
        return os;
    }
};

static std::vector<std::string> split(const std::string& str, char del)
{
    std::vector<std::string> result;
    size_t pos = str.find(del);
    size_t initialPos = 0;
    result.clear();

    while (pos != std::string::npos)
    {
        auto s = str.substr(initialPos, pos - initialPos);
        if (s == " " || s == "")
        {
            initialPos = pos + 1;
            pos = str.find(del, initialPos);
            continue;
        }

        result.push_back(s);
        initialPos = pos + 1;

        pos = str.find(del, initialPos);
    }

    result.push_back(str.substr(initialPos, std::min(pos, str.size()) - initialPos + 1));

    return result;
}