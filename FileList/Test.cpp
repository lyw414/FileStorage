#include "FileList.hpp"

int main(int argc, char * argv[])
{
    std::string cmd;
    char buf[1024] = {0};

    LYW_CODE::FileList fileList("./TestFileList");

    if (argc < 2)
    {
        printf("CMD FORMAT ERROR!\n");
        return 0;
    }

    cmd = argv[1];

    if (cmd == "push_back")
    {
        if (argc >= 3)
        {
            fileList.push_back(argv[2], strlen(argv[2]));
            printf("push back [%s]\n", argv[2]);
        }
        else
        {
            printf("CMD FORMAT ERROR!\n");
        }
    }

    if (cmd == "pop_front")
    {
        unsigned int ret = fileList.pop_front(buf, sizeof(buf));
        printf("pop front Len [%d] [%s]\n", ret , buf);
    }

    if (cmd == "front")
    {
        unsigned int ret = fileList.front(buf, sizeof(buf));
        printf("front Len [%d] [%s]\n", ret , buf);
    }

    if (cmd == "size")
    {
        unsigned int ret = fileList.size();
        printf("size [%d]\n", ret);
    }

    if (cmd == "push_front")
    {
        if (argc >= 3)
        {
            fileList.push_front(argv[2], strlen(argv[2]));
            printf("push front [%s]\n", argv[2]);
        }
        else
        {
            printf("CMD FORMAT ERROR!\n");
        }
    }


    if (cmd == "pop_back")
    {
        unsigned int ret = fileList.pop_back(buf, sizeof(buf));
        printf("pop back Len [%d] [%s]\n", ret , buf);
    }

    if (cmd == "back")
    {
        unsigned int ret = fileList.back(buf, sizeof(buf));
        printf("back Len [%d] [%s]\n", ret , buf);
    }
    
    if (cmd == "show")
    {
        LYW_CODE::FileList::iterator it;

        for (it = fileList.begin(); it != fileList.end(); it++)
        {
            unsigned int ret = it.Data(buf, sizeof(buf));
            printf("Data Len [%d] [%s]\n", ret , buf);
        }

    }
 


    return 0;
}
