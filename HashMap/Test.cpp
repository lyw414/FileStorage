#include "FileStrHashMap.hpp"
#include <string.h> 
int main(int argc, char ** argv)
{
    LYW_CODE::FileHashMap m_map("/usr/local/fuse/0/HashMapFile");

    LYW_CODE::FileHashMap::iterator it;

    char buf[1024] = {0};
    if (argc < 2)
    {
        return 0;
    }

    if (strcmp(argv[1], "add") == 0)
    {
        m_map.add(argv[2], strlen(argv[2]), (void *)argv[3], strlen(argv[3]));
        printf("Add Key [%s] value [%s]\n", argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "find") == 0)
    {
        m_map.find(argv[2],strlen(argv[2]), buf, sizeof(buf));
        printf("Find Key [%s] value [%s]\n", argv[2], buf);
    }
    else if (strcmp(argv[1], "del") == 0)
    {
        m_map.del(argv[2],strlen(argv[2]));
        printf("Delete Key [%s]\n", argv[2] );
    }
    else if (strcmp(argv[1], "size") == 0)
    {
        printf("Map Size [%d]\n", m_map.size());
    }
    else if (strcmp(argv[1], "show") == 0)
    {
        for (it = m_map.begin(); it != m_map.end(); it++)
        {
            printf("Find Key [%s] value [%s]\n", std::string((char *)it.key,it.keyLen).c_str() , std::string((char *)it.value, it.valueLen).c_str());
        }
    }
    return 0;
}
