#include "modelconverter.h"

const int VERSION_MAJOR = 1;
const int VERSION_MINOR = 1;

int main(int argc, char* argv[])
{
    ModelConverter mConverter;
    InitData initData;

    std::cout << std::setprecision(2);
    std::cout << "ModelConverter B3D/S3D/CLP " << VERSION_MAJOR << "." << VERSION_MINOR << " (Assimp Version " << mConverter.getVersionString() << ")\n" << std::endl;
    std::cout << "===================================================\n\n";

    /*help dialog*/
    if (argc < 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "-help")
    {
        std::string empty;
        std::cout << "First parameter must be path to file or -h!\n";
        std::cout << "\nPossible parameters:\n";
        std::cout << "-h\t- Help dialog\n-nc\t- Do not center the model (rigged models are never centered)\n-fs\t- Force a static model\n-ft\t- Force transformed vertices (only rigged models)\n-s\t- Scale the model by a factor (-s=2)\n-p\t- Prefix the output file with the entered string (-p=PRE_)\n-o\t- Print the data of a b3d/s3d/clp file (-ov for verbose output)\n" << std::endl;
        std::getline(std::cin, empty);
        return 0;
    }

    /*command line parameter*/
    initData.fileName = argv[1];

#ifdef _DEBUG
    initData.fileName = "C:\\Users\\n_seh\\Desktop\\blender\\geo\\geo_walk.fbx";
#endif

    for (int i = 2; i < argc; i++)
    {
        std::vector<std::string> sVec = split(argv[i], '=');

        if (sVec.size() == 1)
        {
            if (sVec[0] == "-nc")
            {
                initData.centerEnabled = 0;
            }
            else if (sVec[0] == "-fs")
            {
                initData.forceStatic = true;
            }
            else if (sVec[0] == "-ft")
            {
                initData.forceTransform = true;
            }
            else if (sVec[0] == "-o")
            {
                mConverter.printFile(initData.fileName, false);
                return 0;
            }
            else if (sVec[0] == "-ov")
            {
                mConverter.printFile(initData.fileName, true);
                return 0;
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
                initData.scaleFactor = (float)atof(sVec[1].c_str());
            }
            else if (sVec[0] == "-p")
            {
                initData.prefix = sVec[1];
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

    if (!mConverter.process(initData))
    {
        std::cerr << "Model conversion failed!" << std::endl;
        return -1;
    }

    std::cout << "\n===================================================\n";

    return 0;
}