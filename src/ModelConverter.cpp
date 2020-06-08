#pragma comment(lib, "assimp-vc142-mt.lib")

#include "ModelConverter.h"


using namespace DirectX;



bool ModelConverter::load(InitData& initData)
{
    startTime = std::chrono::high_resolution_clock::now();

    char id[1024];
    _splitpath_s(initData.FileName.c_str(), NULL, 0, NULL, 0, id, 1024, NULL, 0);
    baseFileName = id;

    const aiScene* loadedScene = importer.ReadFile(initData.FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded | aiProcess_GenNormals | aiProcess_CalcTangentSpace);

    if (!loadedScene)
    {
        std::cerr << "Unable to load specified file!\n";
        return false;
    }

    iData = initData;

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

    std::cout << "\nNode/bone hierarchy:\n";
    printNodes(scene->mRootNode);

    std::cout << "\n===================================================\n\n";

    int i = 0;

    XMFLOAT3 cMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    XMFLOAT3 cMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    XMVECTOR vMin = XMLoadFloat3(&cMin);
    XMVECTOR vMax = XMLoadFloat3(&cMax);

    for (UINT j = 0; j < scene->mNumMeshes; j++)
    {
        aiMesh* mesh = scene->mMeshes[i];

        /*reserve memory for vertices*/

        bMeshes.push_back(new Mesh());
        bMeshes[i]->vertices.reserve(mesh->mNumVertices);
        estimatedFileSize += mesh->mNumVertices * 44;

        std::cout << "Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices" << std::endl;

        std::cout << "Input name of material for " << mesh->mName.C_Str() << ": ";
        getline(std::cin, bMeshes[i]->materialName);

        if (i > 0 && bMeshes[i]->materialName == "")
        {
            bMeshes[i]->materialName = bMeshes[(INT_PTR)i - 1]->materialName;
        }

        if (bMeshes[i]->materialName == "del")
        {
            bMeshes.pop_back();
            continue;
        }

        /*get vertices: positions normals tex coords and tangentu */

        for (UINT v = 0; v < mesh->mNumVertices; v++)
        {

            aiVector3D pos = mesh->mVertices[v];
            aiVector3D norm = mesh->mNormals[v];
            aiVector3D tangU = mesh->mTangents[v];
            aiVector3D tex = mesh->mTextureCoords[0][v];

            /*convert to vertex data format*/
            bMeshes[i]->vertices.push_back(Vertex(float3(pos.x, pos.y, pos.z),
                                          float2(tex.x, tex.y),
                                          float3(norm.x, norm.y, norm.z),
                                          float3(tangU.x, tangU.y, tangU.z)
            ));

            XMFLOAT3 pV3 = { pos.x, pos.y, pos.z };

            vMin = XMVectorMin(vMin, XMLoadFloat3(&pV3));
            vMax = XMVectorMax(vMax, XMLoadFloat3(&pV3));

        }


        /*apply centering and scaling if needed*/

        XMFLOAT3 center;
        DirectX::XMStoreFloat3(&center, 0.5f * (vMin + vMax));

        std::cout << "Center is " << center.x << ", " << center.y << ", " << center.z << std::endl;
        if (iData.ScaleFactor != 1.0f)
        {
            std::cout << "Scaling with factor " << iData.ScaleFactor << std::endl;
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

            }
        }

        /*get indices*/
        bMeshes[i]->indices.reserve((INT_PTR)(mesh->mNumFaces) * 3);
        estimatedFileSize += (INT_PTR)(mesh->mNumFaces) * 3 * sizeof(uint32_t);

        std::cout << "Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumFaces << " faces\n" << std::endl;


        for (UINT j = 0; j < mesh->mNumFaces; j++)
        {
            bMeshes[i]->indices.push_back(mesh->mFaces[j].mIndices[0]);
            bMeshes[i]->indices.push_back(mesh->mFaces[j].mIndices[1]);
            bMeshes[i]->indices.push_back(mesh->mFaces[j].mIndices[2]);
        }

        i++;
    }

    auto endTime = std::chrono::high_resolution_clock::now();

    std::cout << "Finished loading file.\n";
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

    int i = 0;

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

    std::sort(bones.begin(), bones.end(), [](const Bone& a, const Bone& b)->bool
              {
                  if (a.ParentIndex == b.ParentIndex)
                  {
                      return a.Index < b.Index;
                  }
                  else
                  {
                      return a.ParentIndex < b.ParentIndex;
                  }
                  
              });

    for (int i = 0; i < bones.size(); i++)
    {
        bones[i].Index = i;
    }

    for (int i = 0; i < bones.size(); i++)
    {
        bones[i].ParentIndex = findIndexInBones(bones, bones[i].ParentName);
    }

    std::vector<int> testHierarchy;

    testHierarchy.push_back(bones[0].Index);

    for (int i = 1; i < bones.size(); i++)
    {
        bool found = false;

        for (int j = 0; j < testHierarchy.size(); j++)
        {
            if (bones[i].ParentIndex == testHierarchy[j])
                found = true;
        }

        if (!found)
        {
            std::cout << "Failed bone hierarchy test!\n\n";
            return false;
        }
        else
        {
            testHierarchy.push_back(bones[i].Index);
        }

    }

    std::cout << "Bone hierarchy test successful.\n";

    std::cout << "\n===================================================\n";

    estimatedFileSize += (int)bones.size() * 75;

    //for (const auto& b : bones)
    //{
    //    std::cout << "Bone " << b.Index << " (" << b.Name << ") is a child of " << b.ParentIndex << " (" << (b.ParentIndex > -1 ? bones[b.ParentIndex].Name : "-1") << ")" << std::endl;

    //    aiVector3D pos;
    //    aiQuaternion rot;
    //    b.AIBone->mOffsetMatrix.DecomposeNoScaling(rot, pos);

    //    //std::cout << "Pos: " << pos.x << " " << pos.y << " " << pos.z << "\n";
    //    //std::cout << "Rot: " << rot.x << " " << rot.y << " " << rot.z << " " << rot.w << "\n";

    //    //std::cout << b.AIBone->mOffsetMatrix.a1 << " " << b.AIBone->mOffsetMatrix.a2 << " " << b.AIBone->mOffsetMatrix.a3 << " " << b.AIBone->mOffsetMatrix.a4 << "\n";
    //    //std::cout << b.AIBone->mOffsetMatrix.b1 << " " << b.AIBone->mOffsetMatrix.b2 << " " << b.AIBone->mOffsetMatrix.b3 << " " << b.AIBone->mOffsetMatrix.b4 << "\n";
    //    //std::cout << b.AIBone->mOffsetMatrix.c1 << " " << b.AIBone->mOffsetMatrix.c2 << " " << b.AIBone->mOffsetMatrix.c3 << " " << b.AIBone->mOffsetMatrix.c4 << "\n";
    //    //std::cout << b.AIBone->mOffsetMatrix.d1 << " " << b.AIBone->mOffsetMatrix.d2 << " " << b.AIBone->mOffsetMatrix.d3 << " " << b.AIBone->mOffsetMatrix.d4 << "\n";
    //}

    std::cout << std::endl;

    for (UINT j = 0; j < scene->mNumMeshes; j++)
    {
        aiMesh* mesh = scene->mMeshes[i];

        /*reserve memory for vertices*/
        rMeshes.push_back(new MeshRigged());
        rMeshes[i]->vertices.reserve(mesh->mNumVertices);
        estimatedFileSize += mesh->mNumVertices * 76;

        std::cout<<"Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices" << std::endl;

        std::cout << "Input name of material for " << mesh->mName.C_Str() << ": ";
        std::getline(std::cin, rMeshes[i]->materialName);

        if (i > 0 && rMeshes[i]->materialName == "")
        {
            rMeshes[i]->materialName = rMeshes[(INT_PTR)i - 1]->materialName;
        }

        if (rMeshes[i]->materialName == "del")
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
            rMeshes[i]->vertices.push_back(SkinnedVertex(float3(pos.x, pos.y, pos.z),
                                             float2(tex.x, tex.y),
                                          float3(norm.x, norm.y, norm.z),
                                          float3(tangU.x, tangU.y, tangU.z)
            ));

            XMFLOAT3 pV3 = { pos.x, pos.y, pos.z };

            vMin = XMVectorMin(vMin, XMLoadFloat3(&pV3));
            vMax = XMVectorMax(vMax, XMLoadFloat3(&pV3));

        }

        /*apply centering and scaling if needed*/

        XMFLOAT3 center;
        DirectX::XMStoreFloat3(&center, 0.5f * (vMin + vMax));

        std::cout << "Center is " << center.x << ", " << center.y << ", " << center.z << std::endl;

        if (iData.ScaleFactor != 1.0f)
        {
            std::cout << "Scaling with factor " << iData.ScaleFactor << std::endl;
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

                //if (rotateX != 0)
                //{
                //    DirectX::XMFLOAT3 xm = { v.Position.x, v.Position.y, v.Position.z };
                //    XMVECTOR a = XMLoadFloat3(&xm);
                //    XMMATRIX m = XMMatrixRotationX(XM_PIDIV2 * rotateX);
                //    a = XMVector3Transform(a, m);
                //    DirectX::XMStoreFloat3(&xm, a);

                //    v.Position.x = xm.x;
                //    v.Position.y = xm.y;
                //    v.Position.z = xm.z;
                //}

            }
        }

        /*get indices*/
        rMeshes[i]->indices.reserve((long long)(mesh->mNumFaces) * 3);
        estimatedFileSize += (long long)(mesh->mNumFaces) * 3 * sizeof(uint32_t);

        std::cout << "Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumFaces * 3 << " faces\n" << std::endl;


        for (UINT j = 0; j < mesh->mNumFaces; j++)
        {
            rMeshes[i]->indices.push_back(mesh->mFaces[j].mIndices[0]);
            rMeshes[i]->indices.push_back(mesh->mFaces[j].mIndices[1]);
            rMeshes[i]->indices.push_back(mesh->mFaces[j].mIndices[2]);
        }

        i++;
    }

    /*add weights from bones to vertices*/
    for (const auto& b : bones)
    {

        for (UINT k = 0; k < b.AIBone->mNumWeights; k++)
        {
            rMeshes[0]->vertices[b.AIBone->mWeights[k].mVertexId].BlendIndices.push_back(k);
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

    

    auto endTime = std::chrono::high_resolution_clock::now();

    estimatedFileSize += 5;

    for (int i = 0; i < rMeshes.size(); i++)
    {
        estimatedFileSize += 3 * 3 * 4 + 9;
        estimatedFileSize += 6 + 3 * 10;
    }

    std::cout << "Finished loading file.\n";
    std::cout << "Estimated size: " << (estimatedFileSize/1024) << " kbytes (" << estimatedFileSize << " bytes)" << std::endl;

    std::cout << "\n===================================================\n\n";

    return true;
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

    /*number of bones*/
    char boneSize = (char)bones.size();
    fileHandle.write(reinterpret_cast<const char*>(&boneSize), sizeof(char));

    for (const auto& b : bones)
    {
        /*bone id*/
        char boneID = (char)b.Index;
        fileHandle.write(reinterpret_cast<const char*>(&boneID), sizeof(char));

        short boneStrSize = (short)b.Name.size();
        fileHandle.write(reinterpret_cast<const char*>(&boneStrSize), sizeof(boneStrSize));
        fileHandle.write(reinterpret_cast<const char*>(&b.Name[0]), boneStrSize);

        /*bone offset matrix*/
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.a1), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.a2), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.a3), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.a4), sizeof(float));

        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.b1), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.b2), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.b3), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.b4), sizeof(float));

        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.c1), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.c2), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.c3), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.c4), sizeof(float));

        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.d1), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.d2), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.d3), sizeof(float));
        fileHandle.write(reinterpret_cast<const char*>(&b.AIBone->mOffsetMatrix.d4), sizeof(float));


    }

    /*bone hierarchy*/
    for (const auto& b : bones)
    {
        fileHandle.write(reinterpret_cast<const char*>(&b.Index), sizeof(int));
        fileHandle.write(reinterpret_cast<const char*>(&b.ParentIndex), sizeof(int));
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
    char* voidPtr = new char[64];

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
        if (boneHierarchyCheck[i].Index != bones[i].Index)
        {
            timesFailed++;
        }

        if (boneHierarchyCheck[i].ParentIndex != bones[i].ParentIndex)
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
