#pragma comment(lib, "assimp-vc142-mt.lib")

#include "ModelConverter.h"

const int VERSION_MAJOR = 0;
const int VERSION_MINOR = 2;

std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim))
    {
        result.push_back(item);
    }

    return result;
}

int main(int argc, char* argv[])
{
    ModelConverter mConverter;
    InitData initData;

    std::cout << "SkinnedModelConverter S3D " << VERSION_MAJOR << "." << VERSION_MINOR << " (Assimp Version " << mConverter.getVersionString() << ")\n" << std::endl;

    if (argc < 2)
    {
        std::cout << "Not enough parameters!" << std::endl;
        return -1;
    }

    /*command line parameter*/

    initData.FileName = argv[1];

#ifdef _DEBUG
    initData.FileName = "C:\\Users\\n_seh\\Desktop\\blender\\basic.fbx";
#endif


    for (int i = 2; i < argc; i++)
    {
        std::vector<std::string> sVec = split(argv[i], '=');

        if (sVec.size() != 2)
        {
            std::cout << "Unknown parameter " << argv[i] << std::endl;
            continue;
        }

        if (sVec[0] == "-s")
        {
            initData.ScaleFactor = (float)atof(sVec[1].c_str());
        }
        else if (sVec[0] == "-p")
        {
            initData.Prefix = sVec[1];
        }
        else if (sVec[0] == "-c")
        {
            initData.CenterEnabled = (atoi(sVec[1].c_str()) != 0 ? 1 : 0);
        }

    }

    if (!mConverter.load(initData))
    {
        std::cout << "Failed to initialize model converter!" << std::endl;
        return -1;
    }

}