#pragma once

#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip> 
#include <Windows.h>
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include <assimp\material.h>
#include <assimp\version.h>
#include <DirectXMath.h>

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
};

struct SkinnedVertex
{
    SkinnedVertex() : Position(0, 0, 0), Texture(0, 0), Normal(0, 0, 0), TangentU(0, 0, 0) {}
    SkinnedVertex(float3 p, float2 t, float3 n, float3 tU) : Position(p), Texture(t), Normal(n), TangentU(tU) {}
    float3 Position;
    float2 Texture;
    float3 Normal;
    float3 TangentU;
    std::vector<float> BlendWeights;
    std::vector<UINT> BlendIndices;
};

struct Mesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    DirectX::XMFLOAT4X4 nodeTransform;
    DirectX::XMFLOAT4X4 nodeRotation;
    std::string materialName;
};

struct MeshRigged
{
public:
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;
    DirectX::XMFLOAT4X4 nodeTransform;
    DirectX::XMFLOAT4X4 nodeRotation;
    std::string materialName;
};

struct Bone
{
    std::string Name;
    aiBone* AIBone;
    int Index = -1;
    std::string ParentName;
    int ParentIndex = -1;
};

struct InitData
{
    std::string FileName = "";
    float ScaleFactor = 1.0f;
    int CenterEnabled = 1;
    std::string Prefix = "";
    int TransformApply = 1;

    friend std::ostream& operator<<(std::ostream& os, const InitData& id)
    {
        os << "File:\t\t" << id.FileName << "\nScale:\t\t" << id.ScaleFactor << "\nCentering:\t" << (id.CenterEnabled ? "On" : "Off") <<
           "\nTransform:\t" << (id.TransformApply ? "On" : "Off") << "\nPrefix:\t\t" << (id.Prefix.empty() ? "None" : id.Prefix) << "\n";
        return os;
    }
};

class ModelConverter
{
public:
    explicit ModelConverter() = default;
    ~ModelConverter() = default;

    bool load(InitData& initData);
    bool loadStatic(const aiScene* scene);
    bool loadRigged(const aiScene* scene);

    bool writeB3D();
    bool writeS3D();

    bool verifyB3D();
    bool verifyS3D();

    std::string getVersionString() const
    {
        std::stringstream res;
        res << aiGetVersionMajor() << "." << aiGetVersionMinor();
        return res.str();
    }

private:
    Assimp::Importer importer;
    std::vector<Mesh*> bMeshes, bMeshesV;
    std::vector<MeshRigged*> rMeshes, rMeshesV;
    std::vector<Bone> bones;
    UINT estimatedFileSize = 0;

    std::string baseFileName = "";
    std::string writeFileName = "";
    InitData iData;
    std::chrono::steady_clock::time_point startTime;


    int findParentBone(std::vector<Bone>& bones, aiNode* node)
    {
        int result = -1;
        aiNode* wNode = node->mParent;

        while (wNode)
        {
            for (int i = 0; i < bones.size(); i++)
            {
                if (bones[i].Name == wNode->mName.C_Str())
                {
                    result = i;
                    goto skip;
                }
            }

            wNode = wNode->mParent;
        }

    skip:
        return result;
    }

    int findIndexInBones(std::vector<Bone>& bones, const std::string& name)
    {
        for (int i = 0; i < bones.size(); i++)
        {
            if (bones[i].Name == name)
            {
                return i;
            }
        }

        return -1;
    }

    bool existsBoneByName(std::vector<Bone>& bones, const std::string& name)
    {
        bool exists = false;

        for (auto b : bones)
        {
            if (b.Name == name)
                exists = true;
        }

        return exists;
    }

    void printNodes(aiNode* node, int depth = 0)
    {
        std::cout << std::string((long long)depth * 3, ' ') << char(0xC0) << std::string(2, '-') << ">" << node->mName.C_Str();
        if (node->mTransformation.IsIdentity())
        {
            std::cout << " (Identity Transform)";
        }
        std::cout <<"\n";

        for (UINT i = 0; i < node->mNumChildren; i++)
        {
            printNodes(node->mChildren[i], depth + 1);
        }
    }

    void transformXM(DirectX::XMFLOAT3& xmf, DirectX::XMMATRIX trfMatrix)
    {
        DirectX::XMVECTOR vec = XMVector3Transform(XMLoadFloat3(&xmf), trfMatrix);
        DirectX::XMStoreFloat3(&xmf, vec);
    }

};