#pragma once

#pragma comment(lib, "assimp-vc142-mt.lib")

#define NOMINMAX
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
#include <cmath>

#include "data.h"

class ModelConverter
{
public:
    explicit ModelConverter() = default;
    ~ModelConverter() = default;

    bool load(const InitData& initData);
    bool load(const aiScene* scene, const InitData& initData);

    bool write();

    void printFile(const std::string& fileName, bool verbose = true);
    void printB3D(const std::string& fileName, bool verbose = true);
    void printS3D(const std::string& fileName, bool verbose = true);
    void printCLP(const std::string& fileName, bool verbose = true);

    std::string getVersionString() const
    {
        std::stringstream res;
        res << aiGetVersionMajor() << "." << aiGetVersionMinor();
        return res.str();
    }

    UnifiedModel model;

private:

    bool writeAnimations();

    static int findParentBone(const std::vector<Bone>& bones, const aiNode* node);
    static int findIndexInBones(const std::vector<Bone>& bones, const std::string& name);
    static bool existsBoneByName(const std::vector<Bone>& bones, const std::string& name);
    static bool isInHierarchy(int index, const std::vector<std::pair<int, int>>& hierarchy);
    static bool isInVector(std::vector<int>& arr, int index);
    static void printNodes(aiNode* node, int depth = 0);
    static void printAIMatrix(const aiMatrix4x4& m);
    static aiMatrix4x4 getGlobalTransform(aiNode* node);
    static void transformXM(DirectX::XMFLOAT3& xmf, DirectX::XMMATRIX trfMatrix);
};