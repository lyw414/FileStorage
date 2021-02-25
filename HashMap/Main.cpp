#include "FileStrHashMap.hpp"
#include <sys/time.h>
#include <string.h>


int total = 10000;

int add( )
{
    char buf[1024];
    char key[64];

    struct timeval begin;
    struct timeval end;

    memset (buf, 0x31, sizeof(buf));
    buf[1023] = '\0';
    gettimeofday(&begin, NULL);
    LYW_CODE::FileHashMap m_map("HashMapFile");
    for (int iLoop = 0; iLoop < total; iLoop++)
    {
        memset(key, 0x00, sizeof(key));
        sprintf(key, "Test_data:%d", iLoop);
        m_map.add(key, strlen(key), buf, 1024);
    }
    gettimeofday(&end, NULL);

    return ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec)) / total;
}


int find( )
{
    char buf[1024];
    char key[64];

    struct timeval begin;
    struct timeval end;

    memset (buf, 0x31, sizeof(buf));
    gettimeofday(&begin, NULL);

    LYW_CODE::FileHashMap m_map("HashMapFile");
    for (int iLoop = 0; iLoop < total; iLoop++)
    {
        memset(key, 0x00, sizeof(key));
        sprintf(key, "Test_data:%d", iLoop);
        m_map.find(key, strlen(key), buf,1024);
    }
    gettimeofday(&end, NULL);

    return ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec)) / total;

}

int del( )
{
    char buf[1024];
    char key[64];

    struct timeval begin;
    struct timeval end;

    memset (buf, 0x31, sizeof(buf));
    gettimeofday(&begin, NULL);

    LYW_CODE::FileHashMap m_map("HashMapFile");
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


    int t = 0;
    int tag = atoi(argv[1]);
    total = atoi(argv[2]);
    
    switch(tag)
    {
    case 0:
        t = add ();
        printf("add TPS[%d]\n", t);
        break;

    case 1:
        t = find ();
        printf("add TPS[%d]\n", t);
        break;

    case 2:
        t = del ();
        printf("add TPS[%d]\n", t);
        break;
    }
}
