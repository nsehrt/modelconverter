#include "modelconverter.h"
#include <functional>

bool ModelConverter::process(const InitData& initData)
{
    Assimp::Importer importer;

    auto startTime = std::chrono::high_resolution_clock::now();

    std::cout.precision(4);
    std::cout << std::fixed;

    /*extract base file name*/
    char id[1024];
    _splitpath_s(initData.fileName.c_str(), NULL, 0, NULL, 0, id, 1024, NULL, 0);
    model.name = id;
    model.fileName = initData.prefix + id;

    /*load scene*/
    const unsigned int ppsteps = aiProcess_CalcTangentSpace | // calculate tangents and bitangents if possible
        aiProcess_JoinIdenticalVertices | // join identical vertices/ optimize indexing
        aiProcess_ValidateDataStructure | // perform a full validation of the loader's output
        aiProcess_ImproveCacheLocality | // improve the cache locality of the output vertices
        aiProcess_RemoveRedundantMaterials | // remove redundant materials
        aiProcess_FindDegenerates | // remove degenerated polygons from the import
        aiProcess_FindInvalidData | // detect invalid model data, such as invalid normal vectors
        aiProcess_GenUVCoords | // convert spherical, cylindrical, box and planar mapping to proper UVs
        aiProcess_TransformUVCoords | // preprocess UV transformations (scaling, translation ...)
        aiProcess_FindInstances | // search for instanced meshes and remove them by references to one master
        aiProcess_LimitBoneWeights | // limit bone weights to 4 per vertex
        aiProcess_OptimizeMeshes | // join small meshes, if possible;
        0;

    const aiScene* scene = importer.ReadFile(initData.fileName,
                                             ppsteps | /* configurable pp steps */
                                             aiProcess_GenSmoothNormals | // generate smooth normal vectors if not existing
                                             aiProcess_SplitLargeMeshes | // split large, unrenderable meshes into submeshes
                                             aiProcess_Triangulate | // triangulate polygons with more than 3 edges
                                             aiProcess_ConvertToLeftHanded | // convert everything to D3D left handed space
                                             aiProcess_SortByPType | // make 'clean' meshes which consist of a single typ of primitives
                                             0);

    if (!scene)
    {
        std::cerr << "Unable to load specified file: " << initData.fileName << "!\n";
        return false;
    }

    /*load model*/
    if (!load(scene, initData))
    {
        std::cerr << "Failed to load model!" << std::endl;
        return false;
    }

    /*write model*/
    if (!write())
    {
        std::cerr << "Failed to write model to " << model.fileName << "!" << std::endl;
        return false;
    }

    /*write animations*/
    if (!model.animations.empty())
    {
        if (!writeAnimations())
        {
            std::cout << "\n===================================================\n";
            std::cerr << "\nFailed to write animations." << std::endl;
        }
    }
    else
    {
        std::cout << "\n===================================================\n";
        std::cout << "\nNo animations to write." << std::endl;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Finished processing file in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms." << std::endl;
    std::cout << "\n===================================================\n\n";

    return true;
}

bool ModelConverter::load(const aiScene* scene, const InitData& initData)
{
    aiVector3D vMin = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
    aiVector3D vMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    std::cout << initData << "\n===================================================\n\n";

    model.meshes.reserve(scene->mNumMeshes);
    std::cout << "Model contains " << scene->mNumMeshes << " mesh(es)!\n" << std::endl;

    /*check for bones*/
    std::vector<UINT> totalWeight(scene->mNumMeshes);
    std::vector<float> totalWeightSum(scene->mNumMeshes);

    for (UINT i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh* fMesh = scene->mMeshes[i];

        for (UINT j = 0; j < fMesh->mNumBones; j++)
        {
            totalWeight[i] += fMesh->mBones[j]->mNumWeights;

            for (UINT k = 0; k < fMesh->mBones[j]->mNumWeights; k++)
            {
                totalWeightSum[i] += fMesh->mBones[j]->mWeights[k].mWeight;
            }

            if (existsBoneByName(model.bones, fMesh->mBones[j]->mName.C_Str()))
            {
                continue;
            }

            Bone b;

            b.bone = fMesh->mBones[j];
            b.name = fMesh->mBones[j]->mName.C_Str();
            b.index = j;

            model.bones.push_back(b);
        }
    }

    /*delete bones to force static model*/
    if (initData.forceStatic) model.bones.clear();

    model.isRigged = !model.bones.empty();

    for (UINT i = 0; i < scene->mNumMeshes; i++)
    {
        std::cout << std::fixed << "Mesh " << scene->mMeshes[i]->mName.C_Str() << " has a total of " << totalWeight[i] << " Weights (" << totalWeightSum[i] << ")" << std::endl;
    }

    std::cout << "\nFound a total of " << model.bones.size() << " bones." << std::endl;

    /*print node hierarchy*/

    aiNode* rootNode = scene->mRootNode;
    model.rootNode = rootNode;

    std::cout << "\n===================================================\n";

    std::cout << "\nNode hierarchy:\n\n";
    printAINodes(rootNode);
    std::cout << std::endl;

    /*calculate bone hierarchy*/
    for (auto& b : model.bones)
    {
        aiNode* fNode = rootNode->FindNode(b.name.c_str());

        if (fNode)
        {
            b.parentIndex = findParentBone(model.bones, fNode);
            if (b.parentIndex >= 0)
                b.parentName = model.bones[b.parentIndex].name;
        }
        else
        {
            b.parentIndex = -1;
        }
    }

    for (int i = 0; i < model.bones.size(); i++)
    {
        model.bones[i].parentIndex = findIndexInBones(model.bones, model.bones[i].parentName);
    }

    model.boneHierarchy.resize(model.bones.size());

    /*find root bone and put it in first spot of the bone hierarchy*/
    std::string rootBoneName;
    int rootFound = 0;

    for (const auto& b : model.bones)
    {
        if (b.parentIndex == -1)
        {
            model.boneHierarchy[0] = std::pair<int, int>(b.index, b.parentIndex);
            rootBoneName = b.bone->mName.C_Str();
            rootFound++;
        }
    }

    if (rootFound != 1 && model.isRigged)
    {
        std::cerr << "Can not find root bone or there are more than one root bone!" << std::endl;
        return false;
    }

    if (model.isRigged)
    {
        int bonesUsed = 1;

        while (bonesUsed != model.bones.size())
        {
            for (auto& b : model.bones)
            {
                if (!b.used)
                {
                    if (isInHierarchy(b.parentIndex, model.boneHierarchy))
                    {
                        model.boneHierarchy[bonesUsed] = std::pair<int, int>(b.index, b.parentIndex);
                        b.used = true;
                        bonesUsed++;
                    }
                }
            }
        }

        /*test if bone hierarchy is correct*/
        std::vector<int> testHierarchy;
        testHierarchy.push_back(-1);

        for (const auto& v : model.boneHierarchy)
        {
            if (isInVector(testHierarchy, v.second))
            {
                testHierarchy.push_back(v.first);
            }
            else
            {
                std::cerr << "Bone hierarchy error!\n";
                return false;
            }
        }

        std::cout << "Bone hierarchy test successful.\n";
    }

    std::cout << "\n===================================================\n\n";

    /*load animation*/

    if (model.isRigged)
    {
        model.animations.resize(scene->mNumAnimations);
        std::cout << "\n";

        for (UINT k = 0; k < scene->mNumAnimations; k++)
        {
            auto anim = scene->mAnimations[k];
            float animTicks = (float)anim->mTicksPerSecond;
            model.animations[k].name = anim->mName.C_Str();

            if (model.animations[k].name == "")
            {
                model.animations[k].name = model.name + "_Animation";
            }

            std::cout << "Animation " << model.animations[k].name << ": " << anim->mDuration / anim->mTicksPerSecond << "s (" << anim->mTicksPerSecond << " tick rate) animates " << anim->mNumChannels << " nodes.\n";

            /*get rid of | character*/
            std::replace(model.animations[k].name.begin(), model.animations[k].name.end(), '|', '_');
            model.animations[k].keyframes.resize(model.bones.size());

            for (UINT p = 0; p < anim->mNumChannels; p++)
            {
                auto channel = anim->mChannels[p];

                /*figure out how many keyframes*/
                int numKeyFrames = std::max(channel->mNumPositionKeys, channel->mNumRotationKeys);
                int nodeIndex = findIndexInBones(model.bones, channel->mNodeName.C_Str());
                if (nodeIndex < 0) continue;

                model.animations[k].keyframes[nodeIndex].resize(numKeyFrames);

                for (int m = 0; m < numKeyFrames; m++)
                {
                    KeyFrame keyFrame;

                    keyFrame.timeStamp = (float)channel->mRotationKeys[m].mTime / animTicks;

                    /*keep data from previous keyframe if there are no new data points
                    */
                    if (m >= (int)channel->mNumRotationKeys)
                    {
                        keyFrame.rotationQuat = channel->mRotationKeys[channel->mNumRotationKeys - 1].mValue;
                    }
                    else
                    {
                        keyFrame.rotationQuat = channel->mRotationKeys[m].mValue;
                    }

                    if (m >= (int)channel->mNumPositionKeys)
                    {
                        keyFrame.translation = channel->mPositionKeys[channel->mNumPositionKeys - 1].mValue;
                    }
                    else
                    {
                        keyFrame.translation = channel->mPositionKeys[m].mValue;
                    }

                    model.animations[k].keyframes[nodeIndex][m] = keyFrame;
                }
            }

            /*fill empty bones with identity keyframe*/
            for (int i = 0; i < (int)model.animations[k].keyframes.size(); i++)
            {
                if (model.animations[k].keyframes[i].empty())
                {
                    model.animations[k].keyframes[i].push_back(KeyFrame());
                    model.animations[k].keyframes[i][0].isEmpty = true;
                }
            }

            std::cout << "\n---------------------------------------------------\n\n";
        }
    }

    std::cout << std::endl;

    /*load meshes*/
    for (UINT j = 0; j < scene->mNumMeshes; j++)
    {
        aiMesh* mesh = scene->mMeshes[j];

        /*reserve memory for vertices*/
        model.meshes.push_back(UnifiedMesh());
        model.meshes[j].vertices.reserve(mesh->mNumVertices);

        std::cout << "Mesh " << j << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices and " << mesh->mNumFaces << " faces.\n" << std::endl;

        std::cout << "Input name of material for " << mesh->mName.C_Str() << ": ";
        std::getline(std::cin, model.meshes[j].materialName);

        if (j > 0 && model.meshes[j].materialName == "")
        {
            model.meshes[j].materialName = model.meshes[(INT_PTR)j - 1].materialName;
        }

        if (model.meshes[j].materialName == "del")
        {
            model.meshes.pop_back();
            continue;
        }

        /*get vertices: positions normals tex coords and tangentu */

        for (UINT v = 0; v < mesh->mNumVertices; v++)
        {
            aiVector3D tex(0);
            aiVector3D tangU(0);
            aiVector3D pos = mesh->mVertices[v];
            aiVector3D norm = mesh->mNormals[v];

            if (mesh->HasTangentsAndBitangents() != 0)
            {
                tangU = mesh->mTangents[v];
            }

            if (mesh->HasTextureCoords(0) != 0)
            {
                tex = mesh->mTextureCoords[0][v];
            }

            /*convert to vertex data format*/
            model.meshes[j].vertices.push_back(Vertex(pos, tex, norm, tangU));

            if (pos < vMin)
            {
                vMin = pos;
            }

            if (vMax < pos)
            {
                vMax = pos;
            }
        }

        /*load transformation from node*/
        aiNode* trfNode = scene->mRootNode->FindNode(mesh->mName.C_Str());

        if (trfNode)
        {
            model.meshes[j].rootTransform = getGlobalTransform(trfNode);
        }
        else
        {
            std::cout << "\nCouldn't find the associated node!\n";
            model.meshes[j].rootTransform = aiMatrix4x4();

            for (int i = 0; i < (int)scene->mRootNode->mNumChildren; i++)
            {
                std::string nodeName = scene->mRootNode->mChildren[i]->mName.C_Str();
                std::string userInput;

                std::cout << "Do you want to use node \"" << scene->mRootNode->mChildren[i]->mName.C_Str() << "\" instead? (y/n)\n";
                std::cin >> userInput;

                if (userInput == "y" || userInput == "yes")
                {
                    model.meshes[j].rootTransform = getGlobalTransform(scene->mRootNode->mChildren[i]);
                    break;;
                }

            }
        }

        if (model.meshes[j].rootTransform.IsIdentity())
        {
            std::cout << "Root transform = Identity matrix\n";
        }

        std::cout << "\n---------------------------------------------------\n\n";

        /*get indices*/
        model.meshes[j].indices.reserve((long long)(mesh->mNumFaces) * 3);

        for (UINT k = 0; k < mesh->mNumFaces; k++)
        {
            model.meshes[j].indices.push_back(mesh->mFaces[k].mIndices[0]);
            model.meshes[j].indices.push_back(mesh->mFaces[k].mIndices[1]);
            model.meshes[j].indices.push_back(mesh->mFaces[k].mIndices[2]);
        }
    }

    /*add weights*/
    if (model.isRigged)
    {
        /*add weights from bones to vertices*/
        for (const auto& b : model.bones)
        {
            for (UINT k = 0; k < b.bone->mNumWeights; k++)
            {
                model.meshes[0].vertices[b.bone->mWeights[k].mVertexId].BlendIndices.push_back(b.index);
                model.meshes[0].vertices[b.bone->mWeights[k].mVertexId].BlendWeights.push_back(b.bone->mWeights[k].mWeight);
            }
        }

        /*check weight validity*/
        for (auto& v : model.meshes[0].vertices)
        {
            if (v.BlendIndices.size() > 4)
            {
                std::cout << "Illegal amount of blend indices!" << std::endl;
            }

            float acc = 0.0f;

            for (const auto& f : v.BlendWeights)
            {
                acc += f;
            }

            if (acc > 1.01f)
            {
                std::cout << "Blend Weight sum over 1! " << acc << std::endl;
            }

            if (acc < 1.0f)
            {
                float add = (1.0f - acc) / 4.0f;

                for (auto& f : v.BlendWeights)
                {
                    f += add;
                }
            }
        }
    }

    /*apply centering and scaling if needed*/

    aiVector3D center = 0.5f * (vMin + vMax);

    if (initData.centerEnabled && !model.isRigged)
    {
        std::cout << "\nCentering at " << center.x << " | " << center.y << " | " << center.z << ".." << std::endl;
    }
    if (initData.scaleFactor != 1.0f)
    {
        std::cout << "Scaling with factor " << initData.scaleFactor << ".." << std::endl;
    }

    for (auto& m : model.meshes)
    {
        for (auto& v : m.vertices)
        {
            if (initData.centerEnabled && !model.isRigged)
            {
                v.Position -= center;
            }

            if (initData.scaleFactor != 1.0f)
            {
                v.Position *= initData.scaleFactor;
            }

            if (!model.isRigged || initData.forceTransform)
            {
                v.Position = m.rootTransform * v.Position;
                v.Normal = m.rootTransform * v.Normal;
                v.TangentU = m.rootTransform * v.TangentU;
            }
        }
    }

    std::cout << "\nFinished loading file.\n";
    std::cout << "\n===================================================\n\n";

    return true;
}

bool ModelConverter::write()
{
    if (model.isRigged)
    {
        model.fileName += ".s3d";
    }
    else
    {
        model.fileName += ".b3d";
    }

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    auto fileHandle = std::fstream(model.fileName, std::ios::out | std::ios::binary);

    if (!fileHandle.is_open())
    {
        return false;
    }

    /*header*/
    if (model.isRigged)
    {
        char header[4] = { 0x73, 0x33, 0x64, 0x66 };
        fileHandle.write(header, 4);
    }
    else
    {
        char header[4] = { 0x62, 0x33, 0x64, 0x66 };
        fileHandle.write(header, 4);
    }

    /*bone data only in s3d*/
    if (model.isRigged)
    {
        /*number of bones*/
        char boneSize = (char)model.bones.size();
        fileHandle.write(reinterpret_cast<const char*>(&boneSize), sizeof(char));

        for (const auto& b : model.bones)
        {
            /*bone id*/
            char boneID = (char)b.index;
            fileHandle.write(reinterpret_cast<const char*>(&boneID), sizeof(char));

            /*bone name*/
            short boneStrSize = (short)b.name.size();
            fileHandle.write(reinterpret_cast<const char*>(&boneStrSize), sizeof(boneStrSize));
            fileHandle.write(reinterpret_cast<const char*>(&b.name[0]), boneStrSize);

            fileHandle.write(reinterpret_cast<const char*>(&b.bone->mOffsetMatrix.Transpose()), sizeof(aiMatrix4x4));
        }

        /*bone hierarchy pairs*/
        for (const auto& b : model.boneHierarchy)
        {
            fileHandle.write(reinterpret_cast<const char*>(&b.first), sizeof(int));
            fileHandle.write(reinterpret_cast<const char*>(&b.second), sizeof(int));
        }

        /*complete node tree*/
        std::function<void(aiNode*, int)> writeTree = [&](aiNode* node, int depth) -> void
        {
            std::cout << std::string((long long)depth * 2, ' ') << ">> writing " << node->mName.C_Str() << "\n";

            /*name*/
            short nodeNameSize = (short)node->mName.length;
            fileHandle.write(reinterpret_cast<const char*>(&nodeNameSize), sizeof(nodeNameSize));
            fileHandle.write(reinterpret_cast<const char*>(&node->mName.C_Str()[0]), nodeNameSize);

            /*transform*/
            fileHandle.write(reinterpret_cast<const char*>(&node->mTransformation.Transpose()), sizeof(aiMatrix4x4));

            /*number of children*/
            fileHandle.write(reinterpret_cast<const char*>(&node->mNumChildren), sizeof(unsigned int));

            for (int i = 0; i < (int)node->mNumChildren; i++)
            {
                writeTree(node->mChildren[i], depth + 1);
            }
        };

        writeTree(model.rootNode, 0);
    }

    /*mesh data for both formats*/

    /*number of meshes*/
    char meshSize = (char)model.meshes.size();
    fileHandle.write(reinterpret_cast<const char*>(&meshSize), sizeof(char));

    for (char i = 0; i < meshSize; i++)
    {
        /*material name*/
        short stringSize = (short)model.meshes[i].materialName.size();
        fileHandle.write(reinterpret_cast<const char*>(&stringSize), sizeof(stringSize));
        fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].materialName[0]), stringSize);

        /*num vertices*/
        int verticesSize = (int)model.meshes[i].vertices.size();
        fileHandle.write(reinterpret_cast<const char*>(&verticesSize), sizeof(int));

        /*vertices*/
        for (int v = 0; v < verticesSize; v++)
        {
            /*position*/
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Position.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Position.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Position.z), sizeof(float));
            /*texture coordinates*/
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Texture.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Texture.y), sizeof(float));
            /*normals*/
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Normal.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Normal.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Normal.z), sizeof(float));
            /*tangentU*/
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].TangentU.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].TangentU.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].TangentU.z), sizeof(float));

            /*bone weights only for rigged*/
            if (model.isRigged)
            {
                /*fill bone data*/
                while (model.meshes[i].vertices[v].BlendIndices.size() < 4)
                {
                    model.meshes[i].vertices[v].BlendIndices.push_back(0);
                }

                while (model.meshes[i].vertices[v].BlendWeights.size() < 4)
                {
                    model.meshes[i].vertices[v].BlendWeights.push_back(0.0f);
                }

                /*bone indices*/
                for (int k = 0; k < 4; k++)
                {
                    fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].BlendIndices[k]), sizeof(UINT));
                    fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].BlendWeights[k]), sizeof(float));
                }
            }
        }

        /*num indices*/
        int indicesSize = (int)model.meshes[i].indices.size();
        fileHandle.write(reinterpret_cast<const char*>(&indicesSize), sizeof(int));

        /*indices*/
        for (int j = 0; j < indicesSize; j++)
        {
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].indices[j]), sizeof(UINT));
        }
    }

    fileHandle.close();

    std::cout << "\nFinished writing " << model.fileName << "." << std::endl;

    return true;
}

void ModelConverter::printFile(const std::string& fileName, bool verbose)
{
    std::cout.precision(4);
    std::cout << std::fixed;

    std::string::size_type idx;

    idx = fileName.rfind('.');

    if (idx != std::string::npos)
    {
        std::string ext = fileName.substr(idx + 1);

        if (ext == "b3d")
        {
            printB3D(fileName, verbose);
        }
        else if (ext == "s3d")
        {
            printS3D(fileName, verbose);
        }
        else if (ext == "clp")
        {
            printCLP(fileName, verbose);
        }
        else
        {
            std::cerr << "Invalid file!" << std::endl;
        }
    }
    else
    {
        std::cerr << "Invalid file!" << std::endl;
    }
    return;
}

void ModelConverter::printB3D(const std::string& fileName, bool verbose)
{
    std::cout << "Printing B3D file " << fileName << "..\n" << std::endl;
    std::cout << "\n---------------------------------------------------\n\n";

    /*open file*/
    std::streampos fileSize;
    std::ifstream file(fileName, std::ios::binary);

    /*get file size*/
    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize == 0 || !file.good())
    {
        std::cerr << "Can not open file!\n";
        return;
    }

    /*check header*/
    bool header = true;
    char headerBuffer[4] = { 'b','3','d','f' };

    for (int i = 0; i < 4; i++)
    {
        char temp;
        file.read(&temp, sizeof(temp));

        if (temp != headerBuffer[i])
        {
            header = false;
            break;
        }
    }

    if (header == false)
    {
        /*header incorrect*/
        std::cerr << "File contains incorrect header!\n";
        return;
    }

    char numMeshes;
    file.read(&numMeshes, sizeof(numMeshes));

    std::cout << std::showpoint << "Number of meshes: " << (int)numMeshes << "\n\n";

    for (char i = 0; i < numMeshes; i++)
    {
        std::cout << "\n===================================================\n\n";
        std::cout << "Mesh " << (int)i << ":\n\n";

        short slen = 0;
        file.read((char*)(&slen), sizeof(short));

        char* matStr = new char[(INT_PTR)slen + 1];
        file.read(matStr, slen);
        matStr[slen] = '\0';

        std::cout << "Material:\t" << matStr << "\n";

        int vertCount = 0;
        file.read((char*)(&vertCount), sizeof(vertCount));

        std::cout << "VertCount:\t" << vertCount << "\n";

        std::cout << "\n---------------------------------------------------\n\n";

        Vertex vertex;

        for (int j = 0; j < vertCount; j++)
        {
            file.read((char*)(&vertex.Position.x), sizeof(float));
            file.read((char*)(&vertex.Position.y), sizeof(float));
            file.read((char*)(&vertex.Position.z), sizeof(float));

            file.read((char*)(&vertex.Texture.x), sizeof(float));
            file.read((char*)(&vertex.Texture.y), sizeof(float));

            file.read((char*)(&vertex.Normal.x), sizeof(float));
            file.read((char*)(&vertex.Normal.y), sizeof(float));
            file.read((char*)(&vertex.Normal.z), sizeof(float));

            file.read((char*)(&vertex.TangentU.x), sizeof(float));
            file.read((char*)(&vertex.TangentU.y), sizeof(float));
            file.read((char*)(&vertex.TangentU.z), sizeof(float));

            if (verbose)
            {
                std::cout << "Pos: " << vertex.Position.x << " | " << vertex.Position.y << " | " << vertex.Position.z << "\n";
                std::cout << "Tex: " << vertex.Texture.x << " | " << vertex.Texture.y << "\n";
                std::cout << "Nor: " << vertex.Normal.x << " | " << vertex.Normal.y << " | " << vertex.Normal.z << "\n";
                std::cout << "Tan: " << vertex.TangentU.x << " | " << vertex.TangentU.y << " | " << vertex.TangentU.z << "\n";
                std::cout << "\n";
            }
        }

        if (verbose)
            std::cout << "\n---------------------------------------------------\n\n";

        int vInd = 0;
        file.read((char*)(&vInd), sizeof(vInd));

        std::cout << "IndCount:\t" << vInd << "\n";

        std::cout << "\n---------------------------------------------------\n\n";

        int temp = 0;

        for (int j = 0; j < vInd; j++)
        {
            file.read((char*)(&temp), sizeof(int));

            if (verbose)
            {
                if ((j + 1) % 3 != 0)
                {
                    std::cout << temp << ", ";
                }
                else
                {
                    std::cout << temp << "\n";
                }
            }
        }
        std::cout << std::endl;
    }
}

void ModelConverter::printS3D(const std::string& fileName, bool verbose)
{
    std::cout << "Printing S3D file " << fileName << "..\n" << std::endl;
    std::cout << "\n---------------------------------------------------\n\n";

    /*open file*/
    std::streampos fileSize;
    std::ifstream file(fileName, std::ios::binary);

    /*get file size*/
    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize == 0 || !file.good())
    {
        std::cerr << "Can not open file!\n";
        return;
    }

    /*check header*/
    bool header = true;
    char headerBuffer[4] = { 's','3','d','f' };

    for (int i = 0; i < 4; i++)
    {
        char temp;
        file.read(&temp, sizeof(temp));

        if (temp != headerBuffer[i])
        {
            header = false;
            break;
        }
    }

    if (header == false)
    {
        /*header incorrect*/
        std::cerr << "File contains incorrect header!\n";
        return;
    }

    /*num bones*/
    char vNumBones = 0;
    file.read((char*)(&vNumBones), sizeof(char));

    std::cout << std::showpoint << "NumBones: " << (int)vNumBones << std::endl;

    std::cout << "\n---------------------------------------------------\n\n";

    int temp;
    aiMatrix4x4 mOffset;

    std::vector<std::string> boneNames(vNumBones);

    for (char i = 0; i < vNumBones; i++)
    {
        /*id*/
        char x;
        file.read((char*)&x, sizeof(char));
        temp = (int)x;

        /*name length*/
        short slen = 0;
        file.read((char*)(&slen), sizeof(short));

        /*name*/
        char* bname = new char[(INT_PTR)slen + 1];
        file.read(bname, slen);
        bname[slen] = '\0';

        boneNames[temp] = std::string(bname);

        std::cout << "Bone ID:\t" << (int)temp << "\n";
        std::wcout << "Bone Name:\t" << bname << "\n";

        /*matrix*/
        ai_real mOff[16];
        file.read((char*)&mOff[0], sizeof(float) * 16);

        mOffset = aiMatrix4x4(mOff[0], mOff[1], mOff[2], mOff[3],
                              mOff[4], mOff[5], mOff[6], mOff[7],
                              mOff[8], mOff[9], mOff[10], mOff[11],
                              mOff[12], mOff[13], mOff[14], mOff[15]
        );

        if (verbose)
        {
            printAIMatrix(mOffset);
        }

        std::cout << "\n";
    }

    std::vector<int> boneHierarchyCheck(vNumBones);

    for (char i = 0; i < vNumBones; i++)
    {
        int index;

        file.read((char*)(&index), sizeof(int));
        file.read((char*)(&boneHierarchyCheck[index]), sizeof(int));
    }

    std::cout << "\n---------------------------------------------------\n\n";

    for (int i = 0; i < boneHierarchyCheck.size(); i++)
    {
        std::cout << boneNames[i] << " (" << i << ") is child of bone " << (boneHierarchyCheck[i] >= 0 ? boneNames[boneHierarchyCheck[i]] : "-1") << " (" << boneHierarchyCheck[i] << ")" << std::endl;
    }

    std::cout << "\n\n---------------------------------------------------\n\n";

    /*node tree*/
    Node root;

    std::function<void(Node*, Node*)> loadTree = [&](Node* node, Node* parent)
    {
        /*read data*/
        short slen = 0;
        file.read((char*)(&slen), sizeof(short));

        char* nameStr = new char[(INT_PTR)slen + 1];
        file.read(nameStr, slen);
        nameStr[slen] = '\0';

        ai_real mTemp[16];
        file.read((char*)&mTemp[0], sizeof(float) * 16);

        int numChildren = 0;
        file.read((char*)(&numChildren), sizeof(int));

        /*fill node*/
        node->name = nameStr;
        node->transform = aiMatrix4x4(mTemp[0], mTemp[1], mTemp[2], mTemp[3],
                                      mTemp[4], mTemp[5], mTemp[6], mTemp[7],
                                      mTemp[8], mTemp[9], mTemp[10], mTemp[11],
                                      mTemp[12], mTemp[13], mTemp[14], mTemp[15]
        );

        node->parent = parent;

        for (int i = 0; i < numChildren; i++)
        {
            node->children.push_back(Node());
            loadTree(&node->children.back(), node);
        }
    };

    loadTree(&root, nullptr);

    printNodes(&root);

    std::cout << "\n---------------------------------------------------\n\n";

    /*meshes*/
    char numMeshes;
    file.read(&numMeshes, sizeof(numMeshes));

    std::cout << "Number of meshes: " << (int)numMeshes << "\n\n";

    for (char i = 0; i < numMeshes; i++)
    {
        std::cout << "\n===================================================\n\n";
        std::cout << "Mesh " << (int)i << ":\n\n";

        short slen = 0;
        file.read((char*)(&slen), sizeof(short));

        char* matStr = new char[(INT_PTR)slen + 1];
        file.read(matStr, slen);
        matStr[slen] = '\0';

        std::cout << "Material:\t" << matStr << "\n";

        int vertCount = 0;
        file.read((char*)(&vertCount), sizeof(vertCount));

        std::cout << "VertCount:\t" << vertCount << "\n";

        std::cout << "\n---------------------------------------------------\n\n";

        Vertex vertex;

        for (int j = 0; j < vertCount; j++)
        {
            file.read((char*)(&vertex.Position.x), sizeof(float));
            file.read((char*)(&vertex.Position.y), sizeof(float));
            file.read((char*)(&vertex.Position.z), sizeof(float));

            file.read((char*)(&vertex.Texture.x), sizeof(float));
            file.read((char*)(&vertex.Texture.y), sizeof(float));

            file.read((char*)(&vertex.Normal.x), sizeof(float));
            file.read((char*)(&vertex.Normal.y), sizeof(float));
            file.read((char*)(&vertex.Normal.z), sizeof(float));

            file.read((char*)(&vertex.TangentU.x), sizeof(float));
            file.read((char*)(&vertex.TangentU.y), sizeof(float));
            file.read((char*)(&vertex.TangentU.z), sizeof(float));

            vertex.BlendIndices.resize(4);
            vertex.BlendWeights.resize(4);

            for (int k = 0; k < 4; k++)
            {
                file.read((char*)(&vertex.BlendIndices[k]), sizeof(UINT));
                file.read((char*)(&vertex.BlendWeights[k]), sizeof(float));
            }

            if (verbose)
            {
                std::cout << "Vertex Index: " << j << "\n";
                std::cout << "Pos: " << vertex.Position.x << " | " << vertex.Position.y << " | " << vertex.Position.z << "\n";
                std::cout << "Tex: " << vertex.Texture.x << " | " << vertex.Texture.y << "\n";
                std::cout << "Nor: " << vertex.Normal.x << " | " << vertex.Normal.y << " | " << vertex.Normal.z << "\n";
                std::cout << "Tan: " << vertex.TangentU.x << " | " << vertex.TangentU.y << " | " << vertex.TangentU.z << "\n";
                std::cout << "BlInd: " << vertex.BlendIndices[0] << " | " << vertex.BlendIndices[1] << " | " << vertex.BlendIndices[2] << " | " << vertex.BlendIndices[3] << "\n";
                std::cout << "BlWgt: " << vertex.BlendWeights[0] << " | " << vertex.BlendWeights[1] << " | " << vertex.BlendWeights[2] << " | " << vertex.BlendWeights[3] << "\n";
                std::cout << "\n";
            }
        }

        if (verbose)
            std::cout << "\n---------------------------------------------------\n\n";

        int vInd = 0;
        file.read((char*)(&vInd), sizeof(vInd));

        std::cout << "IndCount:\t" << vInd << "\n";

        std::cout << "\n---------------------------------------------------\n\n";

        int temp = 0;

        for (int j = 0; j < vInd; j++)
        {
            file.read((char*)(&temp), sizeof(int));

            if (verbose)
            {
                if ((j + 1) % 3 != 0)
                {
                    std::cout << temp << ", ";
                }
                else
                {
                    std::cout << temp << "\n";
                }
            }
        }
        std::cout << std::endl;
    }
}

void ModelConverter::printCLP(const std::string& fileName, bool verbose)
{
    std::cout << "Printing CLP file " << fileName << "..\n" << std::endl;
    std::cout << "\n---------------------------------------------------\n\n";

    /*open file*/
    std::streampos fileSize;
    std::ifstream file(fileName, std::ios::binary);

    /*get file size*/
    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize == 0 || !file.good())
    {
        std::cerr << "Can not open file!\n";
        return;
    }

    /*check header*/
    bool header = true;
    char headerBuffer[4] = { 'c','l','p','f' };

    for (int i = 0; i < 4; i++)
    {
        char temp;
        file.read(&temp, sizeof(temp));

        if (temp != headerBuffer[i])
        {
            header = false;
            break;
        }
    }

    if (header == false)
    {
        /*header incorrect*/
        std::cerr << "File contains incorrect header!\n";
        return;
    }

    int slen = 0;
    file.read((char*)(&slen), sizeof(int));

    char* animName = new char[(INT_PTR)slen + 1];
    file.read(animName, slen);
    animName[slen] = '\0';

    std::cout << std::showpoint << "Name:\t" << animName << "\n";

    UINT vNumBones = 0;
    file.read((char*)(&vNumBones), sizeof(int));

    std::cout << "Bones:\t" << vNumBones << "\n";

    for (UINT i = 0; i < vNumBones; i++)
    {
        std::cout << "\n===================================================\n\n";

        UINT vKeyFrames = 0;
        file.read((char*)(&vKeyFrames), sizeof(int));

        if (vKeyFrames == -1)
        {
            std::cout << "Bone " << (int)i << " has no key frames.\n\n";
            continue;
        }

        std::cout << "Bone " << (int)i << " has " << vKeyFrames << " key frames.\n\n";

        for (UINT j = 0; j < vKeyFrames; j++)
        {
            if (verbose)
                std::cout << "Bone " << i << " Keyframe #" << j << "\n";

            float temp, temp2, temp3, temp4;
            file.read((char*)(&temp), sizeof(float));
            if (verbose)
                std::cout << "TimePos:\t" << temp << "\n";

            file.read((char*)(&temp), sizeof(float));
            file.read((char*)(&temp2), sizeof(float));
            file.read((char*)(&temp3), sizeof(float));
            if (verbose)
                std::cout << "Transl:\t" << temp << " | " << temp2 << " | " << temp3 << "\n";

            file.read((char*)(&temp), sizeof(float));
            file.read((char*)(&temp2), sizeof(float));
            file.read((char*)(&temp3), sizeof(float));
            if (verbose)
                std::cout << "Scale:\t" << temp << " | " << temp2 << " | " << temp3 << "\n";

            file.read((char*)(&temp), sizeof(float));
            file.read((char*)(&temp2), sizeof(float));
            file.read((char*)(&temp3), sizeof(float));
            file.read((char*)(&temp4), sizeof(float));
            if (verbose)
                std::cout << "RotQu:\t" << temp << " | " << temp2 << " | " << temp3 << " | " << temp4 << "\n";
            if (verbose)
                std::cout << "\n---------------------------------------------------\n\n";
        }
    }

    file.close();
}

bool ModelConverter::writeAnimations()
{
    std::cout << "\n===================================================\n\n";

    for (const auto& f : model.animations)
    {
        /*writing to binary file .clp*/
        auto startTime = std::chrono::high_resolution_clock::now();

        std::ios_base::sync_with_stdio(false);
        std::cin.tie(NULL);

        std::string clipFile = f.name + ".clp";

        auto fileHandle = std::fstream(clipFile.c_str(), std::ios::out | std::ios::binary);

        if (!fileHandle.is_open())
        {
            std::cerr << "Can not write CLP file " << clipFile << "!\n" << std::endl;
            continue;
        }

        /*header*/
        char header[4] = { 0x63, 0x6c, 0x70, 0x66 };
        fileHandle.write(header, 4);

        int strSize = (int)f.name.size();
        fileHandle.write(reinterpret_cast<const char*>(&strSize), sizeof(int));
        fileHandle.write(reinterpret_cast<const char*>(&f.name[0]), strSize);

        int boneSize = (int)f.keyframes.size();
        fileHandle.write(reinterpret_cast<const char*>(&boneSize), sizeof(int));

        for (int i = 0; i < f.keyframes.size(); i++)
        {
            int keyfrSize = (int)f.keyframes[i].size();

            if (keyfrSize == 1 && f.keyframes[i][0].isEmpty)
            {
                int t = -1;
                fileHandle.write(reinterpret_cast<const char*>(&t), sizeof(int));
                continue;
            }

            fileHandle.write(reinterpret_cast<const char*>(&keyfrSize), sizeof(int));

            for (const auto& kf : f.keyframes[i])
            {
                fileHandle.write(reinterpret_cast<const char*>(&kf.timeStamp), sizeof(float));

                fileHandle.write(reinterpret_cast<const char*>(&kf.translation.x), sizeof(float));
                fileHandle.write(reinterpret_cast<const char*>(&kf.translation.y), sizeof(float));
                fileHandle.write(reinterpret_cast<const char*>(&kf.translation.z), sizeof(float));

                fileHandle.write(reinterpret_cast<const char*>(&kf.scale.x), sizeof(float));
                fileHandle.write(reinterpret_cast<const char*>(&kf.scale.y), sizeof(float));
                fileHandle.write(reinterpret_cast<const char*>(&kf.scale.z), sizeof(float));

                fileHandle.write(reinterpret_cast<const char*>(&kf.rotationQuat.x), sizeof(float));
                fileHandle.write(reinterpret_cast<const char*>(&kf.rotationQuat.y), sizeof(float));
                fileHandle.write(reinterpret_cast<const char*>(&kf.rotationQuat.z), sizeof(float));
                fileHandle.write(reinterpret_cast<const char*>(&kf.rotationQuat.w), sizeof(float));
            }
        }

        fileHandle.close();

        auto endTime = std::chrono::high_resolution_clock::now();
        std::cout << "\nFinished writing " << clipFile << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms." << std::endl;
        std::cout << "\n---------------------------------------------------\n\n";
    }

    return true;
}

int ModelConverter::findParentBone(const std::vector<Bone>& bones, const aiNode* node)
{
    int result = -1;
    aiNode* wNode = node->mParent;

    while (wNode)
    {
        for (int i = 0; i < bones.size(); i++)
        {
            if (bones[i].name == wNode->mName.C_Str())
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

int ModelConverter::findIndexInBones(const std::vector<Bone>& bones, const std::string& name)
{
    for (int i = 0; i < bones.size(); i++)
    {
        if (bones[i].name == name)
        {
            return i;
        }
    }

    return -1;
}

bool ModelConverter::existsBoneByName(const std::vector<Bone>& bones, const std::string& name)
{
    bool exists = false;

    for (auto b : bones)
    {
        if (b.name == name)
            exists = true;
    }

    return exists;
}

bool ModelConverter::isInHierarchy(int index, const std::vector<std::pair<int, int>>& hierarchy)
{
    for (const auto& v : hierarchy)
    {
        if (v.first == index)
        {
            return true;
        }
    }
    return false;
}

bool ModelConverter::isInVector(std::vector<int>& arr, int index)
{
    if (index == -1) return true;

    for (const auto& v : arr)
    {
        if (v == index) return true;
    }

    return false;
}

void ModelConverter::printAINodes(aiNode* node, int depth)
{
    std::cout << std::string((long long)depth * 3, ' ') << char(0xC0) << std::string(2, '-') << ">" << node->mName.C_Str();
    if (node->mTransformation.IsIdentity())
    {
        std::cout << " (Identity Transform)";
    }
    std::cout << "\n";

    for (UINT i = 0; i < node->mNumChildren; i++)
    {
        printAINodes(node->mChildren[i], depth + 1);
    }
}

void ModelConverter::printNodes(Node* node, int depth)
{
    std::cout << std::string((long long)depth * 3, ' ') << char(0xC0) << std::string(2, '-') << ">" << node->name;
    if (node->transform.IsIdentity())
    {
        std::cout << " (Identity Transform)";
    }
    std::cout << "\n";

    if (!node->transform.IsIdentity())
        printAIMatrix(node->transform);

    for (UINT i = 0; i < node->children.size(); i++)
    {
        printNodes(&node->children[i], depth + 1);
    }
}

void ModelConverter::printAIMatrix(const aiMatrix4x4& m)
{
    aiVector3D scale, translation, rotation;
    aiQuaternion quat;

    m.Decompose(scale, rotation, translation);
    m.Decompose(scale, quat, translation);

    std::cout << m.a1 << " | " << m.a2 << " | " << m.a3 << " | " << m.a4 << "\n";
    std::cout << m.b1 << " | " << m.b2 << " | " << m.b3 << " | " << m.b4 << "\n";
    std::cout << m.c1 << " | " << m.c2 << " | " << m.c3 << " | " << m.c4 << "\n";
    std::cout << m.d1 << " | " << m.d2 << " | " << m.d3 << " | " << m.d4 << "\n\n";
}

aiMatrix4x4 ModelConverter::getGlobalTransform(aiNode* node)
{
    aiMatrix4x4 result = node->mTransformation;

    aiNode* tNode = node->mParent;

    while (tNode != nullptr)
    {
        result = tNode->mTransformation * result;
        tNode = tNode->mParent;
    }

    return result;
}