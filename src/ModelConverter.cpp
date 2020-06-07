#pragma comment(lib, "assimp-vc142-mt.lib")

#include "ModelConverter.h"


using namespace DirectX;


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




//int main(int argc, char* argv[])
//{
//
//
//    /*reserve memory for meshes*/
//    meshes.reserve(loadedScene->mNumMeshes);
//
//    std::cout << "Model contains " << loadedScene->mNumMeshes << " meshes!\n\n" << std::endl;
//
//    int i = 0;
//
//    XMFLOAT3 cMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
//    XMFLOAT3 cMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
//
//    XMVECTOR vMin = XMLoadFloat3(&cMin);
//    XMVECTOR vMax = XMLoadFloat3(&cMax);
//
//    /*load bones from meshes*/
//
//    std::vector<UINT> totalWeight(loadedScene->mNumMeshes);
//    std::vector<float> totalWeightSum(loadedScene->mNumMeshes);
//
//    for (UINT i = 0; i < loadedScene->mNumMeshes; i++)
//    {
//        aiMesh* fMesh = loadedScene->mMeshes[i];
//        std::cout << "Processing mesh " << fMesh->mName.C_Str() << " (" << fMesh->mNumBones << " Bones)." << std::endl;
//
//        for (UINT j = 0; j < fMesh->mNumBones; j++)
//        {
//            totalWeight[i] += fMesh->mBones[j]->mNumWeights;
//
//            for (UINT k = 0; k < fMesh->mBones[j]->mNumWeights; k++)
//            {
//                totalWeightSum[i] += fMesh->mBones[j]->mWeights[k].mWeight;
//            }
//
//            if (existsBoneByName(bones, fMesh->mBones[j]->mName.C_Str()))
//            {
//                continue;
//            }
//
//            Bone b;
//
//            b.AIBone = fMesh->mBones[j];
//            b.Name = fMesh->mBones[j]->mName.C_Str();
//            b.Index = j;
//
//            bones.push_back(b);
//        }
//
//    }
//
//    for (UINT i = 0; i < loadedScene->mNumMeshes; i++)
//    {
//        std::cout << "Mesh " << i << " has a total of " << totalWeight[i] << " Weights (" << totalWeightSum[i] << ")" << std::endl;
//    }
//
//    std::cout << "Found a total of " << bones.size() << " bones." << std::endl;
//
//    aiNode* rootNode = loadedScene->mRootNode;
//    
//    for (auto& b : bones)
//    {
//        aiNode* fNode = rootNode->FindNode(b.Name.c_str());
//
//        if (fNode)
//        {
//            
//            b.ParentIndex = findParentBone(bones, fNode);
//            if(b.ParentIndex >= 0)
//            b.ParentName = bones[b.ParentIndex].Name;
//
//        }
//        else
//        {
//            b.ParentIndex = -1;
//        }
//    }
//
//    std::sort(bones.begin(), bones.end(), [](const Bone& a, const Bone& b)->bool
//              {
//                  if (a.ParentIndex == b.ParentIndex)
//                  {
//                      return a.Index < b.Index;
//                  }
//                  else
//                  {
//                      return a.ParentIndex < b.ParentIndex;
//                  }
//                  
//              });
//
//    for (int i = 0; i < bones.size(); i++)
//    {
//        bones[i].Index = i;
//    }
//
//    for (int i = 0; i < bones.size(); i++)
//    {
//        bones[i].ParentIndex = findIndexInBones(bones, bones[i].ParentName);
//    }
//
//    std::vector<int> testHierarchy;
//
//    testHierarchy.push_back(bones[0].Index);
//    bool hTestFailed = false;
//
//    for (int i = 1; i < bones.size(); i++)
//    {
//        bool found = false;
//
//        for (int j = 0; j < testHierarchy.size(); j++)
//        {
//            if (bones[i].ParentIndex == testHierarchy[j])
//                found = true;
//        }
//
//        if (!found)
//        {
//            std::cout << "Failed hierarchy test!\n\n";
//            hTestFailed = true;
//        }
//        else
//        {
//            testHierarchy.push_back(bones[i].Index);
//        }
//
//    }
//
//    if(!hTestFailed)
//    std::cout << "Hierarchy test successful\n\n";
//
//    for (const auto& b : bones)
//    {
//        std::cout << "Bone " << b.Index << " (" << b.Name << ") is a child of " << b.ParentIndex << " (" << (b.ParentIndex > -1 ? bones[b.ParentIndex].Name : "-1") << ")" << std::endl;
//        std::cout << b.AIBone->mOffsetMatrix.a1 << " " << b.AIBone->mOffsetMatrix.a2 << " " << b.AIBone->mOffsetMatrix.a3 << " " << b.AIBone->mOffsetMatrix.a4 << "\n";
//        std::cout << b.AIBone->mOffsetMatrix.b1 << " " << b.AIBone->mOffsetMatrix.b2 << " " << b.AIBone->mOffsetMatrix.b3 << " " << b.AIBone->mOffsetMatrix.b4 << "\n";
//        std::cout << b.AIBone->mOffsetMatrix.c1 << " " << b.AIBone->mOffsetMatrix.c2 << " " << b.AIBone->mOffsetMatrix.c3 << " " << b.AIBone->mOffsetMatrix.c4 << "\n";
//        std::cout << b.AIBone->mOffsetMatrix.d1 << " " << b.AIBone->mOffsetMatrix.d2 << " " << b.AIBone->mOffsetMatrix.d3 << " " << b.AIBone->mOffsetMatrix.d4 << "\n";
//    }
//
//    std::cout << std::endl;
//
//    for (UINT j = 0; j < loadedScene->mNumMeshes; j++)
//    {
//        aiMesh* mesh = loadedScene->mMeshes[i];
//
//        /*reserve memory for vertices*/
//        meshes.push_back(new Mesh());
//        meshes[i]->vertices.reserve(mesh->mNumVertices);
//        estimatedFileSize += mesh->mNumVertices * sizeof(Vertex);
//
//        std::cout<<"Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices" << std::endl;
//
//        std::cout << "Input name of material for " << mesh->mName.C_Str() << ": ";
//        std::getline(std::cin, meshes[i]->materialName);
//
//        if (i > 0 && meshes[i]->materialName == "")
//        {
//            meshes[i]->materialName = meshes[i - 1]->materialName;
//        }
//
//        if (meshes[i]->materialName == "del")
//        {
//            meshes.pop_back();
//            continue;
//        }
//
//        /*get vertices: positions normals tex coords and tangentu */
//
//        for (UINT v = 0; v < mesh->mNumVertices; v++)
//        {
//
//            aiVector3D tex(0);
//            aiVector3D tangU(0);
//            aiVector3D pos = mesh->mVertices[v];
//            aiVector3D norm = mesh->mNormals[v];
//
//            if (mesh->HasTangentsAndBitangents() != 0)
//            {
//                tangU = mesh->mTangents[v];
//            }
//            
//            tex = mesh->mTextureCoords[0][v];
//            
//            /*convert to vertex data format*/
//            meshes[i]->vertices.push_back(Vertex(float3(pos.x, pos.y, pos.z),
//                                             float2(tex.x, tex.y),
//                                          float3(norm.x, norm.y, norm.z),
//                                          float3(tangU.x, tangU.y, tangU.z)
//            ));
//
//            XMFLOAT3 pV3 = { pos.x, pos.y, pos.z };
//
//            vMin = XMVectorMin(vMin, XMLoadFloat3(&pV3));
//            vMax = XMVectorMax(vMax, XMLoadFloat3(&pV3));
//
//        }
//
//        //XMFLOAT3 center;
//        //DirectX::XMStoreFloat3(&center, 0.5f * (vMin + vMax));
//
//        //std::cout << "Center is " << center.x << ", " << center.y << ", " << center.z << std::endl;
//        //if (scaleFactor != 0.0f)
//        //{
//        //    std::cout << "Scaling with factor " << scaleFactor << std::endl;
//        //}
//
//        //for (auto& m : meshes)
//        //{
//        //    for (auto& v : m->vertices)
//        //    {
//        //        //v.Position.x -= center.x;
//        //        //v.Position.y -= center.y;
//        //        //v.Position.z -= center.z;
//
//
//        //        if (scaleFactor != 0.0f)
//        //        {
//        //            DirectX::XMFLOAT3 xm = { v.Position.x, v.Position.y, v.Position.z };
//        //            XMVECTOR a = XMLoadFloat3(&xm);
//        //            a = XMVectorScale(a, scaleFactor);
//        //            DirectX::XMStoreFloat3(&xm, a);
//
//        //            v.Position.x = xm.x;
//        //            v.Position.y = xm.y;
//        //            v.Position.z = xm.z;
//        //        }
//
//        //        if (rotateX != 0)
//        //        {
//        //            DirectX::XMFLOAT3 xm = { v.Position.x, v.Position.y, v.Position.z };
//        //            XMVECTOR a = XMLoadFloat3(&xm);
//        //            XMMATRIX m = XMMatrixRotationX(XM_PIDIV2 * rotateX);
//        //            a = XMVector3Transform(a, m);
//        //            DirectX::XMStoreFloat3(&xm, a);
//
//        //            v.Position.x = xm.x;
//        //            v.Position.y = xm.y;
//        //            v.Position.z = xm.z;
//        //        }
//
//        //    }
//        //}
//
//        /*get indices*/
//        meshes[i]->indices.reserve((int)(mesh->mNumFaces) * 3);
//        estimatedFileSize += (int)(mesh->mNumFaces) * 3 * sizeof(uint32_t);
//
//        std::cout << "Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumFaces * 3 << " faces\n" << std::endl;
//
//
//        for (UINT j = 0; j < mesh->mNumFaces; j++)
//        {
//            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[0]);
//            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[1]);
//            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[2]);
//        }
//
//        i++;
//    }
//
//    /*add weights from bones to vertices*/
//    for (const auto& b : bones)
//    {
//
//        for (UINT k = 0; k < b.AIBone->mNumWeights; k++)
//        {
//            meshes[0]->vertices[b.AIBone->mWeights[k].mVertexId].BlendIndices.push_back(k);
//            meshes[0]->vertices[b.AIBone->mWeights[k].mVertexId].BlendWeights.push_back(b.AIBone->mWeights[k].mWeight);
//        }
//
//    }
//
//    for (const auto& v : meshes[0]->vertices)
//    {
//        if (v.BlendIndices.size() > 4)
//        {
//            std::cout << "Illegal amount of blend indices!" << std::endl;
//        }
//
//        float acc = 0.0f;
//
//        for (const auto& f : v.BlendWeights)
//        {
//            acc += f;
//        }
//
//        if (acc > 1.01f)
//        {
//            std::cout << "Blend Weight sum over 1! " << acc << std::endl;
//        }
//
//        if (acc < 0.9f)
//        {
//            std::cout << "Blend Weight sum under 0.9! " << acc << std::endl;
//        }
//
//    }
//
//    
//
//    auto endTime = std::chrono::high_resolution_clock::now();
//
//    estimatedFileSize += 5;
//
//    for (int i = 0; i < meshes.size(); i++)
//    {
//        estimatedFileSize += 3 * 3 * 4 + 9;
//        estimatedFileSize += 6 + 3 * 10;
//    }
//
//    std::cout << "Finished loading " << fileName << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms" << std::endl;
//    std::cout << "Estimated size: " << (estimatedFileSize/1024) << " kbytes (" << estimatedFileSize << " bytes)" << std::endl;
//
//    /*writing to binary file .b3d*/
//    startTime = std::chrono::high_resolution_clock::now();
//
//    std::ios_base::sync_with_stdio(false);
//    std::cin.tie(NULL);
//
//    fileName.append(".s3d");
//    fileName = prefix + fileName;
//    auto fileHandle = std::fstream(fileName.c_str(), std::ios::out | std::ios::binary);
//
//    /*header*/
//    char header[4] = { 0x73, 0x33, 0x64, 0x66 };
//    fileHandle.write(header, 4);
//
//    /*number of meshes*/
//    char meshSize = (char)meshes.size();
//    fileHandle.write(reinterpret_cast<const char*>(&meshSize), sizeof(char));
//
//    for (char i = 0; i < meshSize; i++)
//    {
//        /*material name*/
//        short stringSize = (short)meshes[i]->materialName.size();
//        fileHandle.write(reinterpret_cast<const char*>(&stringSize), sizeof(stringSize));
//        fileHandle.write(reinterpret_cast<const char*>(&meshes[i]->materialName[0]), stringSize);
//
//        /*num vertices*/
//        int verticesSize = (int)meshes[i]->vertices.size();
//        fileHandle.write(reinterpret_cast<const char*>(&verticesSize), sizeof(int));
//
//        /*vertices*/
//        for (int v = 0; v < verticesSize; v++)
//        {
//            XMFLOAT3 Position;
//            Position.x = meshes[i]->vertices[v].Position.x;
//            Position.y = meshes[i]->vertices[v].Position.y;
//            Position.z = meshes[i]->vertices[v].Position.z;
//
//            XMFLOAT3 Normal;
//            Normal.x = meshes[i]->vertices[v].Normal.x;
//            Normal.y = meshes[i]->vertices[v].Normal.y;
//            Normal.z = meshes[i]->vertices[v].Normal.z;
//
//            XMFLOAT3 Tangent;
//            Tangent.x = meshes[i]->vertices[v].TangentU.x;
//            Tangent.y = meshes[i]->vertices[v].TangentU.y;
//            Tangent.z = meshes[i]->vertices[v].TangentU.z;
//
//#ifndef M3D_LOAD
//            DirectX::XMStoreFloat3(&Position, XMVector3Transform(XMLoadFloat3(&Position), XMMatrixRotationX(XM_PIDIV2)));
//            DirectX::XMStoreFloat3(&Normal, XMVector3Transform(XMLoadFloat3(&Normal), XMMatrixRotationX(XM_PIDIV2)));
//            DirectX::XMStoreFloat3(&Tangent, XMVector3Transform(XMLoadFloat3(&Tangent), XMMatrixRotationX(XM_PIDIV2)));
//#endif
//
//            /*position*/
//            fileHandle.write(reinterpret_cast<const char*>(&Position.x), sizeof(float));
//            fileHandle.write(reinterpret_cast<const char*>(&Position.y), sizeof(float));
//            fileHandle.write(reinterpret_cast<const char*>(&Position.z), sizeof(float));
//            /*texture coordinates*/
//            fileHandle.write(reinterpret_cast<const char*>(&meshes[i]->vertices[v].Texture.u), sizeof(float));
//            fileHandle.write(reinterpret_cast<const char*>(&meshes[i]->vertices[v].Texture.v), sizeof(float));
//            /*normals*/
//            fileHandle.write(reinterpret_cast<const char*>(&Normal.x), sizeof(float));
//            fileHandle.write(reinterpret_cast<const char*>(&Normal.y), sizeof(float));
//            fileHandle.write(reinterpret_cast<const char*>(&Normal.z), sizeof(float));
//            /*tangentU*/
//            fileHandle.write(reinterpret_cast<const char*>(&Tangent.x), sizeof(float));
//            fileHandle.write(reinterpret_cast<const char*>(&Tangent.y), sizeof(float));
//            fileHandle.write(reinterpret_cast<const char*>(&Tangent.z), sizeof(float));
//
//        }
//
//
//        /*num indices*/
//        int indicesSize = (int)meshes[i]->indices.size();
//        fileHandle.write(reinterpret_cast<const char*>(&indicesSize), sizeof(int));
//
//        /*indices*/
//        for (int j = 0; j < indicesSize; j++)
//        {
//            fileHandle.write(reinterpret_cast<const char*>(&meshes[i]->indices[j]), sizeof(uint32_t));
//        }
//
//    }
//
//    fileHandle.close();
//    
//    endTime = std::chrono::high_resolution_clock::now();
//    std::cout << "\nFinished writing " << fileName << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms\n" << std::endl;
//
//    /*verification*/
//    std::cout << "Verifying s3d file is correct...\n";
//
//    /*load b3d file*/
//    std::streampos fileSize;
//    std::ifstream vFile(fileName, std::ios::binary);
//
//    /*file size*/
//    vFile.seekg(0, std::ios::end);
//    fileSize = vFile.tellg();
//    vFile.seekg(0, std::ios::beg);
//
//    /*check header*/
//    bool hr = true;
//    std::cout << "header...\t";
//
//    char buff[4] = { 's', '3', 'd', 'f' };
//
//    for (int i = 0; i < 4; i++)
//    {
//        char t;
//        vFile.read(&t, sizeof(char));
//
//        if (t != buff[i])
//        {
//            hr = false;
//            break;
//        }
//    }
//
//    if (hr == false)
//    {
//        std::cout << "not a s3d file\n";
//    }
//    else
//    {
//        std::cout << "ok\n";
//    }
//
//    /*num meshes*/
//    std::cout << "num meshes...\t";
//    char vNumMeshes = 0;
//    vFile.read((char*)(&vNumMeshes), sizeof(char));
//
//    if (vNumMeshes == meshes.size())
//    {
//        std::cout << "ok (" << (int)vNumMeshes << ")\n";
//    }
//    else
//    {
//        std::cout << "failed\n";
//    }
//
//    for (char i = 0; i < vNumMeshes; i++)
//    {
//        vmeshes.push_back(new Mesh());
//
//        /*material*/
//        std::cout << "material...\t";
//
//        short slen = 0;
//        vFile.read((char*)(&slen), sizeof(short));
//
//        char* mat = new char[slen + 1];
//        vFile.read(mat, slen);
//        mat[slen] = '\0';
//
//        if (meshes[i]->materialName.compare(mat) == 0)
//        {
//            std::cout << "ok (" << mat << ")\n";
//        }
//        else
//        {
//            std::cout << "failed" << std::endl;
//        }
//
//
//        /*num vertices*/
//        std::cout << "num vertices...\t";
//
//        int vnVert = 0;
//        vFile.read((char*)(&vnVert), sizeof(int));
//
//        if (vnVert == meshes[i]->vertices.size())
//        {
//            std::cout << "ok (" << vnVert << ")\n";
//        }
//        else
//        {
//            std::cout << "failed" << std::endl;
//        }
//
//        vmeshes[i]->vertices.resize(vnVert);
//
//        /*vertices*/
//        std::cout << "vertices...\t";
//
//        for (int j = 0; j < vnVert; j++)
//        {
//            vFile.read((char*)(&vmeshes[i]->vertices[j].Position.x), sizeof(float));
//            vFile.read((char*)(&vmeshes[i]->vertices[j].Position.y), sizeof(float));
//            vFile.read((char*)(&vmeshes[i]->vertices[j].Position.z), sizeof(float));
//
//            vFile.read((char*)(&vmeshes[i]->vertices[j].Texture.u), sizeof(float));
//            vFile.read((char*)(&vmeshes[i]->vertices[j].Texture.v), sizeof(float));
//
//            vFile.read((char*)(&vmeshes[i]->vertices[j].Normal.x), sizeof(float));
//            vFile.read((char*)(&vmeshes[i]->vertices[j].Normal.y), sizeof(float));
//            vFile.read((char*)(&vmeshes[i]->vertices[j].Normal.z), sizeof(float));
//
//            vFile.read((char*)(&vmeshes[i]->vertices[j].TangentU.x), sizeof(float));
//            vFile.read((char*)(&vmeshes[i]->vertices[j].TangentU.y), sizeof(float));
//            vFile.read((char*)(&vmeshes[i]->vertices[j].TangentU.z), sizeof(float));
//        }
//
//
//        if(vmeshes[i]->vertices[0].Position.z == vmeshes[i]->vertices[0].Position.z &&
//           vmeshes[i]->vertices[0].TangentU.x == vmeshes[i]->vertices[0].TangentU.x)
//        {
//            std::cout << "ok" << std::endl;
//        }
//        else
//        {
//            std::cout << "failed" << std::endl;
//        }
//
//        
//
//        /*num indices*/
//        std::cout << "num indices...\t";
//
//        int vInd = 0;
//        vFile.read((char*)(&vInd), sizeof(int));
//
//        if (vInd == meshes[i]->indices.size())
//        {
//            std::cout << "ok (" << vInd << ")\n";
//        }
//        else
//        {
//            std::cout << "failed - " << vInd << " vs " << meshes[i]->indices.size() << std::endl;
//        }
//
//        /*indices*/
//        std::cout << "indices...\t";
//
//        vmeshes[i]->indices.resize(vInd);
//
//        for (int j = 0; j < vInd; j++)
//        {
//            vFile.read((char*)(&vmeshes[i]->indices[j]), sizeof(int));
//        }
//
//        
//        if (vmeshes[i]->indices == meshes[i]->indices)
//        {
//            std::cout << "ok" << std::endl;
//        }
//        else
//        {
//            std::cout << "failed" << std::endl;
//        }
//
//
//    }
//
//    return true;
//
//}

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
        processStatus = processRigged(loadedScene);
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
            std::cerr << "Failed to verify static model!" << writeFileName << "!" << std::endl;
            return processStatus;
        }

    }

    return processStatus;
}

/*load and save a static model*/
bool ModelConverter::loadStatic(const aiScene* scene)
{
    std::cout << "Processing model in static mode.\n" << std::endl;

    std::cout << iData << "\n===================================================\n\n";

    meshes.reserve(scene->mNumMeshes);

    std::cout << "Model contains " << scene->mNumMeshes << " mesh(es)!\n" << std::endl;

    int i = 0;

    XMFLOAT3 cMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
    XMFLOAT3 cMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    XMVECTOR vMin = XMLoadFloat3(&cMin);
    XMVECTOR vMax = XMLoadFloat3(&cMax);

    for (UINT j = 0; j < scene->mNumMeshes; j++)
    {
        aiMesh* mesh = scene->mMeshes[i];

        /*reserve memory for vertices*/
        meshes.push_back(new Mesh());
        meshes[i]->vertices.reserve(mesh->mNumVertices);
        estimatedFileSize += mesh->mNumVertices * sizeof(Vertex);

        std::cout << "Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices" << std::endl;

        std::cout << "Input name of material for " << mesh->mName.C_Str() << ": ";
        getline(std::cin, meshes[i]->materialName);

        if (i > 0 && meshes[i]->materialName == "")
        {
            meshes[i]->materialName = meshes[(INT_PTR)i - 1]->materialName;
        }

        if (meshes[i]->materialName == "del")
        {
            meshes.pop_back();
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
            meshes[i]->vertices.push_back(Vertex(float3(pos.x, pos.y, pos.z),
                                          float2(tex.x, tex.y),
                                          float3(norm.x, norm.y, norm.z),
                                          float3(tangU.x, tangU.y, tangU.z)
            ));

            XMFLOAT3 pV3 = { pos.x, pos.y, pos.z };

            vMin = XMVectorMin(vMin, XMLoadFloat3(&pV3));
            vMax = XMVectorMax(vMax, XMLoadFloat3(&pV3));

        }

        XMFLOAT3 center;
        DirectX::XMStoreFloat3(&center, 0.5f * (vMin + vMax));

        std::cout << "Center is " << center.x << ", " << center.y << ", " << center.z << std::endl;
        if (iData.ScaleFactor != 1.0f)
        {
            std::cout << "Scaling with factor " << iData.ScaleFactor << std::endl;
        }

        for (auto& m : meshes)
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
                    XMStoreFloat3(&xm, a);

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
                //    XMStoreFloat3(&xm, a);

                //    v.Position.x = xm.x;
                //    v.Position.y = xm.y;
                //    v.Position.z = xm.z;
                //}

            }
        }

        /*get indices*/
        meshes[i]->indices.reserve((INT_PTR)(mesh->mNumFaces) * 3);
        estimatedFileSize += (INT_PTR)(mesh->mNumFaces) * sizeof(uint32_t);

        std::cout << "Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumFaces << " faces\n" << std::endl;


        for (UINT j = 0; j < mesh->mNumFaces; j++)
        {
            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[0]);
            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[1]);
            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[2]);
        }

        i++;
    }

    auto endTime = std::chrono::high_resolution_clock::now();

    estimatedFileSize += 5;

    for (int i = 0; i < meshes.size(); i++)
    {
        estimatedFileSize += 3 * 3 * 4 + 9;
        estimatedFileSize += 6 + 3 * 10;
    }

    std::cout << "Finished loading " << iData.FileName << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms" << std::endl;
    std::cout << "Estimated size: " << (estimatedFileSize / 1024) << " kbytes (" << estimatedFileSize << " bytes)" << std::endl;

    return true;
}

bool ModelConverter::processRigged(const aiScene* scene)
{
    return false;
}

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
    char meshSize = (char)meshes.size();
    fileHandle.write(reinterpret_cast<const char*>(&meshSize), sizeof(char));

    for (char i = 0; i < meshSize; i++)
    {
        /*material name*/
        short stringSize = (short)meshes[i]->materialName.size();
        fileHandle.write(reinterpret_cast<const char*>(&stringSize), sizeof(stringSize));
        fileHandle.write(reinterpret_cast<const char*>(&meshes[i]->materialName[0]), stringSize);

        /*num vertices*/
        int verticesSize = (int)meshes[i]->vertices.size();
        fileHandle.write(reinterpret_cast<const char*>(&verticesSize), sizeof(int));

        /*vertices*/
        for (int v = 0; v < verticesSize; v++)
        {
            XMFLOAT3 Position;
            Position.x = meshes[i]->vertices[v].Position.x;
            Position.y = meshes[i]->vertices[v].Position.y;
            Position.z = meshes[i]->vertices[v].Position.z;

            XMFLOAT3 Normal;
            Normal.x = meshes[i]->vertices[v].Normal.x;
            Normal.y = meshes[i]->vertices[v].Normal.y;
            Normal.z = meshes[i]->vertices[v].Normal.z;

            XMFLOAT3 Tangent;
            Tangent.x = meshes[i]->vertices[v].TangentU.x;
            Tangent.y = meshes[i]->vertices[v].TangentU.y;
            Tangent.z = meshes[i]->vertices[v].TangentU.z;

            /*position*/
            fileHandle.write(reinterpret_cast<const char*>(&Position.x), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Position.y), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&Position.z), sizeof(float));
            /*texture coordinates*/
            fileHandle.write(reinterpret_cast<const char*>(&meshes[i]->vertices[v].Texture.u), sizeof(float));
            fileHandle.write(reinterpret_cast<const char*>(&meshes[i]->vertices[v].Texture.v), sizeof(float));
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
        int indicesSize = (int)meshes[i]->indices.size();
        fileHandle.write(reinterpret_cast<const char*>(&indicesSize), sizeof(int));

        /*indices*/
        for (int j = 0; j < indicesSize; j++)
        {
            fileHandle.write(reinterpret_cast<const char*>(&meshes[i]->indices[j]), sizeof(uint32_t));
        }

    }

    fileHandle.close();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "\nFinished writing " << writeFileName << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms\n" << std::endl;

    return true;
}

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

    if (vNumMeshes == meshes.size())
    {
        std::cout << "ok (" << (int)vNumMeshes << ")\n";
    }
    else
    {
        std::cout << "failed\n";
    }

    for (char i = 0; i < vNumMeshes; i++)
    {
        vmeshes.push_back(new Mesh());

        /*material*/
        std::cout << "material...\t";

        short slen = 0;
        vFile.read((char*)(&slen), sizeof(short));

        char* mat = new char[(INT_PTR)slen + 1];
        vFile.read(mat, slen);
        mat[slen] = '\0';

        if (meshes[i]->materialName.compare(mat) == 0)
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

        if (vnVert == meshes[i]->vertices.size())
        {
            std::cout << "ok (" << vnVert << ")\n";
        }
        else
        {
            std::cout << "failed" << std::endl;
        }

        vmeshes[i]->vertices.resize(vnVert);

        /*vertices*/
        std::cout << "vertices...\t";

        for (int j = 0; j < vnVert; j++)
        {
            vFile.read((char*)(&vmeshes[i]->vertices[j].Position.x), sizeof(float));
            vFile.read((char*)(&vmeshes[i]->vertices[j].Position.y), sizeof(float));
            vFile.read((char*)(&vmeshes[i]->vertices[j].Position.z), sizeof(float));

            vFile.read((char*)(&vmeshes[i]->vertices[j].Texture.u), sizeof(float));
            vFile.read((char*)(&vmeshes[i]->vertices[j].Texture.v), sizeof(float));

            vFile.read((char*)(&vmeshes[i]->vertices[j].Normal.x), sizeof(float));
            vFile.read((char*)(&vmeshes[i]->vertices[j].Normal.y), sizeof(float));
            vFile.read((char*)(&vmeshes[i]->vertices[j].Normal.z), sizeof(float));

            vFile.read((char*)(&vmeshes[i]->vertices[j].TangentU.x), sizeof(float));
            vFile.read((char*)(&vmeshes[i]->vertices[j].TangentU.y), sizeof(float));
            vFile.read((char*)(&vmeshes[i]->vertices[j].TangentU.z), sizeof(float));
        }


        if (vmeshes[i]->vertices[0].Position.z == vmeshes[i]->vertices[0].Position.z &&
            vmeshes[i]->vertices[0].TangentU.x == vmeshes[i]->vertices[0].TangentU.x)
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

        if (vInd == meshes[i]->indices.size())
        {
            std::cout << "ok (" << vInd << ")\n";
        }
        else
        {
            std::cout << "failed - " << vInd << " vs " << meshes[i]->indices.size() << std::endl;
        }

        /*indices*/
        std::cout << "indices...\t";

        vmeshes[i]->indices.resize(vInd);

        for (int j = 0; j < vInd; j++)
        {
            vFile.read((char*)(&vmeshes[i]->indices[j]), sizeof(int));
        }


        if (vmeshes[i]->indices == meshes[i]->indices)
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
