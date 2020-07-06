#include "modelconverter.h"

using namespace DirectX;


bool ModelConverter::load(const InitData& initData)
{
    Assimp::Importer importer;

    auto startTime = std::chrono::high_resolution_clock::now();

    /*extract base file name*/
    char id[1024];
    _splitpath_s(initData.fileName.c_str(), NULL, 0, NULL, 0, id, 1024, NULL, 0);
    model.name = id;
    model.fileName = initData.prefix + id;

    /*load scene*/
    const aiScene* scene = importer.ReadFile(initData.fileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);

    if (!scene)
    {
        std::cerr << "Unable to load specified file!\n";
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
    XMFLOAT3 cMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    XMFLOAT3 cMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    XMVECTOR vMin = XMLoadFloat3(&cMin);
    XMVECTOR vMax = XMLoadFloat3(&cMax);

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

    model.isRigged = !model.bones.empty();


    for (UINT i = 0; i < scene->mNumMeshes; i++)
    {
        std::cout << std::fixed << "Mesh " << scene->mMeshes[i]->mName.C_Str() << " has a total of " << totalWeight[i] << " Weights (" << totalWeightSum[i] << ")" << std::endl;
    }

    std::cout << "\nFound a total of " << model.bones.size() << " bones." << std::endl;


    /*print node hierarchy*/

    aiNode* rootNode = scene->mRootNode;

    std::cout << "\n===================================================\n";

    std::cout << "\nNode hierarchy:\n\n";
    printNodes(rootNode);
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

        auto rNode = scene->mRootNode->FindNode(rootBoneName.c_str());

        /*load global armature inverse*/
        model.globalInverse = getGlobalTransform(rNode->mParent).Inverse();

        std::cout << "Armature global matrix:\n";
        printAIMatrix(model.globalInverse);
        std::cout << "\n===================================================\n\n";


        model.animations.resize(scene->mNumAnimations);
        std::cout << "\n";

        for (UINT k = 0; k < scene->mNumAnimations; k++)
        {

            auto anim = scene->mAnimations[k];
            float animTicks = (float)anim->mTicksPerSecond;

            std::cout << "Animation " << anim->mName.C_Str() << ": " << anim->mDuration / anim->mTicksPerSecond << "s (" << anim->mTicksPerSecond << " tick rate) has " << anim->mNumChannels << " channels.\n";

            model.animations[k].name = anim->mName.C_Str();
            /*get rid of | character*/
            std::replace(model.animations[k].name.begin(), model.animations[k].name.end(), '|', '_');
            model.animations[k].keyframes.resize(model.bones.size());


            for (size_t x = 0; x < model.animations[k].keyframes.size(); x++)
            {
                model.animations[k].keyframes[x].push_back(KeyFrame());
            }

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


                    aiMatrix4x4 keyMatrix = aiMatrix4x4({ 1,1,1 }, channel->mRotationKeys[m].mValue, channel->mPositionKeys[m].mValue);

                    aiVector3D scale, translation;
                    aiQuaternion rotation;

                    keyMatrix.Decompose(scale, rotation, translation);


                    keyFrame.timeStamp = (float)channel->mRotationKeys[m].mTime / animTicks;

                    keyFrame.rotationQuat = { rotation.x,
                                                rotation.y,
                                                rotation.z,
                                                rotation.w };

                    if (m <= (int)channel->mNumPositionKeys)
                    {
                        keyFrame.timeStamp = (float)channel->mPositionKeys[m].mTime / animTicks;
                        keyFrame.translation = { translation.x,
                            translation.y,
                            translation.z };
                    }

                    model.animations[k].keyframes[nodeIndex][m] = keyFrame;

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

        std::cout << "Mesh " << j << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices and " <<  mesh->mNumFaces << " faces.\n" << std::endl;

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
            model.meshes[j].vertices.push_back(Vertex(float3(pos.x, pos.y, pos.z),
                                           float2(tex.x, tex.y),
                                           float3(norm.x, norm.y, norm.z),
                                           float3(tangU.x, tangU.y, tangU.z)
            ));

            XMFLOAT3 pV3 = { pos.x, pos.y, pos.z };

            vMin = XMVectorMin(vMin, XMLoadFloat3(&pV3));
            vMax = XMVectorMax(vMax, XMLoadFloat3(&pV3));

        }

        /*load transformation from node*/
        aiNode* trfNode = scene->mRootNode->FindNode(mesh->mName.C_Str());

        if (trfNode)
        {

            aiMatrix4x4 matrix = getGlobalTransform(trfNode);

            model.meshes[j].nodeTransform._11 = matrix.a1;
            model.meshes[j].nodeTransform._12 = matrix.a2;
            model.meshes[j].nodeTransform._13 = matrix.a3;
            model.meshes[j].nodeTransform._14 = matrix.a4;
            model.meshes[j].nodeTransform._21 = matrix.b1;
            model.meshes[j].nodeTransform._22 = matrix.b2;
            model.meshes[j].nodeTransform._23 = matrix.b3;
            model.meshes[j].nodeTransform._24 = matrix.b4;
            model.meshes[j].nodeTransform._31 = matrix.c1;
            model.meshes[j].nodeTransform._32 = matrix.c2;
            model.meshes[j].nodeTransform._33 = matrix.c3;
            model.meshes[j].nodeTransform._34 = matrix.c4;
            model.meshes[j].nodeTransform._41 = matrix.d1;
            model.meshes[j].nodeTransform._42 = matrix.d2;
            model.meshes[j].nodeTransform._43 = matrix.d3;
            model.meshes[j].nodeTransform._44 = matrix.d4;

        }
        else
        {
            std::cout << "\nCouldn't find the associated node!\n";

            XMStoreFloat4x4(&model.meshes[j].nodeTransform, XMMatrixIdentity());
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
        for (const auto& v : model.meshes[0].vertices)
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

            if (acc < 0.925f)
            {
                std::cout << "Blend Weight sum under 0.925! " << acc << std::endl;
            }

        }

    }

    /*apply centering and scaling if needed*/

    XMFLOAT3 center;
    DirectX::XMStoreFloat3(&center, 0.5f * (vMin + vMax));

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
                v.Position.x -= center.x;
                v.Position.y -= center.y;
                v.Position.z -= center.z;
            }

            if (initData.scaleFactor != 1.0f)
            {
                DirectX::XMFLOAT3 xm = { v.Position.x, v.Position.y, v.Position.z };
                XMVECTOR a = XMLoadFloat3(&xm);
                a = XMVectorScale(a, initData.scaleFactor);
                DirectX::XMStoreFloat3(&xm, a);

                v.Position.x = xm.x;
                v.Position.y = xm.y;
                v.Position.z = xm.z;
            }


            XMFLOAT3 vec = { v.Position.x, v.Position.y, v.Position.z };
            XMFLOAT3 nVec = { v.Normal.x, v.Normal.y, v.Normal.z };
            XMFLOAT3 tVec = { v.TangentU.x, v.TangentU.y, v.TangentU.z };

            XMMATRIX trf = XMLoadFloat4x4(&m.nodeTransform);
            trf = XMMatrixInverse(&XMMatrixDeterminant(trf), trf);

            transformXM(vec, trf);
            transformXM(nVec, trf);
            transformXM(tVec, trf);

            v.Position = { vec.x, vec.y, vec.z };
            v.Normal = { nVec.x, nVec.y, nVec.z };
            v.TangentU = { tVec.x, tVec.y, tVec.z };


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
        /*global armature inverse*/
        auto wMatrix = model.globalInverse.Transpose(); //!!!

        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.a1), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.a2), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.a3), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.a4), sizeof(float));

        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.b1), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.b2), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.b3), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.b4), sizeof(float));

        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.c1), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.c2), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.c3), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.c4), sizeof(float));

        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.d1), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.d2), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.d3), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&wMatrix.d4), sizeof(float));


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

            /*bone offset matrix*/
            aiMatrix4x4 offsetMatrix = b.bone->mOffsetMatrix.Transpose(); //!!!

            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.a1), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.a2), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.a3), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.a4), sizeof(float));

            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.b1), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.b2), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.b3), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.b4), sizeof(float));

            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.c1), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.c2), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.c3), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.c4), sizeof(float));

            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.d1), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.d2), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.d3), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&offsetMatrix.d4), sizeof(float));


        }

        /*bone hierarchy*/
        for (const auto& b : model.boneHierarchy)
        {
            fileHandle.write(reinterpret_cast<const char*>(&b.first), sizeof(int));
            fileHandle.write(reinterpret_cast<const char*>(&b.second), sizeof(int));
        }
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
            XMFLOAT3 Position;
            Position.x = model.meshes[i].vertices[v].Position.x;
            Position.y = model.meshes[i].vertices[v].Position.y;
            Position.z = model.meshes[i].vertices[v].Position.z;

            XMFLOAT3 Normal;
            Normal.x = model.meshes[i].vertices[v].Normal.x;
            Normal.y = model.meshes[i].vertices[v].Normal.y;
            Normal.z = model.meshes[i].vertices[v].Normal.z;

            XMFLOAT3 Tangent;
            Tangent.x = model.meshes[i].vertices[v].TangentU.x;
            Tangent.y = model.meshes[i].vertices[v].TangentU.y;
            Tangent.z = model.meshes[i].vertices[v].TangentU.z;

            /*position*/
            fileHandle.write(reinterpret_cast<const char*>(&Position.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Position.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Position.z), sizeof(float));
            /*texture coordinates*/
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Texture.u), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].vertices[v].Texture.v), sizeof(float));
            /*normals*/
            fileHandle.write(reinterpret_cast<const char*>(&Normal.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Normal.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Normal.z), sizeof(float));
            /*tangentU*/
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.z), sizeof(float));


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
            fileHandle.write(reinterpret_cast<const char*>(&model.meshes[i].indices[j]), sizeof(uint32_t));
        }

    }

    fileHandle.close();



    std::cout << "Finished writing " << model.fileName << "." << std::endl;
    std::cout << "\n===================================================\n\n";

    return true;
}

void ModelConverter::printFile(const std::string& fileName, bool verbose)
{
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

    std::cout << std::fixed << std::showpoint << std::setprecision(2) << "Number of meshes: " << (int)numMeshes << "\n\n";

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

            file.read((char*)(&vertex.Texture.u), sizeof(float));
            file.read((char*)(&vertex.Texture.v), sizeof(float));

            file.read((char*)(&vertex.Normal.x), sizeof(float));
            file.read((char*)(&vertex.Normal.y), sizeof(float));
            file.read((char*)(&vertex.Normal.z), sizeof(float));

            file.read((char*)(&vertex.TangentU.x), sizeof(float));
            file.read((char*)(&vertex.TangentU.y), sizeof(float));
            file.read((char*)(&vertex.TangentU.z), sizeof(float));

            if (verbose)
            {
                std::cout << "Pos: " << vertex.Position.x << " | " << vertex.Position.y << " | " << vertex.Position.z << "\n";
                std::cout << "Tex: " << vertex.Texture.u << " | " << vertex.Texture.v << "\n";
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

    /*armature global inverse*/
    aiMatrix4x4 mArmature;

    /*matrix*/
    file.read((char*)&mArmature.a1, sizeof(float));
    file.read((char*)&mArmature.a2, sizeof(float));
    file.read((char*)&mArmature.a3, sizeof(float));
    file.read((char*)&mArmature.a4, sizeof(float));

    file.read((char*)&mArmature.b1, sizeof(float));
    file.read((char*)&mArmature.b2, sizeof(float));
    file.read((char*)&mArmature.b3, sizeof(float));
    file.read((char*)&mArmature.b4, sizeof(float));

    file.read((char*)&mArmature.c1, sizeof(float));
    file.read((char*)&mArmature.c2, sizeof(float));
    file.read((char*)&mArmature.c3, sizeof(float));
    file.read((char*)&mArmature.c4, sizeof(float));

    file.read((char*)&mArmature.d1, sizeof(float));
    file.read((char*)&mArmature.d2, sizeof(float));
    file.read((char*)&mArmature.d3, sizeof(float));
    file.read((char*)&mArmature.d4, sizeof(float));

    if (verbose)
    {
        std::cout << "Global armature inverse transform:\n";
        printAIMatrix(mArmature);
    }

    std::cout << "\n---------------------------------------------------\n\n";

    /*num bones*/
    char vNumBones = 0;
    file.read((char*)(&vNumBones), sizeof(char));

    std::cout << std::fixed << std::showpoint << std::setprecision(2) << "NumBones: " << (int)vNumBones << std::endl;

    std::cout << "\n---------------------------------------------------\n\n";

    int temp;
    DirectX::XMFLOAT4X4 mOffset;

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
        file.read((char*)&mOffset._11, sizeof(float));
        file.read((char*)&mOffset._12, sizeof(float));
        file.read((char*)&mOffset._13, sizeof(float));
        file.read((char*)&mOffset._14, sizeof(float));

        file.read((char*)&mOffset._21, sizeof(float));
        file.read((char*)&mOffset._22, sizeof(float));
        file.read((char*)&mOffset._23, sizeof(float));
        file.read((char*)&mOffset._24, sizeof(float));

        file.read((char*)&mOffset._31, sizeof(float));
        file.read((char*)&mOffset._32, sizeof(float));
        file.read((char*)&mOffset._33, sizeof(float));
        file.read((char*)&mOffset._34, sizeof(float));

        file.read((char*)&mOffset._41, sizeof(float));
        file.read((char*)&mOffset._42, sizeof(float));
        file.read((char*)&mOffset._43, sizeof(float));
        file.read((char*)&mOffset._44, sizeof(float));

        if (verbose)
        {
            std::cout << "Offset Matrix:\n";
            std::cout << mOffset._11 << " | " << mOffset._12 << " | " << mOffset._13 << " | " << mOffset._14 << "\n";
            std::cout << mOffset._21 << " | " << mOffset._22 << " | " << mOffset._23 << " | " << mOffset._24 << "\n";
            std::cout << mOffset._31 << " | " << mOffset._32 << " | " << mOffset._33 << " | " << mOffset._34 << "\n";
            std::cout << mOffset._41 << " | " << mOffset._42 << " | " << mOffset._43 << " | " << mOffset._44 << "\n";
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

            file.read((char*)(&vertex.Texture.u), sizeof(float));
            file.read((char*)(&vertex.Texture.v), sizeof(float));

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
                std::cout << "Pos: " << vertex.Position.x << " | " << vertex.Position.y << " | " << vertex.Position.z << "\n";
                std::cout << "Tex: " << vertex.Texture.u << " | " << vertex.Texture.v << "\n";
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

    std::cout << std::fixed << std::showpoint << std::setprecision(2) << "Name:\t" << animName << "\n";

    UINT vNumBones = 0;
    file.read((char*)(&vNumBones), sizeof(int));

    std::cout << "Bones:\t" << vNumBones << "\n";


    for (UINT i = 0; i < vNumBones; i++)
    {
        std::cout << "\n===================================================\n\n";

        UINT vKeyFrames = 0;
        file.read((char*)(&vKeyFrames), sizeof(int));
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
        std::cout << "Finished writing " << clipFile << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms." << std::endl;
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

void ModelConverter::printNodes(aiNode* node, int depth)
{
    std::cout << std::string((long long)depth * 3, ' ') << char(0xC0) << std::string(2, '-') << ">" << node->mName.C_Str();
    if (node->mTransformation.IsIdentity())
    {
        std::cout << " (Identity Transform)";
    }
    std::cout << "\n";

    for (UINT i = 0; i < node->mNumChildren; i++)
    {
        printNodes(node->mChildren[i], depth + 1);
    }
}

void ModelConverter::printAIMatrix(const aiMatrix4x4& m)
{
    aiVector3D scale, translation, rotation;
    aiQuaternion quat;

    m.Decompose(scale, rotation, translation);
    m.Decompose(scale, quat, translation);

    std::cout << std::fixed << "T: " << translation.x << " | " << translation.y << " | " << translation.z << "\n";
    std::cout << std::fixed << "R: " << DirectX::XMConvertToDegrees(rotation.x) << " | " << DirectX::XMConvertToDegrees(rotation.y) << " | " << DirectX::XMConvertToDegrees(rotation.z) << "\n";
    std::cout << std::fixed << "Q: " << quat.x << " | " << quat.y << " | " << quat.z << " | " << quat.w << "\n";
    std::cout << std::fixed << "S: " << scale.x << " | " << scale.y << " | " << scale.z << "\n";
}

aiMatrix4x4 ModelConverter::getGlobalTransform(aiNode* node)
{
    aiMatrix4x4 result = node->mTransformation;

    aiNode* tNode = node;

    while (tNode->mParent != nullptr)
    {
        tNode = tNode->mParent;
        result *= tNode->mTransformation;
    }

    return result;
}

void ModelConverter::transformXM(DirectX::XMFLOAT3& xmf, DirectX::XMMATRIX trfMatrix)
{
    DirectX::XMVECTOR vec = XMVector3Transform(XMLoadFloat3(&xmf), trfMatrix);
    DirectX::XMStoreFloat3(&xmf, vec);
}
