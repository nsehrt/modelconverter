#pragma comment(lib, "assimp-vc142-mt.lib")

#include "ModelConverter.h"
#include <cmath>

using namespace DirectX;



bool ModelConverter::load(InitData& initData)
{
    startTime = std::chrono::high_resolution_clock::now();

    char id[1024];
    _splitpath_s(initData.FileName.c_str(), NULL, 0, NULL, 0, id, 1024, NULL, 0);
    baseFileName = id;

    iData = initData;

    /*load m3d file*/
    std::string ext = initData.FileName.substr(initData.FileName.rfind('.') + 1);

    if (ext == "m3d")
    {
        std::cout << "Load m3d file..\n";

        baseFileName.append(".s3d");
        writeFileName = iData.Prefix + baseFileName;

        bool processStatus = loadM3D();

        if (!processStatus)
        {
            std::cerr << "Failed to load M3D model!" << std::endl;
            return processStatus;
        }

        processStatus = writeS3D();

        if (!processStatus)
        {
            std::cerr << "Failed to write rigged model to " << writeFileName << "!" << std::endl;
            return processStatus;
        }

        processStatus = verifyS3D();

        if (!processStatus)
        {
            std::cerr << "Failed to verify rigged model! (" << writeFileName << ")" << std::endl;
            return processStatus;
        }

        processStatus = writeCLP();

        if (!processStatus)
        {
            std::cerr << "Failed to write animation clips!" << std::endl;
            return processStatus;
        }

        return true;
    }


    /*the normal import*/
    const aiScene* loadedScene = importer.ReadFile(initData.FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | );

    if (!loadedScene)
    {
        std::cerr << "Unable to load specified file!\n";
        return false;
    }

    /*check if rigged*/
    bool rigged = false;

        for (UINT i = 0; i < loadedScene->mNumMeshes; i++)
        {
            if (loadedScene->mMeshes[i]->HasBones())
            {
                rigged = true;
            }
        }

    bool processStatus = false;

    if (rigged)
    {
        baseFileName.append(".s3d");
        writeFileName = iData.Prefix + baseFileName;

        processStatus = loadRigged(loadedScene);

        if (!processStatus)
        {
            std::cerr << "Failed to load rigged model!" << std::endl;
            return processStatus;
        }

        processStatus = writeS3D();

        if (!processStatus)
        {
            std::cerr << "Failed to write rigged model to " << writeFileName << "!" << std::endl;
            return processStatus;
        }

        processStatus = verifyS3D();

        if (!processStatus)
        {
            std::cerr << "Failed to verify rigged model! (" << writeFileName << ")" << std::endl;
            return processStatus;
        }

        if (animationClips.size() > 0)
        {
            processStatus = writeCLP();

            if (!processStatus)
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

    }
    else
    {
        baseFileName.append(".b3d");
        writeFileName = iData.Prefix + baseFileName;

        processStatus = loadStatic(loadedScene);

        if (!processStatus)
        {
            std::cerr << "Failed to load static model!" << std::endl;
            return processStatus;
        }

        processStatus = writeB3D();

        if (!processStatus)
        {
            std::cerr << "Failed to write static model to " << writeFileName << "!" << std::endl;
            return processStatus;
        }

        processStatus = verifyB3D();

        if (!processStatus)
        {
            std::cerr << "Failed to verify static model! (" << writeFileName << ")" << std::endl;
            return processStatus;
        }

    }

    return processStatus;
}

/************LOAD*******************/

/*load and save a static model*/
bool ModelConverter::loadStatic(const aiScene* scene)
{
    std::cout << "Processing model in static mode.\n" << std::endl;

    std::cout << iData << "\n===================================================\n\n";

    bMeshes.reserve(scene->mNumMeshes);

    std::cout << "Model contains " << scene->mNumMeshes << " mesh(es)!" << std::endl;

    std::cout << "\n===================================================\n";

    std::cout << "\nNode hierarchy:\n";
    printNodes(scene->mRootNode);

    std::cout << "\n===================================================\n\n";

    XMFLOAT3 cMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    XMFLOAT3 cMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    XMVECTOR vMin = XMLoadFloat3(&cMin);
    XMVECTOR vMax = XMLoadFloat3(&cMax);

    for (UINT j = 0; j < scene->mNumMeshes; j++)
    {
        aiMesh* mesh = scene->mMeshes[j];

        /*reserve memory for vertices*/

        bMeshes.push_back(new Mesh());
        bMeshes[j]->vertices.reserve(mesh->mNumVertices);
        estimatedFileSize += mesh->mNumVertices * 44;

        std::cout << "Mesh " << j << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices." << std::endl;
        std::cout << "Mesh " << j << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumFaces << " faces.\n" << std::endl;

        std::cout << "Input name of material for " << mesh->mName.C_Str() << ": ";
        getline(std::cin, bMeshes[j]->materialName);

        if (j > 0 && bMeshes[j]->materialName == "")
        {
            bMeshes[j]->materialName = bMeshes[(INT_PTR)j - 1]->materialName;
        }

        if (bMeshes[j]->materialName == "del")
        {
            bMeshes.pop_back();
            continue;
        }

        /*get vertices: positions normals tex coords and tangentu */

        for (UINT v = 0; v < mesh->mNumVertices; v++)
        {
            aiVector3D tangU;
            aiVector3D pos = mesh->mVertices[v];
            aiVector3D norm = mesh->mNormals[v];
            if(mesh->HasTangentsAndBitangents())
                tangU = mesh->mTangents[v];

            aiVector3D tex = mesh->mTextureCoords[0][v];

            /*convert to vertex data format*/
            bMeshes[j]->vertices.push_back(Vertex(float3(pos.x, pos.y, pos.z),
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

            bMeshes[j]->nodeTransform._11 = matrix.a1;
            bMeshes[j]->nodeTransform._12 = matrix.a2;
            bMeshes[j]->nodeTransform._13 = matrix.a3;
            bMeshes[j]->nodeTransform._14 = matrix.a4;
            bMeshes[j]->nodeTransform._21 = matrix.b1;
            bMeshes[j]->nodeTransform._22 = matrix.b2;
            bMeshes[j]->nodeTransform._23 = matrix.b3;
            bMeshes[j]->nodeTransform._24 = matrix.b4;
            bMeshes[j]->nodeTransform._31 = matrix.c1;
            bMeshes[j]->nodeTransform._32 = matrix.c2;
            bMeshes[j]->nodeTransform._33 = matrix.c3;
            bMeshes[j]->nodeTransform._34 = matrix.c4;
            bMeshes[j]->nodeTransform._41 = matrix.d1;
            bMeshes[j]->nodeTransform._42 = matrix.d2;
            bMeshes[j]->nodeTransform._43 = matrix.d3;
            bMeshes[j]->nodeTransform._44 = matrix.d4;

        }
        else
        {
            std::cout << "\nCouldn't find the associated node!\n";

            XMStoreFloat4x4(&bMeshes[j]->nodeTransform, XMMatrixIdentity());
        }

        std::cout << "\n---------------------------------------------------\n\n";

        if (iData.TransformApply != 0)
        {
            XMVECTOR vRot, vPos, vScale;
            XMMatrixDecompose(&vScale, &vRot, &vPos, XMLoadFloat4x4(&bMeshes[j]->nodeTransform));

            XMFLOAT3 tScale;
            XMStoreFloat3(&tScale, vScale);

            if (tScale.x != tScale.y && tScale.x != tScale.z)
            {
                std::cout << "Warning: Non-uniform scaling in node detected!\n";
            }

            XMFLOAT3 tPos;
            XMStoreFloat3(&tPos, vPos);

            tPos.x /= tScale.x;
            tPos.y /= tScale.y;
            tPos.z /= tScale.z;

            if (tScale.x != 1.0f || tScale.y != 1.0f || tScale.z != 1.0f)
            {
                tScale.x = 1.0f;
                tScale.y = 1.0f;
                tScale.z = 1.0f;

                std::cout << "Warning: Scaling not equal 1!\n";
            }

            if (iData.CenterEnabled)
            {
                tPos.x = 0;
                tPos.y = 0;
                tPos.z = 0;
            }

            if (iData.AdditionalRotationX != 0)
            {
                XMVECTOR ar = XMQuaternionRotationRollPitchYaw(XMConvertToRadians((float)iData.AdditionalRotationX), 0.0f, 0.0f);
                if (XMQuaternionIsIdentity(vRot))
                {
                    vRot = ar;
                }
                else
                {
                    vRot = XMVectorMultiply(vRot, ar);
                }
                
            }

            vScale = XMLoadFloat3(&tScale);
            vPos = XMLoadFloat3(&tPos);

            XMStoreFloat4x4(&bMeshes[j]->nodeTransform, XMMatrixScalingFromVector(vScale) * XMMatrixRotationQuaternion(vRot) * XMMatrixTranslationFromVector(vPos));
            XMStoreFloat4x4(&bMeshes[j]->nodeRotation, XMMatrixRotationQuaternion(vRot));
        }

        /*get indices*/
        bMeshes[j]->indices.reserve((INT_PTR)(mesh->mNumFaces) * 3);
        estimatedFileSize += (INT_PTR)(mesh->mNumFaces) * 3 * sizeof(uint32_t);


        for (UINT k = 0; k < mesh->mNumFaces; k++)
        {
            bMeshes[j]->indices.push_back(mesh->mFaces[k].mIndices[0]);
            bMeshes[j]->indices.push_back(mesh->mFaces[k].mIndices[1]);
            bMeshes[j]->indices.push_back(mesh->mFaces[k].mIndices[2]);
        }

    }

    /*apply centering transform and scaling if needed*/
    XMFLOAT3 center;
    DirectX::XMStoreFloat3(&center, 0.5f * (vMin + vMax));

    if (iData.CenterEnabled)
    {
        std::cout << "\nCentering at " << center.x << " | " << center.y << " | " << center.z << ".." << std::endl;
    }
    if (iData.ScaleFactor != 1.0f)
    {
        std::cout << "Scaling with factor " << iData.ScaleFactor << ".." << std::endl;
    }
    if (iData.TransformApply)
    {
        std::cout << "Applying transforms..\n";
    }

    for (auto& m : bMeshes)
    {
        for (auto& v : m->vertices)
        {
            if (iData.CenterEnabled)
            {
                v.Position.x -= center.x;
                v.Position.y -= center.y;
                v.Position.z -= center.z;
            }

            if (iData.ScaleFactor != 0.0f)
            {
                DirectX::XMFLOAT3 xm = { v.Position.x, v.Position.y, v.Position.z };
                XMVECTOR a = XMLoadFloat3(&xm);
                a = XMVectorScale(a, iData.ScaleFactor);
                DirectX::XMStoreFloat3(&xm, a);

                v.Position.x = xm.x;
                v.Position.y = xm.y;
                v.Position.z = xm.z;
            }

            if (iData.TransformApply != 0)
            {
                XMFLOAT3 vec = { v.Position.x, v.Position.y, v.Position.z };
                XMFLOAT3 nVec = { v.Normal.x, v.Normal.y, v.Normal.z };
                XMFLOAT3 tVec = { v.TangentU.x, v.TangentU.y, v.TangentU.z };

                XMMATRIX trf = XMLoadFloat4x4(&m->nodeTransform);
                XMMATRIX rf = XMLoadFloat4x4(&m->nodeRotation);

                transformXM(vec, trf);
                transformXM(nVec, rf);
                transformXM(tVec, rf);

                v.Position = { vec.x, vec.y, vec.z };
                v.Normal = { nVec.x, nVec.y, nVec.z };
                v.TangentU = { tVec.x, tVec.y, tVec.z };

            }

        }
    }


    auto endTime = std::chrono::high_resolution_clock::now();

    std::cout << "\nFinished loading file.\n";
    std::cout << "Estimated size: " << (estimatedFileSize / 1024) << " kbytes (" << estimatedFileSize << " bytes)" << std::endl;
    std::cout << "\n===================================================\n\n";
   

    return true;
}



bool ModelConverter::loadRigged(const aiScene* scene)
{
    std::cout << "Processing model in rigged mode.\n" << std::endl;

    std::cout << iData << "\n===================================================\n\n";

    rMeshes.reserve(scene->mNumMeshes);

    std::cout << "Model contains " << scene->mNumMeshes << " mesh(es)!\n" << std::endl;

    XMFLOAT3 cMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    XMFLOAT3 cMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    XMVECTOR vMin = XMLoadFloat3(&cMin);
    XMVECTOR vMax = XMLoadFloat3(&cMax);

    /*load bones from meshes*/

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

            if (existsBoneByName(bones, fMesh->mBones[j]->mName.C_Str()))
            {
                continue;
            }

            Bone b;

            b.AIBone = fMesh->mBones[j];
            b.Name = fMesh->mBones[j]->mName.C_Str();
            b.Index = j;

            bones.push_back(b);
        }

    }

    for (UINT i = 0; i < scene->mNumMeshes; i++)
    {
        std::cout << "Mesh " << i << " (" << scene->mMeshes[i]->mName.C_Str() << ") has a total of " << totalWeight[i] << " Weights (" << totalWeightSum[i] << ")" << std::endl;
    }

    std::cout << "\nFound a total of " << bones.size() << " bones." << std::endl;

    aiNode* rootNode = scene->mRootNode;

    std::cout << "\n===================================================\n";

    std::cout << "\nNode/bone hierarchy:\n";
    printNodes(rootNode);
    std::cout << std::endl;

    for (auto& b : bones)
    {
        aiNode* fNode = rootNode->FindNode(b.Name.c_str());

        if (fNode)
        {
            
            b.ParentIndex = findParentBone(bones, fNode);
            if(b.ParentIndex >= 0)
            b.ParentName = bones[b.ParentIndex].Name;

        }
        else
        {
            b.ParentIndex = -1;
        }
    }


    for (int i = 0; i < bones.size(); i++)
    {
        bones[i].ParentIndex = findIndexInBones(bones, bones[i].ParentName);
    }

    bHierarchy.resize(bones.size());
     

    /*find root bone*/
    std::string rootBoneName;
    int rootFound = 0;

    for (const auto& b : bones)
    {
        if (b.ParentIndex == -1)
        {
            bHierarchy[0] = std::pair<int, int>(b.Index, b.ParentIndex);
            rootBoneName = b.AIBone->mName.C_Str();
            rootFound++;
        }
    }

    if (rootFound != 1)
    {
        std::cerr << "Can not find root bone or more than one root bone!" << std::endl;
        return false;
    }

    int bonesUsed = 1;

    while (bonesUsed != bones.size())
    {

        for (auto& b : bones)
        {
            if (!b.used)
            {
                if (isInHierarchy(b.ParentIndex))
                {
                    bHierarchy[bonesUsed] = std::pair<int, int>(b.Index, b.ParentIndex);
                    b.used = true;
                    bonesUsed++;
                }

            }
        }

    }

    std::vector<int> testHierarchy;
    testHierarchy.push_back(-1);

    for (const auto& v : bHierarchy)
    {

        if (isInArray(testHierarchy, v.second))
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

    std::cout << "\n===================================================\n\n";

    /*animationen laden*/

    /*figure out root transform*/
    auto rNode = scene->mRootNode->FindNode(rootBoneName.c_str());

    /*load global armature inverse*/
    globalArmatureInverse = getGlobalTransform(rNode->mParent).Inverse();
 
    aiVector3D sc, tr;
    aiQuaternion ro, ron;
    globalArmatureInverse.Decompose(sc, ro, tr);
    
    ron = ro;
    ron.z *= -1;

    globalArmatureInverse = aiMatrix4x4(sc, ron, tr);

    std::cout << "Armature global matrix:\n";
    printAIMatrix(globalArmatureInverse);
    std::cout << "\n===================================================\n\n";


    animationClips.resize(scene->mNumAnimations);
    std::cout << "\n";

    for (UINT k = 0; k < scene->mNumAnimations; k++)
    {

        auto anim = scene->mAnimations[k];
        float animTicks = (float)anim->mTicksPerSecond;

        std::cout << "Animation " << anim->mName.C_Str() << ": " << anim->mDuration / anim->mTicksPerSecond << "s (" << anim->mTicksPerSecond << " tick rate) has " << anim->mNumChannels << " channels.\n";

        animationClips[k].name = anim->mName.C_Str();
        /*get rid of | character*/
        std::replace(animationClips[k].name.begin(), animationClips[k].name.end(), '|', '_');
        animationClips[k].keyframes.resize(bones.size());


        for (size_t x = 0; x < animationClips[k].keyframes.size(); x++)
        {
            animationClips[k].keyframes[x].push_back(KeyFrame());
        }

        for (UINT p = 0; p < anim->mNumChannels;p++)
        {
            auto channel = anim->mChannels[p];
            
            /*figure out how many keyframes*/
            int numKeyFrames = std::max(channel->mNumPositionKeys, channel->mNumRotationKeys);
            int nodeIndex = findIndexInBones(bones, channel->mNodeName.C_Str());
            if (nodeIndex < 0) continue;

            animationClips[k].keyframes[nodeIndex].resize(numKeyFrames);

            for (int m = 0; m < numKeyFrames; m++)
            {
                KeyFrame keyFrame;


                aiMatrix4x4 keyMatrix = aiMatrix4x4({ 1,1,1 }, channel->mRotationKeys[m].mValue, channel->mPositionKeys[m].mValue);

                aiVector3D scale, translation;
                aiQuaternion rotation;

                keyMatrix.Decompose(scale, rotation, translation);


                keyFrame.timeStamp = (float) channel->mRotationKeys[m].mTime / animTicks;
                
                keyFrame.rotationQuat = { rotation.x,
                                            rotation.y,
                                            rotation.z,
                                            rotation.w };

                if (m <= (int)channel->mNumPositionKeys)
                {
                    keyFrame.timeStamp = (float) channel->mPositionKeys[m].mTime / animTicks;
                    keyFrame.translation = { translation.x,
                        translation.y,
                        translation.z };
                }

                animationClips[k].keyframes[nodeIndex][m] = keyFrame;

            }

        }

        std::cout << "\n---------------------------------------------------\n\n";
    }

    /******************/
    estimatedFileSize += (int)bones.size() * 75;

    std::cout << std::endl;

    for (UINT j = 0; j < scene->mNumMeshes; j++)
    {
        aiMesh* mesh = scene->mMeshes[j];

        /*reserve memory for vertices*/
        rMeshes.push_back(new MeshRigged());
        rMeshes[j]->vertices.reserve(mesh->mNumVertices);
        estimatedFileSize += mesh->mNumVertices * 76;

        std::cout<<"Mesh " << j << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices" << std::endl;
        std::cout << "Mesh " << j << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumFaces * 3 << " faces.\n" << std::endl;

        std::cout << "Input name of material for " << mesh->mName.C_Str() << ": ";
        std::getline(std::cin, rMeshes[j]->materialName);

        if (j > 0 && rMeshes[j]->materialName == "")
        {
            rMeshes[j]->materialName = rMeshes[(INT_PTR)j - 1]->materialName;
        }

        if (rMeshes[j]->materialName == "del")
        {
            rMeshes.pop_back();
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
            
            tex = mesh->mTextureCoords[0][v];
            
            /*convert to vertex data format*/
            rMeshes[j]->vertices.push_back(SkinnedVertex(float3(pos.x, pos.y, pos.z),
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

            rMeshes[j]->nodeTransform._11 = matrix.a1;
            rMeshes[j]->nodeTransform._12 = matrix.a2;
            rMeshes[j]->nodeTransform._13 = matrix.a3;
            rMeshes[j]->nodeTransform._14 = matrix.a4;
            rMeshes[j]->nodeTransform._21 = matrix.b1;
            rMeshes[j]->nodeTransform._22 = matrix.b2;
            rMeshes[j]->nodeTransform._23 = matrix.b3;
            rMeshes[j]->nodeTransform._24 = matrix.b4;
            rMeshes[j]->nodeTransform._31 = matrix.c1;
            rMeshes[j]->nodeTransform._32 = matrix.c2;
            rMeshes[j]->nodeTransform._33 = matrix.c3;
            rMeshes[j]->nodeTransform._34 = matrix.c4;
            rMeshes[j]->nodeTransform._41 = matrix.d1;
            rMeshes[j]->nodeTransform._42 = matrix.d2;
            rMeshes[j]->nodeTransform._43 = matrix.d3;
            rMeshes[j]->nodeTransform._44 = matrix.d4;

        }
        else
        {
            std::cout << "\nCouldn't find the associated node!\n";

            XMStoreFloat4x4(&rMeshes[j]->nodeTransform, XMMatrixIdentity());
        }

        std::cout << "\n---------------------------------------------------\n\n";

        if (iData.TransformApply != 0)
        {
            XMVECTOR vRot, vPos, vScale;
            XMMatrixDecompose(&vScale, &vRot, &vPos, XMLoadFloat4x4(&rMeshes[j]->nodeTransform));

            XMFLOAT3 tScale;
            XMStoreFloat3(&tScale, vScale);

            if (tScale.x != tScale.y && tScale.x != tScale.z)
            {
                std::cout << "Warning: Non-uniform scaling in node detected!\n";
            }

            XMFLOAT3 tPos;
            XMStoreFloat3(&tPos, vPos);

            tPos.x /= tScale.x;
            tPos.y /= tScale.y;
            tPos.z /= tScale.z;

            if (tScale.x != 1.0f || tScale.y != 1.0f || tScale.z != 1.0f)
            {
                tScale.x = 1.0f;
                tScale.y = 1.0f;
                tScale.z = 1.0f;

                std::cout << "Warning: Scaling not equal 1!\n";
            }

            if (iData.CenterEnabled)
            {
                tPos.x = 0;
                tPos.y = 0;
                tPos.z = 0;
            }

            if (iData.AdditionalRotationX != 0)
            {
                XMVECTOR ar = XMQuaternionRotationRollPitchYaw(XMConvertToRadians((float)iData.AdditionalRotationX), 0.0f, 0.0f);
                if (XMQuaternionIsIdentity(vRot))
                {
                    vRot = ar;
                }
                else
                {
                    vRot = XMVectorMultiply(vRot, ar);
                }

            }

            vScale = XMLoadFloat3(&tScale);
            vPos = XMLoadFloat3(&tPos);

            XMStoreFloat4x4(&rMeshes[j]->nodeTransform, XMMatrixScalingFromVector(vScale) * XMMatrixRotationQuaternion(vRot) * XMMatrixTranslationFromVector(vPos));
            XMStoreFloat4x4(&rMeshes[j]->nodeRotation, XMMatrixRotationQuaternion(vRot));

        }

        /*get indices*/
        rMeshes[j]->indices.reserve((long long)(mesh->mNumFaces) * 3);
        estimatedFileSize += (long long)(mesh->mNumFaces) * 3 * sizeof(uint32_t);

        for (UINT k = 0; k < mesh->mNumFaces; k++)
        {
            rMeshes[j]->indices.push_back(mesh->mFaces[k].mIndices[0]);
            rMeshes[j]->indices.push_back(mesh->mFaces[k].mIndices[1]);
            rMeshes[j]->indices.push_back(mesh->mFaces[k].mIndices[2]);
        }

    }

    /*add weights from bones to vertices*/
    for (const auto& b : bones)
    {

        for (UINT k = 0; k < b.AIBone->mNumWeights; k++)
        {
            rMeshes[0]->vertices[b.AIBone->mWeights[k].mVertexId].BlendIndices.push_back(b.Index);
            rMeshes[0]->vertices[b.AIBone->mWeights[k].mVertexId].BlendWeights.push_back(b.AIBone->mWeights[k].mWeight);
        }

    }

    for (const auto& v : rMeshes[0]->vertices)
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


    /*apply centering and scaling if needed*/

    XMFLOAT3 center;
    DirectX::XMStoreFloat3(&center, 0.5f * (vMin + vMax));

    
    if (iData.CenterEnabled)
    {
        std::cout << "\nCentering at " << center.x << " | " << center.y << " | " << center.z << ".." << std::endl;
    }
    if (iData.ScaleFactor != 1.0f)
    {
        std::cout << "Scaling with factor " << iData.ScaleFactor << ".." << std::endl;
    }
    if (iData.TransformApply)
    {
        std::cout << "Applying transforms..\n";
    }

    for (auto& m : rMeshes)
    {
        for (auto& v : m->vertices)
        {

            if (iData.CenterEnabled)
            {
                v.Position.x -= center.x;
                v.Position.y -= center.y;
                v.Position.z -= center.z;
            }

            if (iData.ScaleFactor != 1.0f)
            {
                DirectX::XMFLOAT3 xm = { v.Position.x, v.Position.y, v.Position.z };
                XMVECTOR a = XMLoadFloat3(&xm);
                a = XMVectorScale(a, iData.ScaleFactor);
                DirectX::XMStoreFloat3(&xm, a);

                v.Position.x = xm.x;
                v.Position.y = xm.y;
                v.Position.z = xm.z;
            }

            if (iData.TransformApply != 0)
            {
                XMFLOAT3 vec = { v.Position.x, v.Position.y, v.Position.z };
                XMFLOAT3 nVec = { v.Normal.x, v.Normal.y, v.Normal.z };
                XMFLOAT3 tVec = { v.TangentU.x, v.TangentU.y, v.TangentU.z };

                XMMATRIX trf = XMLoadFloat4x4(&m->nodeTransform);
                XMMATRIX rf = XMLoadFloat4x4(&m->nodeRotation);

                transformXM(vec, trf);
                transformXM(nVec, rf);
                transformXM(tVec, rf);

                v.Position = { vec.x, vec.y, vec.z };
                v.Normal = { nVec.x, nVec.y, nVec.z };
                v.TangentU = { tVec.x, tVec.y, tVec.z };

            }

        }
    }
    

    auto endTime = std::chrono::high_resolution_clock::now();

    std::cout << "\nFinished loading file.\n";
    std::cout << "Estimated size: " << (estimatedFileSize/1024) << " kbytes (" << estimatedFileSize << " bytes)" << std::endl;
    std::cout << "\n===================================================\n\n";

    return true;
}

bool ModelConverter::loadM3D()
{
    /*load bones, rmeshes*/
    std::ifstream fin(iData.FileName);

    UINT numMaterials = 0;
    UINT numVertices = 0;
    UINT numTriangles = 0;
    UINT numBones = 0;
    UINT numAnimationClips = 0;

    std::string ignore;

    if (fin)
    {
        fin >> ignore; // file header text
        fin >> ignore >> numMaterials;
        fin >> ignore >> numVertices;
        fin >> ignore >> numTriangles;
        fin >> ignore >> numBones;
        fin >> ignore >> numAnimationClips;

        /*skip materials*/
        fin >> ignore;

        for (UINT i = 0; i < numMaterials; ++i)
        {
            fin >> ignore >> ignore;
            fin >> ignore >> ignore >> ignore >> ignore;
            fin >> ignore >> ignore >> ignore >> ignore;
            fin >> ignore >> ignore;
            fin >> ignore >> ignore;
            fin >> ignore >> ignore;
            fin >> ignore >> ignore;
            fin >> ignore >> ignore;
        } 

        /*subset table*/
        std::vector<Subset> subsets;
        subsets.resize(numMaterials);

        fin >> ignore; // subset header text
        for (UINT i = 0; i < numMaterials; ++i)
        {
            fin >> ignore >> subsets[i].Id;
            fin >> ignore >> subsets[i].VertexStart;
            fin >> ignore >> subsets[i].VertexCount;
            fin >> ignore >> subsets[i].FaceStart;
            fin >> ignore >> subsets[i].FaceCount;
        }

        /*vertices*/
        std::vector<SkinnedVertex> skvertices(numVertices);
        int boneIndices[4];
        float weights[4];

        fin >> ignore;

        for (UINT i = 0; i < numVertices; ++i)
        {
            float blah;
            fin >> ignore >> skvertices[i].Position.x >> skvertices[i].Position.y >> skvertices[i].Position.z;
            fin >> ignore >> skvertices[i].TangentU.x >> skvertices[i].TangentU.y >> skvertices[i].TangentU.z >> blah /*skvertices[i].TangentU.w*/;
            fin >> ignore >> skvertices[i].Normal.x >> skvertices[i].Normal.y >> skvertices[i].Normal.z;
            fin >> ignore >> skvertices[i].Texture.u >> skvertices[i].Texture.v;
            fin >> ignore >> weights[0] >> weights[1] >> weights[2] >> weights[3];
            fin >> ignore >> boneIndices[0] >> boneIndices[1] >> boneIndices[2] >> boneIndices[3];

            skvertices[i].BlendWeights.resize(4);
            skvertices[i].BlendIndices.resize(4);

            skvertices[i].BlendWeights[0] = weights[0];
            skvertices[i].BlendWeights[1] = weights[1];
            skvertices[i].BlendWeights[2] = weights[2];
            skvertices[i].BlendWeights[3] = 1.0f - weights[0] - weights[1] - weights[2];

            skvertices[i].BlendIndices[0] = (BYTE)boneIndices[0];
            skvertices[i].BlendIndices[1] = (BYTE)boneIndices[1];
            skvertices[i].BlendIndices[2] = (BYTE)boneIndices[2];
            skvertices[i].BlendIndices[3] = (BYTE)boneIndices[3];
        }

        /*indices*/
        std::vector<USHORT> indices((INT_PTR)numTriangles*3);

        fin >> ignore; // triangles header textd
        for (UINT i = 0; i < numTriangles; ++i)
        {
            fin >> indices[(INT_PTR)i * 3 + 0] >> indices[(INT_PTR)i * 3 + 2] >> indices[(INT_PTR)i * 3 + 1];
        }


        std::vector<aiMatrix4x4> boneOffsets(numBones);

        fin >> ignore; // BoneOffsets header text
        for (UINT i = 0; i < numBones; ++i)
        {
            fin >> ignore >>
                boneOffsets[i].a1 >> boneOffsets[i].a2 >> boneOffsets[i].a3 >> boneOffsets[i].a4 >>
                boneOffsets[i].b1 >> boneOffsets[i].b2 >> boneOffsets[i].b3 >> boneOffsets[i].b4 >>
                boneOffsets[i].c1 >> boneOffsets[i].c2 >> boneOffsets[i].c3 >> boneOffsets[i].c4 >>
                boneOffsets[i].d1 >> boneOffsets[i].d2 >> boneOffsets[i].d3 >> boneOffsets[i].d4;
        }
    

        std::vector<int> boneIndexToParent(numBones);

        fin >> ignore; // BoneHierarchy header text
        for (UINT i = 0; i < numBones; ++i)
        {
            fin >> ignore >> boneIndexToParent[i];
        }

        //animation

        animationClips.resize(1);
        animationClips[0].name = "take1";
        fin >> ignore; // AnimationClips header text
        for (UINT clipIndex = 0; clipIndex < numAnimationClips; ++clipIndex)
        {
            std::string clipName;
            fin >> ignore >> clipName;
            fin >> ignore; // {

            animationClips[0].keyframes.resize(numBones);

            for (UINT boneIndex = 0; boneIndex < numBones; ++boneIndex)
            {
                UINT numKeyframes = 0;
                fin >> ignore >> ignore >> numKeyframes;
                fin >> ignore; // {

                animationClips[0].keyframes[boneIndex].resize(numKeyframes);

                for (UINT i = 0; i < numKeyframes; ++i)
                {
                    float t = 0.0f;
                    XMFLOAT3 p(0.0f, 0.0f, 0.0f);
                    XMFLOAT3 s(1.0f, 1.0f, 1.0f);
                    XMFLOAT4 q(0.0f, 0.0f, 0.0f, 1.0f);
                    fin >> ignore >> t;
                    fin >> ignore >> p.x >> p.y >> p.z;
                    fin >> ignore >> s.x >> s.y >> s.z;
                    fin >> ignore >> q.x >> q.y >> q.z >> q.w;

                    animationClips[0].keyframes[boneIndex][i].timeStamp = t;
                    animationClips[0].keyframes[boneIndex][i].translation = p;
                    animationClips[0].keyframes[boneIndex][i].scale = s;
                    animationClips[0].keyframes[boneIndex][i].rotationQuat = q;
                }

                fin >> ignore; // }
            }
            fin >> ignore; // }
        }


        fin.close();

        /****/
        bones.resize(numBones);

        for (UINT i = 0; i < numBones; i++)
        {
            bones[i].Index = i;
            bones[i].ParentIndex = boneIndexToParent[i];
            bones[i].Name = "bone";
            bones[i].ParentName = "bone";
            XMStoreFloat4x4(&bones[i].nodeTransform, XMMatrixIdentity());
            bones[i].AIBone = new aiBone();
            bones[i].AIBone->mOffsetMatrix = boneOffsets[i];
        }



        rMeshes.push_back(new MeshRigged());
        rMeshes[0]->vertices.resize(numVertices);

        for (UINT i = 0; i < numVertices; i++)
        {
           rMeshes[0]->vertices[i] = skvertices[i];
        }

        rMeshes[0]->materialName = "default";

        rMeshes[0]->indices.resize(indices.size());

        for (UINT k = 0; k < indices.size(); k++)
        {
            rMeshes[0]->indices[k] = indices[k];
        }

        return true;
    }


    return false;
}




/************WRITE*******************/
bool ModelConverter::writeB3D()
{

    /*writing to binary file .b3d*/
    startTime = std::chrono::high_resolution_clock::now();

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    auto fileHandle = std::fstream(writeFileName.c_str(), std::ios::out | std::ios::binary);

    if (!fileHandle.is_open())
    {
        return false;
    }

    /*header*/
    char header[4] = { 0x62, 0x33, 0x64, 0x66 };
    fileHandle.write(header, 4);

    /*number of meshes*/
    char meshSize = (char)bMeshes.size();
    fileHandle.write(reinterpret_cast<const char*>(&meshSize), sizeof(char));

    for (char i = 0; i < meshSize; i++)
    {
        /*material name*/
        short stringSize = (short)bMeshes[i]->materialName.size();
        fileHandle.write(reinterpret_cast<const char*>(&stringSize), sizeof(stringSize));
        fileHandle.write(reinterpret_cast<const char*>(&bMeshes[i]->materialName[0]), stringSize);

        /*num vertices*/
        int verticesSize = (int)bMeshes[i]->vertices.size();
        fileHandle.write(reinterpret_cast<const char*>(&verticesSize), sizeof(int));

        /*vertices*/
        for (int v = 0; v < verticesSize; v++)
        {
            XMFLOAT3 Position;
            Position.x = bMeshes[i]->vertices[v].Position.x;
            Position.y = bMeshes[i]->vertices[v].Position.y;
            Position.z = bMeshes[i]->vertices[v].Position.z;

            XMFLOAT3 Normal;
            Normal.x = bMeshes[i]->vertices[v].Normal.x;
            Normal.y = bMeshes[i]->vertices[v].Normal.y;
            Normal.z = bMeshes[i]->vertices[v].Normal.z;

            XMFLOAT3 Tangent;
            Tangent.x = bMeshes[i]->vertices[v].TangentU.x;
            Tangent.y = bMeshes[i]->vertices[v].TangentU.y;
            Tangent.z = bMeshes[i]->vertices[v].TangentU.z;

            /*position*/
            fileHandle.write(reinterpret_cast<const char*>(&Position.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Position.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Position.z), sizeof(float));
            /*texture coordinates*/
            fileHandle.write(reinterpret_cast<const char*>(&bMeshes[i]->vertices[v].Texture.u), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&bMeshes[i]->vertices[v].Texture.v), sizeof(float));
            /*normals*/
            fileHandle.write(reinterpret_cast<const char*>(&Normal.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Normal.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Normal.z), sizeof(float));
            /*tangentU*/
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.z), sizeof(float));

        }


        /*num indices*/
        int indicesSize = (int)bMeshes[i]->indices.size();
        fileHandle.write(reinterpret_cast<const char*>(&indicesSize), sizeof(int));

        /*indices*/
        for (int j = 0; j < indicesSize; j++)
        {
            fileHandle.write(reinterpret_cast<const char*>(&bMeshes[i]->indices[j]), sizeof(uint32_t));
        }

    }

    fileHandle.close();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Finished writing " << writeFileName << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms." << std::endl;
    std::cout << "\n===================================================\n\n";

    return true;
}

bool ModelConverter::writeS3D()
{
    /*writing to binary file .s3d*/
    startTime = std::chrono::high_resolution_clock::now();

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    auto fileHandle = std::fstream(writeFileName.c_str(), std::ios::out | std::ios::binary);

    if (!fileHandle.is_open())
    {
        return false;
    }

    /*header*/
    char header[4] = { 0x73, 0x33, 0x64, 0x66 };
    fileHandle.write(header, 4);

    /*global armature inverse*/
    auto wMatrix = globalArmatureInverse;

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
    char boneSize = (char)bones.size();
    fileHandle.write(reinterpret_cast<const char*>(&boneSize), sizeof(char));

    for (const auto& b : bones)
    {
        /*bone id*/
        char boneID = (char)b.Index;
        fileHandle.write(reinterpret_cast<const char*>(&boneID), sizeof(char));

        /*bone name*/
        short boneStrSize = (short)b.Name.size();
        fileHandle.write(reinterpret_cast<const char*>(&boneStrSize), sizeof(boneStrSize));
        fileHandle.write(reinterpret_cast<const char*>(&b.Name[0]), boneStrSize);

        /*bone offset matrix*/
        aiMatrix4x4 offsetMatrix = b.AIBone->mOffsetMatrix.Transpose(); /*transpose because assimp uses opengl style matrices*/

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
    for (const auto& b : bHierarchy)
    {
        fileHandle.write(reinterpret_cast<const char*>(&b.first), sizeof(int));
        fileHandle.write(reinterpret_cast<const char*>(&b.second), sizeof(int));
    }

    /*number of meshes*/
    char meshSize = (char)rMeshes.size();
    fileHandle.write(reinterpret_cast<const char*>(&meshSize), sizeof(char));

    for (char i = 0; i < meshSize; i++)
    {
        /*material name*/
        short stringSize = (short)rMeshes[i]->materialName.size();
        fileHandle.write(reinterpret_cast<const char*>(&stringSize), sizeof(stringSize));
        fileHandle.write(reinterpret_cast<const char*>(&rMeshes[i]->materialName[0]), stringSize);

        /*num vertices*/
        int verticesSize = (int)rMeshes[i]->vertices.size();
        fileHandle.write(reinterpret_cast<const char*>(&verticesSize), sizeof(int));

        /*vertices*/
        for (int v = 0; v < verticesSize; v++)
        {
            XMFLOAT3 Position;
            Position.x = rMeshes[i]->vertices[v].Position.x;
            Position.y = rMeshes[i]->vertices[v].Position.y;
            Position.z = rMeshes[i]->vertices[v].Position.z;

            XMFLOAT3 Normal;
            Normal.x = rMeshes[i]->vertices[v].Normal.x;
            Normal.y = rMeshes[i]->vertices[v].Normal.y;
            Normal.z = rMeshes[i]->vertices[v].Normal.z;

            XMFLOAT3 Tangent;
            Tangent.x = rMeshes[i]->vertices[v].TangentU.x;
            Tangent.y = rMeshes[i]->vertices[v].TangentU.y;
            Tangent.z = rMeshes[i]->vertices[v].TangentU.z;

            /*position*/
            fileHandle.write(reinterpret_cast<const char*>(&Position.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Position.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Position.z), sizeof(float));
            /*texture coordinates*/
            fileHandle.write(reinterpret_cast<const char*>(&rMeshes[i]->vertices[v].Texture.u), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&rMeshes[i]->vertices[v].Texture.v), sizeof(float));
            /*normals*/
            fileHandle.write(reinterpret_cast<const char*>(&Normal.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Normal.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Normal.z), sizeof(float));
            /*tangentU*/
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Tangent.z), sizeof(float));

            /*fill bone data*/
            while (rMeshes[i]->vertices[v].BlendIndices.size() < 4)
            {
                rMeshes[i]->vertices[v].BlendIndices.push_back(0);
            }

            while (rMeshes[i]->vertices[v].BlendWeights.size() < 4)
            {
                rMeshes[i]->vertices[v].BlendWeights.push_back(0.0f);
            }

            /*bone indices*/
            for (int k = 0; k < 4; k++)
            {
                fileHandle.write(reinterpret_cast<const char*>(&rMeshes[i]->vertices[v].BlendIndices[k]), sizeof(UINT));
                fileHandle.write(reinterpret_cast<const char*>(&rMeshes[i]->vertices[v].BlendWeights[k]), sizeof(float));
            }

        }


        /*num indices*/
        int indicesSize = (int)rMeshes[i]->indices.size();
        fileHandle.write(reinterpret_cast<const char*>(&indicesSize), sizeof(int));

        /*indices*/
        for (int j = 0; j < indicesSize; j++)
        {
            fileHandle.write(reinterpret_cast<const char*>(&rMeshes[i]->indices[j]), sizeof(uint32_t));
        }

    }

    fileHandle.close();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Finished writing " << writeFileName << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms." << std::endl;
    std::cout << "\n===================================================\n\n";

    return true;
}



/************VERIFY*******************/
bool ModelConverter::verifyB3D()
{
    /*verification*/
    std::cout << "Verifying b3d file is correct...\n";

    /*load b3d file*/
    std::streampos fileSize;
    std::ifstream vFile(writeFileName, std::ios::binary);

    /*file size*/
    vFile.seekg(0, std::ios::end);
    fileSize = vFile.tellg();
    vFile.seekg(0, std::ios::beg);

    /*check header*/
    bool hr = true;
    std::cout << "header...\t";

    char buff[4] = { 'b', '3', 'd', 'f' };

    for (int i = 0; i < 4; i++)
    {
        char t;
        vFile.read(&t, sizeof(char));

        if (t != buff[i])
        {
            hr = false;
            break;
        }
    }

    if (hr == false)
    {
        std::cout << "not a b3d file\n";
    }
    else
    {
        std::cout << "ok\n";
    }

    /*num meshes*/
    std::cout << "num meshes...\t";
    char vNumMeshes = 0;
    vFile.read((char*)(&vNumMeshes), sizeof(char));

    if (vNumMeshes == bMeshes.size())
    {
        std::cout << "ok (" << (int)vNumMeshes << ")\n";
    }
    else
    {
        std::cout << "failed\n";
    }

    for (char i = 0; i < vNumMeshes; i++)
    {
        bMeshesV.push_back(new Mesh());

        /*material*/
        std::cout << "material...\t";

        short slen = 0;
        vFile.read((char*)(&slen), sizeof(short));

        char* mat = new char[(INT_PTR)slen + 1];
        vFile.read(mat, slen);
        mat[slen] = '\0';

        if (bMeshes[i]->materialName.compare(mat) == 0)
        {
            std::cout << "ok (" << mat << ")\n";
        }
        else
        {
            std::cout << "failed" << std::endl;
        }


        /*num vertices*/
        std::cout << "num vertices...\t";

        int vnVert = 0;
        vFile.read((char*)(&vnVert), sizeof(int));

        if (vnVert == bMeshes[i]->vertices.size())
        {
            std::cout << "ok (" << vnVert << ")\n";
        }
        else
        {
            std::cout << "failed" << std::endl;
        }

        bMeshesV[i]->vertices.resize(vnVert);

        /*vertices*/
        std::cout << "vertices...\t";

        for (int j = 0; j < vnVert; j++)
        {
            vFile.read((char*)(&bMeshesV[i]->vertices[j].Position.x), sizeof(float));
            vFile.read((char*)(&bMeshesV[i]->vertices[j].Position.y), sizeof(float));
            vFile.read((char*)(&bMeshesV[i]->vertices[j].Position.z), sizeof(float));

            vFile.read((char*)(&bMeshesV[i]->vertices[j].Texture.u), sizeof(float));
            vFile.read((char*)(&bMeshesV[i]->vertices[j].Texture.v), sizeof(float));

            vFile.read((char*)(&bMeshesV[i]->vertices[j].Normal.x), sizeof(float));
            vFile.read((char*)(&bMeshesV[i]->vertices[j].Normal.y), sizeof(float));
            vFile.read((char*)(&bMeshesV[i]->vertices[j].Normal.z), sizeof(float));

            vFile.read((char*)(&bMeshesV[i]->vertices[j].TangentU.x), sizeof(float));
            vFile.read((char*)(&bMeshesV[i]->vertices[j].TangentU.y), sizeof(float));
            vFile.read((char*)(&bMeshesV[i]->vertices[j].TangentU.z), sizeof(float));
        }


        if (bMeshesV[i]->vertices[0].Position.z == bMeshesV[i]->vertices[0].Position.z &&
            bMeshesV[i]->vertices[0].TangentU.x == bMeshesV[i]->vertices[0].TangentU.x)
        {
            std::cout << "ok" << std::endl;
        }
        else
        {
            std::cout << "failed" << std::endl;
        }



        /*num indices*/
        std::cout << "num indices...\t";

        int vInd = 0;
        vFile.read((char*)(&vInd), sizeof(int));

        if (vInd == bMeshes[i]->indices.size())
        {
            std::cout << "ok (" << vInd << ")\n";
        }
        else
        {
            std::cout << "failed - " << vInd << " vs " << bMeshes[i]->indices.size() << std::endl;
        }

        /*indices*/
        std::cout << "indices...\t";

        bMeshesV[i]->indices.resize(vInd);

        for (int j = 0; j < vInd; j++)
        {
            vFile.read((char*)(&bMeshesV[i]->indices[j]), sizeof(int));
        }


        if (bMeshesV[i]->indices == bMeshes[i]->indices)
        {
            std::cout << "ok" << std::endl;
        }
        else
        {
            std::cout << "failed" << std::endl;
        }


    }

    return true;
}

bool ModelConverter::verifyS3D()
{
    /*verification*/
    std::cout << "Verifying s3d file is correct...\n";

    /*load b3d file*/
    std::streampos fileSize;
    std::ifstream vFile(writeFileName, std::ios::binary);

    /*file size*/
    vFile.seekg(0, std::ios::end);
    fileSize = vFile.tellg();
    vFile.seekg(0, std::ios::beg);

    /*check header*/
    bool hr = true;
    std::cout << "header...\t";

    char buff[4] = { 's', '3', 'd', 'f' };

    for (int i = 0; i < 4; i++)
    {
        char t;
        vFile.read(&t, sizeof(char));

        if (t != buff[i])
        {
            hr = false;
            break;
        }
    }

    if (hr == false)
    {
        std::cout << "not a s3d file\n";
    }
    else
    {
        std::cout << "ok\n";
    }

    /*armature global inverse*/
    char* voidPtr = new char[64];

    vFile.read(voidPtr, sizeof(float) * 16);

    std::cout << "armature...\tok\n";

    /*num bones*/
    std::cout << "num bones...\t";

    char vNumBones = 0;
    vFile.read((char*)(&vNumBones), sizeof(char));

    if (vNumBones == bones.size())
    {
        std::cout << "ok (" << (int)vNumBones << ")\n";
    }
    else
    {
        std::cout << "failed (" << vNumBones << " - " << bones.size() << ")\n";
        return false;
    }

    std::cout << "bone data...\t";

    std::vector<int> vBoneID(vNumBones);
    std::vector<std::string> vBoneName(vNumBones);
    

    for (char i = 0; i < vNumBones; i++)
    {
        /*id*/
        vFile.read((char*)(&vBoneID[i]), sizeof(char));

        /*name length*/
        short slen = 0;
        vFile.read((char*)(&slen), sizeof(short));

        /*name*/
        char* bname = new char[(INT_PTR)slen + 1];
        vFile.read(bname, slen);
        bname[slen] = '\0';

        vBoneName[i] = std::string(bname);
        delete[] bname;
    
        /*matrix*/
        vFile.read(voidPtr, sizeof(float) * 16);
    }

    UINT boneIDCheck = 0;
    for (int i = 0; i < vBoneID.size(); i++)
    {
        if (vBoneID[i] != bones[i].Index)
        {
            boneIDCheck++;
        }

        if (vBoneName[i] != bones[i].Name)
        {
            boneIDCheck++;
        }
    }

    if (boneIDCheck == 0)
    {
        std::cout << "ok\n";
    }
    else
    {
        std::cout << "failed! (" << boneIDCheck << " errors\n";
        return false;
    }

    /*bone hierarchy*/

    std::cout << "bone order...\t";

    std::vector<Bone> boneHierarchyCheck(vNumBones);

    for (char i = 0; i < vNumBones; i++)
    {
        vFile.read((char*)(&boneHierarchyCheck[i].Index), sizeof(int));
        vFile.read((char*)(&boneHierarchyCheck[i].ParentIndex), sizeof(int));
    }

    UINT timesFailed = 0;
    for (char i = 0; i < vNumBones; i++)
    {
        if (boneHierarchyCheck[i].Index != bHierarchy[i].first)
        {
            timesFailed++;
        }

        if (boneHierarchyCheck[i].ParentIndex != bHierarchy[i].second)
        {
            timesFailed++;
        }

    }

    if (timesFailed == 0)
    {
        std::cout << "ok\n";
    }
    else
    {
        std::cout << "failed! (" << timesFailed << " errors)\n";
    }


    /*num meshes*/
    std::cout << "num meshes...\t";
    char vNumMeshes = 0;
    vFile.read((char*)(&vNumMeshes), sizeof(char));

    if (vNumMeshes == rMeshes.size())
    {
        std::cout << "ok (" << (int)vNumMeshes << ")\n";
    }
    else
    {
        std::cout << "failed (" << vNumMeshes << " - " << rMeshes.size() << ")\n";
        return false;
    }

    for (char i = 0; i < vNumMeshes; i++)
    {
        rMeshesV.push_back(new MeshRigged());

        /*material*/
        std::cout << "material...\t";

        short slen = 0;
        vFile.read((char*)(&slen), sizeof(short));

        char* mat = new char[(INT_PTR)slen + 1];
        vFile.read(mat, slen);
        mat[slen] = '\0';

        if (rMeshes[i]->materialName.compare(mat) == 0)
        {
            std::cout << "ok (" << mat << ")\n";
        }
        else
        {
            std::cout << "failed" << std::endl;
        }


        /*num vertices*/
        std::cout << "num vertices...\t";

        int vnVert = 0;
        vFile.read((char*)(&vnVert), sizeof(int));

        if (vnVert == rMeshes[i]->vertices.size())
        {
            std::cout << "ok (" << vnVert << ")\n";
        }
        else
        {
            std::cout << "failed" << std::endl;
        }

        rMeshesV[i]->vertices.resize(vnVert);

        /*vertices*/
        std::cout << "vertices...\t";

        for (int j = 0; j < vnVert; j++)
        {
            vFile.read((char*)(&rMeshesV[i]->vertices[j].Position.x), sizeof(float));
            vFile.read((char*)(&rMeshesV[i]->vertices[j].Position.y), sizeof(float));
            vFile.read((char*)(&rMeshesV[i]->vertices[j].Position.z), sizeof(float));

            vFile.read((char*)(&rMeshesV[i]->vertices[j].Texture.u), sizeof(float));
            vFile.read((char*)(&rMeshesV[i]->vertices[j].Texture.v), sizeof(float));

            vFile.read((char*)(&rMeshesV[i]->vertices[j].Normal.x), sizeof(float));
            vFile.read((char*)(&rMeshesV[i]->vertices[j].Normal.y), sizeof(float));
            vFile.read((char*)(&rMeshesV[i]->vertices[j].Normal.z), sizeof(float));

            vFile.read((char*)(&rMeshesV[i]->vertices[j].TangentU.x), sizeof(float));
            vFile.read((char*)(&rMeshesV[i]->vertices[j].TangentU.y), sizeof(float));
            vFile.read((char*)(&rMeshesV[i]->vertices[j].TangentU.z), sizeof(float));

            rMeshesV[i]->vertices[j].BlendIndices.resize(4);
            rMeshesV[i]->vertices[j].BlendWeights.resize(4);

            for (int k = 0; k < 4; k++)
            {
                vFile.read((char*)(&rMeshesV[i]->vertices[j].BlendIndices[k]), sizeof(UINT));
                vFile.read((char*)(&rMeshesV[i]->vertices[j].BlendWeights[k]), sizeof(float));
            }
        }


        if (rMeshesV[i]->vertices[0].Position.z == rMeshesV[i]->vertices[0].Position.z &&
            rMeshesV[i]->vertices[0].TangentU.x == rMeshesV[i]->vertices[0].TangentU.x)
        {
            std::cout << "ok" << std::endl;
        }
        else
        {
            std::cout << "failed" << std::endl;
        }



        /*num indices*/
        std::cout << "num indices...\t";

        int vInd = 0;
        vFile.read((char*)(&vInd), sizeof(int));

        if (vInd == rMeshes[i]->indices.size())
        {
            std::cout << "ok (" << vInd << ")\n";
        }
        else
        {
            std::cout << "failed - " << vInd << " vs " << rMeshes[i]->indices.size() << std::endl;
        }

        /*indices*/
        std::cout << "indices...\t";

        rMeshesV[i]->indices.resize(vInd);

        for (int j = 0; j < vInd; j++)
        {
            vFile.read((char*)(&rMeshesV[i]->indices[j]), sizeof(int));
        }


        if (rMeshesV[i]->indices == rMeshes[i]->indices)
        {
            std::cout << "ok" << std::endl;
        }
        else
        {
            std::cout << "failed" << std::endl;
        }


    }

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

        if(verbose)
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

        SkinnedVertex vertex;

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

        if(verbose)
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

bool ModelConverter::writeCLP()
{
    std::cout << "\n===================================================\n\n";

    for (const auto& f : animationClips)
    {
        /*writing to binary file .clp*/
        startTime = std::chrono::high_resolution_clock::now();

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

        int strSize = (int) f.name.size();
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
            if(verbose)
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