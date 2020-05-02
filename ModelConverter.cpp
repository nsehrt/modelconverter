#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
#include <Windows.h>
#pragma comment(lib, "assimp-vc142-mt.lib")
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include <assimp\material.h>
#include <DirectXMath.h>

using namespace DirectX;

/*alternative load for m3d*/
//#define M3D_LOAD

#pragma warning( disable : 26454 )
using namespace std;
typedef unsigned int UINT;
typedef unsigned char BYTE;


struct float3
{
    float3() : x(), y(), z() { RtlSecureZeroMemory(this, sizeof(this)); }
    float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z){}
    float x, y, z;
};

struct float2
{
    float2(float _u, float _v) : u(_u), v(_v){}
    float u, v;
};

struct Vertex
{
    Vertex() : Position(0,0,0), Texture(0,0), Normal(0,0,0), TangentU(0,0,0){}
    Vertex(float3 p, float2 t, float3 n, float3 tU) : Position(p), Texture(t), Normal(n), TangentU(tU){}
    float3 Position;
    float2 Texture;
    float3 Normal;
    float3 TangentU;
};

struct Mesh
{
public:
    vector<Vertex> vertices;
    vector<uint32_t> indices;
    std::string materialName;
};

std::string filePath, fileName;
uint32_t estimatedFileSize = 0;

int main(int argc, char* argv[])
{
    Assimp::Importer fbxImport;
    vector<Mesh*> meshes, vmeshes;

    if (argc < 2)
    {
        cout << "ModelConverter B3D 1.1\nPath to model file: " << flush;
#if _DEBUG
#ifndef M3D_LOAD
        filePath = "C:\\Users\\n_seh\\Desktop\\temp\\pinetree.fbx";
#else
        filePath = "skull.m3d";
#endif
#else
        cin >> filePath;
#endif
        cout << endl;
    }
    else
    {
        std::stringstream ss;
        ss << argv[1];
        filePath = ss.str();
    }



    auto startTime = std::chrono::high_resolution_clock::now();

    char id[1024];
    _splitpath_s(filePath.c_str(), NULL, 0, NULL, 0, id, 1024, NULL, 0);
    fileName = id;

#ifdef M3D_LOAD
    /*************************************************/

    std::ifstream fin(filePath);

    if (!fin)
    {
        return false;
    }

    UINT vcount = 0;
    UINT tcount = 0;
    std::string ignore;

    fin >> ignore >> vcount;
    fin >> ignore >> tcount;
    fin >> ignore >> ignore >> ignore >> ignore;

    meshes.push_back(new Mesh());
    meshes[0]->vertices.resize(vcount);

    for (UINT i = 0; i < vcount; ++i)
    {
        fin >> meshes[0]->vertices[i].Position.x >> meshes[0]->vertices[i].Position.y >> meshes[0]->vertices[i].Position.z;
        meshes[0]->vertices[i].Texture.u = 0.f;
        meshes[0]->vertices[i].Texture.v = 0.f;
        fin >> meshes[0]->vertices[i].Normal.x >> meshes[0]->vertices[i].Normal.y >> meshes[0]->vertices[i].Normal.z;
        meshes[0]->vertices[i].TangentU.x = 0.f;
        meshes[0]->vertices[i].TangentU.y = 0.f;
        meshes[0]->vertices[i].TangentU.z = 0.f;
    }

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;

    int indexCount = 3 * tcount;
    meshes[0]->indices.resize(indexCount);

    for (UINT i = 0; i < tcount; ++i)
    {
        fin >> meshes[0]->indices[i * 3 + 0] >> meshes[0]->indices[i * 3 + 1] >> meshes[0]->indices[i * 3 + 2];
    }
    
    meshes[0]->materialName = "skull";

    fin.close();

    /*************************************************/
#else
    const aiScene* loadedScene = fbxImport.ReadFile(filePath, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);

    if (!loadedScene)
    {
        std::cerr << "Unable to load file " << fileName << ": " << fbxImport.GetErrorString() << std::endl;
        return false;
    }

    /*reserve memory for meshes*/
    meshes.reserve(loadedScene->mNumMeshes);
    
    cout << "Model contains " << loadedScene->mNumMeshes << " meshes!\n" << endl;

    for (UINT i = 0; i < loadedScene->mNumMeshes; i++)
    {
        aiMesh* mesh = loadedScene->mMeshes[i];

        /*reserve memory for vertices*/
        meshes.push_back(new Mesh());
        meshes[i]->vertices.reserve(mesh->mNumVertices);
        estimatedFileSize += mesh->mNumVertices * sizeof(Vertex);

        cout<<"Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumVertices << " vertices" << endl;

        cout << "Input name of material for " << mesh->mName.C_Str() << ": ";
        getline(cin, meshes[i]->materialName);

        if (i > 0 && meshes[i]->materialName == "")
        {
            meshes[i]->materialName = meshes[i - 1]->materialName;
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
            meshes[i]->vertices.push_back(Vertex(float3(pos.x, pos.y, pos.z),
                                             float2(tex.x, tex.y),
                                          float3(norm.x, norm.y, norm.z),
                                          float3(tangU.x, tangU.y, tangU.z)
            ));

            //cout << i << ": " << meshes[i]->vertices.back().Position.x << " " <<meshes[i]->vertices.back().Position.y << " " << meshes[i]->vertices.back().Position.z << endl;

        }

        /*get indices*/
        meshes[i]->indices.reserve((int)(mesh->mNumFaces) * 3);
        estimatedFileSize += (int)(mesh->mNumFaces) * 3 * sizeof(uint32_t);

        cout << "Mesh " << i << " (" << mesh->mName.C_Str() << ") has " << mesh->mNumFaces * 3 << " faces\n" << endl;


        for (UINT j = 0; j < mesh->mNumFaces; j++)
        {
            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[0]);
            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[1]);
            meshes[i]->indices.push_back(mesh->mFaces[j].mIndices[2]);
        }


    }
#endif
    auto endTime = std::chrono::high_resolution_clock::now();

    estimatedFileSize += 5;

    for (int i = 0; i < meshes.size(); i++)
    {
        estimatedFileSize += 3 * 3 * 4 + 9;
        estimatedFileSize += 6 + 3 * 10;
    }

    cout << "Finished loading " << fileName << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms" << endl;
    cout << "Estimated size: " << (estimatedFileSize/1024) << " kbytes (" << estimatedFileSize << " bytes)" << endl;

    /*writing to binary file .b3d*/
    startTime = std::chrono::high_resolution_clock::now();

    std::ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    fileName.append(".b3d");
    auto fileHandle = std::fstream(fileName.c_str(), std::ios::out | std::ios::binary);

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

#ifndef M3D_LOAD
            XMStoreFloat3(&Position, XMVector3Transform(XMLoadFloat3(&Position), XMMatrixRotationX(XM_PIDIV2)));
            XMStoreFloat3(&Normal, XMVector3Transform(XMLoadFloat3(&Normal), XMMatrixRotationX(XM_PIDIV2)));
            XMStoreFloat3(&Tangent, XMVector3Transform(XMLoadFloat3(&Tangent), XMMatrixRotationX(XM_PIDIV2)));
#endif

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
    
    endTime = std::chrono::high_resolution_clock::now();
    cout << "\nFinished writing " << fileName << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms\n" << endl;

    /*verification*/
    cout << "Verifying b3d file is correct...\n";

    /*load b3d file*/
    streampos fileSize;
    ifstream vFile(fileName, std::ios::binary);

    /*file size*/
    vFile.seekg(0, std::ios::end);
    fileSize = vFile.tellg();
    vFile.seekg(0, std::ios::beg);

    /*check header*/
    bool hr = true;
    cout << "header...\t";

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
        cout << "not a b3d file\n";
    }
    else
    {
        cout << "ok\n";
    }

    /*num meshes*/
    cout << "num meshes...\t";
    char vNumMeshes = 0;
    vFile.read((char*)(&vNumMeshes), sizeof(char));

    if (vNumMeshes == meshes.size())
    {
        cout << "ok (" << (int)vNumMeshes << ")\n";
    }
    else
    {
        cout << "failed\n";
    }

    for (char i = 0; i < vNumMeshes; i++)
    {
        vmeshes.push_back(new Mesh());

        /*material*/
        cout << "material...\t";

        short slen = 0;
        vFile.read((char*)(&slen), sizeof(short));

        char* mat = new char[slen + 1];
        vFile.read(mat, slen);
        mat[slen] = '\0';

        if (meshes[i]->materialName.compare(mat) == 0)
        {
            cout << "ok (" << mat << ")\n";
        }
        else
        {
            cout << "failed" << endl;
        }


        /*num vertices*/
        cout << "num vertices...\t";

        int vnVert = 0;
        vFile.read((char*)(&vnVert), sizeof(int));

        if (vnVert == meshes[i]->vertices.size())
        {
            cout << "ok (" << vnVert << ")\n";
        }
        else
        {
            cout << "failed" << endl;
        }

        vmeshes[i]->vertices.resize(vnVert);

        /*vertices*/
        cout << "vertices...\t";

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


        if(vmeshes[i]->vertices[0].Position.z == vmeshes[i]->vertices[0].Position.z &&
           vmeshes[i]->vertices[0].TangentU.x == vmeshes[i]->vertices[0].TangentU.x)
        {
            cout << "ok" << endl;
        }
        else
        {
            cout << "failed" << endl;
        }

        

        /*num indices*/
        cout << "num indices...\t";

        int vInd = 0;
        vFile.read((char*)(&vInd), sizeof(int));

        if (vInd == meshes[i]->indices.size())
        {
            cout << "ok (" << vInd << ")\n";
        }
        else
        {
            cout << "failed - " << vInd << " vs " << meshes[i]->indices.size() << endl;
        }

        /*indices*/
        cout << "indices...\t";

        vmeshes[i]->indices.resize(vInd);

        for (int j = 0; j < vInd; j++)
        {
            vFile.read((char*)(&vmeshes[i]->indices[j]), sizeof(int));
        }

        
        if (vmeshes[i]->indices == meshes[i]->indices)
        {
            cout << "ok" << endl;
        }
        else
        {
            cout << "failed" << endl;
        }


    }

    return true;

}