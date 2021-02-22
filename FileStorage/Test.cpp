#include "FileStorage.hpp"


int main(int argc, char ** argv)
{
    LYW_CODE::FileStorage<> storage("LYWFile");
    //printf("%d\n",sizeof(LYW_CODE::FileStorage<>::TIndexFileBlock));
    if (argc < 1)
    {
        return 0;
    }
    char buf2[8192] = {0};
    memset(buf2,0x32,8192);
    unsigned long long handle = 0;
    int tag = atoi(argv[1]);
    int size = 0; 
    switch(tag) 
    {
    case 0:
      size = atoi(argv[2]);
      handle = storage.allocate(size);
      //sprintf(buf2,"%lld", handle);
      storage.write(handle, buf2, size );
      printf("allocate %lld\n", handle);
      break;
    case 1:
      memset(buf2,0x00, sizeof(buf2));
      handle = atoi(argv[2]);
      storage.read(handle,buf2, 1024);
      printf("read %s\n", buf2);
      break;
    case 2:
      handle = atoi(argv[2]);
      storage.free(handle);
      printf("free %lld\n", handle) ;
      break;
    }

    //storage.showBlockInfo();

    return 0;
}
