#pragma once

typedef unsigned int UINT;
typedef unsigned char BYTE;


struct uint4
{
    uint4() : x(), y(), z(), w() { RtlSecureZeroMemory(this, sizeof(this)); }
    uint4(UINT _x, UINT _y, UINT _z, UINT _w) : x(_x), y(_y), z(_z), w(_w) {}
    UINT x, y, z, w;
};

struct float4
{
    float4() : x(), y(), z(), w() { RtlSecureZeroMemory(this, sizeof(this)); }
    float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    float x, y, z, w;
};

struct float3
{
    float3() : x(), y(), z() { RtlSecureZeroMemory(this, sizeof(this)); }
    float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    float x, y, z;
};

struct float2
{
    float2(float _u, float _v) : u(_u), v(_v) {}
    float u, v;
};


struct Vertex
{
    Vertex() : Position(0, 0, 0), Texture(0, 0), Normal(0, 0, 0), TangentU(0, 0, 0) {}
    Vertex(float3 p, float2 t, float3 n, float3 tU) : Position(p), Texture(t), Normal(n), TangentU(tU) {}
    float3 Position;
    float2 Texture;
    float3 Normal;
    float3 TangentU;
    std::vector<float> BlendWeights;
    std::vector<UINT> BlendIndices;
};


struct UnifiedMesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    DirectX::XMFLOAT4X4 nodeTransform;
    std::string materialName;
};


struct Bone
{
    std::string name;
    int index = -1;
    aiBone* bone;
    std::string parentName;
    int parentIndex = -1;
    DirectX::XMFLOAT4X4 nodeTransform;
    bool used = false;
};


struct KeyFrame
{
    KeyFrame()
    {
        timeStamp = 0.0f;
        translation = { 0.0f,0.0f,0.0f };
        scale = { 1.0f,1.0f,1.0f };
        XMStoreFloat4(&rotationQuat, DirectX::XMQuaternionIdentity());
    }

    float timeStamp;
    DirectX::XMFLOAT3 translation;
    DirectX::XMFLOAT3 scale;
    DirectX::XMFLOAT4 rotationQuat;
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
    aiMatrix4x4 globalInverse;

    bool isRigged = false;
};


struct InitData
{
    std::string fileName = "";
    float scaleFactor = 1.0f;
    int centerEnabled = 1;
    std::string prefix = "";

    friend std::ostream& operator<<(std::ostream& os, const InitData& id)
    {
        os << "File:\t\t" << id.fileName << "\nScale:\t\t" << id.scaleFactor << "\nCentering:\t" << (id.centerEnabled ? "On" : "Off") <<
              "\nPrefix:\t\t" << (id.prefix.empty() ? "None" : id.prefix) << "\n";
        return os;
    }
};

static std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim))
    {
        result.push_back(item);
    }

    return result;
}