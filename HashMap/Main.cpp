#include "FileStrHashMap_1.hpp"
#include <sys/time.h>
#include <string.h>


int total = 1000000;

int add( LYW_CODE::FileHashMap & m_map)
{
    char buf[1024];
    char key[64];

    struct timeval begin;
    struct timeval end;

    memset (buf, 0x31, sizeof(buf));
    gettimeofday(&begin, NULL);
    for (int iLoop = 0; iLoop < total; iLoop++)
    {
        memset(key, 0x00, sizeof(key));
        sprintf(key, "Test_data:%d", iLoop);
        m_map.add(key, strlen(key), buf, 1024);
    }
    gettimeofday(&end, NULL);

    return ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec)) / total;
}


int find( LYW_CODE::FileHashMap & m_map)
{
    char buf[1024];
    char key[64];

    struct timeval begin;
    struct timeval end;

    memset (buf, 0x31, sizeof(buf));
    gettimeofday(&begin, NULL);
    for (int iLoop = 0; iLoop < total; iLoop++)
    {
        memset(key, 0x00, sizeof(key));
        sprintf(key, "Test_data:%d", iLoop);
        m_map.find(key, strlen(key), buf,1024);
    }
    gettimeofday(&end, NULL);

    return ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec)) / total;

}

int del( LYW_CODE::FileHashMap & m_map)
{
    char buf[1024];
    char key[64];

    struct timeval begin;
    struct timeval end;

    memset (buf, 0x31, sizeof(buf));
    gettimeofday(&begin, NULL);
    for (int iLoop = 0; iLoop < total; iLoop++)
    {
        memset(key, 0x00, sizeof(key));
        sprintf(key, "Test_data:%d", iLoop);
        m_map.del(key, strlen(key));
    }
    gettimeofday(&end, NULL);

    return ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec)) / total;


}

int main(int argc, char ** argv)
{
    LYW_CODE::FileHashMap m_map("HashMapFile");
    int t = 0;
    t = add (m_map);
    printf("add TPS[%d]\n", t);
    t = find (m_map);
    printf("add TPS[%d]\n", t);
    t = del (m_map);
    printf("add TPS[%d]\n", t);

}
