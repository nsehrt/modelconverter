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
#include <cmath>

#include "data.h"

class ModelConverter
{
public:
    explicit ModelConverter() = default;
    ~ModelConverter() = default;

    /*
    Loads the model using ASSIMP, converts it to own data structures and saves it
    as B3D, S3D and CLP files.
    @returns Success status
    @param Initdata holds the user specified preferences and the file name.
    */
    bool process(const InitData& initData);

    /*
    Print the contents of a B3D, S3D or CLP file to the command line.
    @param Path to the file
    @param verbose output*/
    void printFile(const std::string& fileName, bool verbose = true);

    /*
    Returns the ASSIMP version as a string

    @returns ASSIMP version*/
    std::string getVersionString() const
    {
        std::stringstream res;
        res << aiGetVersionMajor() << "." << aiGetVersionMinor();
        return res.str();
    }

private:
    UnifiedModel model;

    bool load(const aiScene* scene, const InitData& initData);
    bool write();
    bool writeAnimations();
    void printB3D(const std::string& fileName, bool verbose = true);
    void printS3D(const std::string& fileName, bool verbose = true);
    void printCLP(const std::string& fileName, bool verbose = true);

    static int findParentBone(const std::vector<Bone>& bones, const aiNode* node);
    static int findIndexInBones(const std::vector<Bone>& bones, const std::string& name);
    static bool existsBoneByName(const std::vector<Bone>& bones, const std::string& name);
    static bool isInHierarchy(int index, const std::vector<std::pair<int, int>>& hierarchy);
    static bool isInVector(std::vector<int>& arr, int index);
    static void printAINodes(aiNode* node, int depth = 0);
    static void printNodes(Node* node, int depth = 0);
    static void printAIMatrix(const aiMatrix4x4& m);
    static aiMatrix4x4 getGlobalTransform(aiNode* node);
};