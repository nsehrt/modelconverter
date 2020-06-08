#pragma comment(lib, "assimp-vc142-mt.lib")

#include "ModelConverter.h"

const int VERSION_MAJOR = 0;
const int VERSION_MINOR = 7;

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
    std::cout << "===================================================\n\n";

    /*help dialog*/
    if (argc < 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "-help")
    {
        std::cout << "First parameter must be path to file or -h!\n";
        std::cout << "\nPossible parameters:\n";
        std::cout << "-h\t- Help dialog\n-c\t- Center the model\n-s\t- Scale the model by this factor\n-p\t- Prefix the output file with the entered string\n" << std::endl;
        return 0;
    }


    /*command line parameter*/

    initData.FileName = argv[1];

#ifdef _DEBUG
    initData.FileName = "C:\\Users\\n_seh\\Desktop\\blender\\geo.fbx";
#endif


    for (int i = 2; i < argc; i++)
    {
        std::vector<std::string> sVec = split(argv[i], '=');

        if (sVec.size() == 1)
        {
            if (sVec[0] == "-c")
            {
                initData.CenterEnabled = 1;
            }
            else
            {
                std::cerr << "Unknown parameter " << argv[i] << std::endl;
                continue;
            }
        }
        else if (sVec.size() == 2)
        {
            if (sVec[0] == "-s")
            {
                initData.ScaleFactor = (float)atof(sVec[1].c_str());
            }
            else if (sVec[0] == "-p")
            {
                initData.Prefix = sVec[1];
            }
            else
            {
                std::cerr << "Unknown parameter " << argv[i] << std::endl;
                continue;
            }
        }
        else
        {
            std::cerr << "Unknown parameter " << argv[i] << std::endl;
            continue;
        }


    }

    if (!mConverter.load(initData))
    {
        std::cerr << "Model conversion failed!" << std::endl;
        return -1;
    }

}