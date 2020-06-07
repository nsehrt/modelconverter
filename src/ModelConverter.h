#pragma once

#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
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
    std::vector<float> BlendWeights;
    std::vector<UINT> BlendIndices;
};

struct Mesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
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

    friend std::ostream& operator<<(std::ostream& os, const InitData& id)
    {
        os << "Filename: " << id.FileName << "\nScalefactor: " << id.ScaleFactor << "\nCentering: " << id.CenterEnabled <<
            "\nPrefix: " << id.Prefix << "\n";
        return os;
    }
};

class ModelConverter
{
public:
    explicit ModelConverter() = default;
    ~ModelConverter() = default;

    bool load(InitData& initData);

    std::string getVersionString()
    {
        std::stringstream res;
        res << aiGetVersionMajor() << "." << aiGetVersionMinor();
        return res.str();
    }

private:
    Assimp::Importer importer;
    std::vector<Mesh*> meshes, vmeshes;
    std::vector<Bone> bones;

    std::string baseFileName;

};