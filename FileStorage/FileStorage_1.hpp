#include "FileIO.hpp"
#include <string.h>
#include <map>

#ifdef __linux__
#define FILE_STORAGE_LONG long long 
#define FILE_STORAGE_UNSIGNED_LONG unsigned long long 
#define FILE_STORAGE_INT int 
#define FILE_STORAGE_UNSIGNED_INT unsigned int
#define FILE_STORAGE_CHAR char 
#define FILE_STORAGE_UNSIGNED_CHAR unsigned char 
#endif

#define FILE_STORAGE_VERIFY_INFO "****LYW STORAGE****"

#define FILE_STORAEE_BLOCK_SIZE 4
#define FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE 4096

/* use cache will allocate quickly, but allocate info need cache, otherwise allocateinfo will be read from file when allocate */
#define FILE_STORAGE_IS_USE_CACHE 1


namespace LYW_CODE
{
    class FileStorage
    {
    public:
        typedef struct _FileStorageHandle
        {
            FILE_STORAGE_UNSIGNED_LONG beginIndex;
            FILE_STORAGE_UNSIGNED_INT size;
        } FileStorageHandle;

    private:
        typedef struct _VerifyInfo
        {
            FILE_STORAGE_CHAR verifyInfo[16];
        } VerifyInfo_t;

        typedef struct _FileInfo
        {
            FILE_STORAGE_UNSIGNED_INT pageNum;
            FILE_STORAGE_UNSIGNED_INT pageSize;
            FILE_STORAGE_UNSIGNED_INT blockIndexFieldSize;
            FILE_STORAGE_UNSIGNED_INT blockSize;
        } FileInfo_t;


        typedef struct _BlockIndexFieldHead
        {
            FILE_STORAGE_UNSIGNED_INT maxSerialBlockSize;
            FILE_STORAGE_UNSIGNED_INT maxSerialBlockIndex;
        } BlockIndexFieldHead_t;

        typedef struct _QuickAllocateCache
        {
            BlockIndexFieldHead_t allocateInfo;
        } QuickAllocateCache_t;


    public:
        FileStorage (const std::string & fileName)
        {
            m_FileName = fileName;

            m_quickAllocate.clear();

            m_Storage = new IOType;

            m_BlockIndex = NULL;
        }

        ~FileStorage ()
        {
            if (m_Storage != NULL)
            {
                delete m_Storage;
                m_Storage = NULL;
            }
        }

        FileStorageHandle allocate(FILE_STORAGE_UNSIGNED_INT size)
        {
            FileStorageHandle handle;

            memset(&handle, 0x00, sizeof(handle));

            if (!IsInit())
            {
                return handle;
            }

            /*quick allocate*/ 
            if (QuickAllocate(handle, size))
            {
                return handle;
            }

            /*normal allocate*/
            if (NormalAllocate(handle, size))
            {
                return handle;
            }
            return handle;
        }



        void Free(const FileStorageHandle & handle)
        {
        }

        int write(const FileStorageHandle & handle, void * buf, unsigned long lenOfBuf)
        {
            return -1;
        }

        int write(const FileStorageHandle & handle, unsigned int pos, void * buf, unsigned long lenOfBuf)
        {
            return -1;
        }

        int read (const FileStorageHandle & handle, void * buf, unsigned long len)
        {
            return -1;
        }

        int read (const FileStorageHandle & handle, unsigned int pos, void * buf, unsigned long len)
        {
            return -1;
        }

    private:

        /**
        * @brief        search from m_quickAllocate
        *
        */
        bool QuickAllocate(FileStorageHandle & handle, FILE_STORAGE_UNSIGNED_INT size)
        {
            FILE_STORAGE_UNSIGNED_INT allocateSize = 0;

            std::map <FILE_STORAGE_UNSIGNED_INT, std::map <FILE_STORAGE_UNSIGNED_INT, QuickAllocateCache_t> > :: iterator it;

            std::map <FILE_STORAGE_UNSIGNED_INT, QuickAllocateCache_t> :: iterator allocateInfo;

            int needPageNum = 0;

            FILE_STORAGE_UNSIGNED_INT allocatePageIndex = 0;

            QuickAllocateCache_t quickAllocateCache;

            FILE_STORAGE_UNSIGNED_LONG index;
            FILE_STORAGE_UNSIGNED_INT leftPageNum = 0;

            BlockIndexFieldHead_t * blockIndexFieldHead = (BlockIndexFieldHead_t *)m_BlockIndex;

            memset(&quickAllocateCache, 0x00, sizeof(QuickAllocateCache_t));

            /*need allocate size*/
            handle.size = calculateAllocateSizeBySize(size);

            /*need page num*/
            needPageNum = handle.size / (m_FileInfo.pageSize -  m_FileInfo.blockIndexFieldSize);
            if (handle.size % (m_FileInfo.pageSize -  m_FileInfo.blockIndexFieldSize) != 0) 
            {
                needPageNum++;
            }

            for (it = m_quickAllocate.begin(); it != m_quickAllocate.end(); it++)
            {
                if (it->first >= handle.size && it->second.size() > 0)
                {
                    allocateInfo = it->second.begin();
                    allocatePageIndex = allocateInfo->first;
                    memcpy(&quickAllocateCache, &allocateInfo->second, sizeof(QuickAllocateCache_t));
                    it->second.erase(allocatePageIndex);
                    break;

                    /*load index and recalculate page minSize*/
                    //handle.beginIndex = calculateFileIndexByBlockIndex (allocateInfo->first, allocateInfo->second.maxSuccessiveBlockIndex);

                    /*moro than one page*/
                    //ReadFromFile(sizeof(VerifyInfo_t) + sizeof(FileInfo_t) + allocateInfo->first * m_FileInfo.pageSiz, m_BlockIndex, m_FileInfo.blockIndexFieldSize);

                }

            }

            
            if (quickAllocateCache.allocateInfo.maxSerialBlockSize < handle.size)
            {
                /*not found*/
                allocatePageIndex = allocatePages(needPageNum);
                quickAllocateCache.allocateInfo.maxSerialBlockSize = (m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize) * needPageNum;
                quickAllocateCache.allocateInfo.maxSerialBlockIndex = m_FileInfo.blockIndexFieldSize;
            }

            blockIndexFieldHead->maxSerialBlockSize = 0;
            blockIndexFieldHead->maxSerialBlockIndex = m_FileInfo.blockIndexFieldSize;

            index = sizeof(VerifyInfo_t) + sizeof(FileInfo_t) + allocatePageIndex * m_FileInfo.pageSize;

            memset(m_BlockIndex + sizeof(BlockIndexFieldHead_t), 0xFF, m_FileInfo.blockIndexFieldSize - sizeof(BlockIndexFieldHead_t));
            for(int iLoop = 0; iLoop < needPageNum - 1; iLoop++)
            {

                WriteToFile(index, &m_BlockIndex, m_FileInfo.blockIndexFieldSize);
                index += m_FileInfo.pageSize;
            }

            /*Last Block*/
            blockIndexFieldHead->maxSerialBlockSize = handle.size - (needPageNum - 1) * (m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize);

            blockIndexFieldHead->maxSerialBlockIndex = m_FileInfo.pageSize - blockIndexFieldHead->maxSerialBlockSize;


            if (blockIndexFieldHead->maxSerialBlockSize != 0)
            {
                QuickAllocateCache_t tmp;
                memcpy(&tmp.allocateInfo, blockIndexFieldHead, sizeof(QuickAllocateCache_t));
                FILE_STORAGE_UNSIGNED_INT minSize = CalculateMinAllocateSize(blockIndexFieldHead->maxSerialBlockSize); 
                m_quickAllocate[minSize][ allocatePageIndex + needPageNum - 1] = tmp;


            }

            WriteToFile(index, &m_BlockIndex, m_FileInfo.blockIndexFieldSize);
            index += m_FileInfo.pageSize;



            /*Left Page*/
            leftPageNum = quickAllocateCache.allocateInfo.maxSerialBlockSize / (m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize) - needPageNum;

            if (leftPageNum > 0)
            {
                QuickAllocateCache_t tmp;
                tmp.allocateInfo.maxSerialBlockSize = leftPageNum * (m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize);
                tmp.allocateInfo.maxSerialBlockIndex = m_FileInfo.blockIndexFieldSize;
                
                FILE_STORAGE_UNSIGNED_INT minSize = CalculateMinAllocateSize(blockIndexFieldHead->maxSerialBlockSize); 
                m_quickAllocate[minSize][ allocatePageIndex + needPageNum] = tmp;
            }

            return false;
        }
        
        
        /**
        * @brief        search from file 
        *
        */
        bool NormalAllocate(FileStorageHandle & handle, FILE_STORAGE_UNSIGNED_INT size)
        {
            return false;
        }

        bool IsInit()
        {
            if (m_Storage->IsOpen())
            {
                return true;
            }

            m_quickAllocate.clear();

            if (!m_Storage->open(m_FileName, 0))
            {
                return false;
            }

            if (LoadStorageFile() < 0)
            {
                /*load file failed*/
                return false;
            }

            return true;
        }

        int LoadStorageFile()
        {
            int ret = 0;

            FILE_STORAGE_UNSIGNED_LONG index = 0;

            FILE_STORAGE_UNSIGNED_INT serialEmptyPageNum = 0;

            memset(m_VerifyInfo, 0x00, sizeof(VerifyInfo_t));

            BlockIndexFieldHead_t * blockIndexFieldHead;

            QuickAllocateCache_t quickAllocateCache;

            FILE_STORAGE_UNSIGNED_INT minSize;


            if (m_Storage->lseek(0,SEEK_SET)) 
            {
                m_Storage->close();
                return -2;
            }

            if((ret = m_Storage->read((void *)&m_VerifyInfo,sizeof(VerifyInfo_t), sizeof(VerifyInfo_t))) < 0)
            {
                m_Storage->close();
                return -2;
            }

            index += sizeof(VerifyInfo_t);

            if (memcmp(m_VerifyInfo.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO)) == 0)
            {
                if(m_Storage->read((void *)&m_FileInfo,sizeof(FileInfo_t), sizeof(FileInfo_t)) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                index += sizeof(FileInfo_t);

                if (m_BlockIndex != NULL)
                {
                    ::free(m_BlockIndex);
                }
                m_BlockIndex = ::malloc(m_FileInfo.blockIndexFieldSize);
                
                for (FILE_STORAGE_UNSIGNED_INT iLoop = 0; iLoop < m_FileInfo.pageNum; iLoop++)
                {
                    if (ReadFromFile(index, m_BlockIndex, m_FileInfo.blockIndexFieldSize) > 0)
                    {
                        AnalysisIndexField(iLoop);
                        blockIndexFieldHead = (BlockIndexFieldHead_t *) m_BlockIndex;
                        if (blockIndexFieldHead->maxSerialBlockSize == (m_FileInfo.pageNum - m_FileInfo.pageNum)) 
                        {
                            /*serial empty page*/
                            serialEmptyPageNum++;
                        }
                        else
                        {
                            if (serialEmptyPageNum != 0) 
                            {
                                quickAllocateCache.allocateInfo.maxSerialBlockIndex = 0;
                                quickAllocateCache.allocateInfo.maxSerialBlockSize = serialEmptyPageNum * (m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize);
                                if ((minSize = CalculateMinAllocateSize(quickAllocateCache.allocateInfo.maxSerialBlockSize)) > 0)
                                {
                                    m_quickAllocate[minSize][iLoop - serialEmptyPageNum] = quickAllocateCache;
                                }
                            }
                            serialEmptyPageNum = 0;
                            memcpy(&quickAllocateCache.allocateInfo, blockIndexFieldHead, sizeof(BlockIndexFieldHead_t)):
                            if ((minSize = CalculateMinAllocateSize(blockIndexFieldHead->maxSerialBlockSize)) > 0)
                            {
                                m_quickAllocate[minSize][iLoop] = quickAllocateCache;
                            }

                        }
                    }
                    else
                    {
                        serialEmptyPageNum++;
                    }

                    index += m_FileInfo.pageSize;
                }


                //end check
                if (serialEmptyPageNum != 0) 
                {
                    quickAllocateCache.allocateInfo.maxSerialBlockIndex = 0;
                    quickAllocateCache.allocateInfo.maxSerialBlockSize = serialEmptyPageNum * (m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize);
                    if ((minSize = CalculateMinAllocateSize(quickAllocateCache.allocateInfo.maxSerialBlockSize)) > 0)
                    {
                        m_quickAllocate[minSize][m_FileInfo.pageNum - serialEmptyPageNum] = quickAllocateCache;
                    }
                }
            }
            else
            {
                /*create new file*/
                memcpy(m_VerifyInfo.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO));

                m_FileInfo.pageNum = 0;

                m_FileInfo.pageSize = FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE + (FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE - sizeof(BlockIndexFieldHead_t)) * FILE_STORAEE_BLOCK_SIZE * 8;

                m_FileInfo.blockIndexFieldSize = FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE;

                m_FileInfo.pageDataSize = FILE_STORAEE_BLOCK_SIZE;

                WriteToFile(0, &m_VerifyInfo, sizeof(VerifyInfo_t));

                WriteToFile(sizeof(VerifyInfo_t), &m_FileInfo, sizeof(FileInfo_t));

                if (m_BlockIndex != NULL)
                {
                    ::free(m_BlockIndex);
                }
                m_BlockIndex = ::malloc(m_FileInfo.blockIndexFieldSize);
 

            }

            return false;
        }

        int WriteToFile(FILE_STORAGE_UNSIGNED_LONG beginIndex, void * data, size_t lenOfData) 
        {
            if (m_Storage->lseek(beginIndex, SEEK_SET) < 0)
            {
                m_Storage->close();
                return -2;
            }

            if(m_Storage->write((const void *)data, lenOfData) < 0)
            {
               m_Storage->close();
                return -2;
            }

            return lenOfData;
        }


        int ReadFromFile(FILE_STORAGE_UNSIGNED_LONG beginIndex, void * data, size_t lenOfData) 
        {
            int ret = 0;
            size_t leftLen = 0;
            if (m_Storage->lseek(beginIndex, SEEK_SET) < 0)
            {
                m_Storage->close();
                return -2;
            }
            
            while (readLen > 0)
            {
                if((ret = m_Storage->read((const void *)data + readLen, lenOfData , lenOfData)) < 0)
                {
                   m_Storage->close();
                    return -2;
                }
                else if (ret = 0)
                {
                    return -2;
                }

                readLen += ret;
            }

            return ret;
        }

        void AnalysisIndexField(FILE_STORAGE_UNSIGNED_INT blockIndex)
        {
            FILE_STORAGE_UNSIGNED_INT index = 0;
            QuickAllocateCache_t * quickAllocateCache;
            FILE_STORAGE_UNSIGNED_CHAR * blockIndex =  m_BlockIndex;
            FILE_STORAGE_UNSIGNED_INT minSize;

            quickAllocateCache = (QuickAllocateCache_t *)blockIndex;
            index += sizeof(QuickAllocateCache_t);

            if ((minSize = CalculateMinAllocateSize(quickAllocateCache)) > 0)
            {
                m_quickAllocate[minSize][blockIndex] = *quickAllocateCache;
            }
        }

        FILE_STORAGE_UNSIGNED_INT CalculateMinAllocateSize(FILE_STORAGE_UNSIGNED_INT leftAllocateSize)
        {
            FILE_STORAGE_UNSIGNED_INT minSize = 1;
            minSize << (sizeof(FILE_STORAGE_UNSIGNED_INT) * 8 - 1);
            for (int iLoop = 0; iLoop < sizeof(FILE_STORAGE_UNSIGNED_INT) * 8; iLoop++)
            {
                 if (((minSize >> 1) & leftAllocateSize) != 0)
                 {
                     return minSize;
                 }
            }

            return 0;
        }
 
        FILE_STORAGE_UNSIGNED_LONG calculateFileIndexByBlockIndex(FILE_STORAGE_UNSIGNED_INT pageIndex, FILE_STORAGE_UNSIGNED_INT blockIndex)
        {
            return (sizeof(VerifyInfo_t) + sizeof(FileInfo_t) + pageIndex * m_FileInfo. pageSize + m_FileInfo.blockIndexFieldSize + blockIndex);
        }

        FILE_STORAGE_UNSIGNED_INT calculateAllocateSizeBySize(FILE_STORAGE_UNSIGNED_INT size)
        {
            FILE_STORAGE_UNSIGNED_INT allocateSize = (size / m_FileInfo.blockSize) * m_FileInfo.blockSize;
            if (size % m_FileInfo.blockSize != 0)
            {
                allocateSize += m_FileInfo.blockSize;
            }
            return allocateSize;
        }

        FILE_STORAGE_INT allocatePages(FILE_STORAGE_UNSIGNED_INT Num)
        {
            //m_FileInfo.pageNum += Num;
            BlockIndexFieldHead_t blockIndexFieldHead = m_BlockIndex;
            FILE_STORAGE_UNSIGNED_LONG index = 0;

            FILE_STORAGE_UNSIGNED_INT minSize = 0;
            

            QuickAllocateCache_t quickAllocateCache;

            if (Num == 0)
            {
                return -1;
            }

            index = sizeof(VerifyInfo_t) + sizeof(FileInfo_t) + m_FileInfo.pageNum * m_FileInfo.pageSize;

            for (int iLoop = 0; iLoop < Num; iLoop++)
            {
                memset(m_BlockIndex, 0x00, m_FileInfo.blockIndexFieldSize);
                blockIndexFieldHead.maxSerialBlockSize = m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize;
                blockIndexFieldHead.maxSerialBlockIndex = m_FileInfo.blockIndexFieldSize;
                memset(m_BlockIndex + sizeof(BlockIndexFieldHead_t), 0x00, m_FileInfo.blockIndexFieldSize - sizeof(BlockIndexFieldHead_t));
                WriteToFile(index, &m_BlockIndex, m_FileInfo.blockIndexFieldSize);
                index += m_FileInfo.pageSize;
            }

            quickAllocateCache.allocateInfo.maxSerialBlockSize = (m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize) * Num;

            quickAllocateCache.allocateInfo.maxSerialBlockIndex = m_FileInfo.blockIndexFieldSize;

            minSize = CalculateMinAllocateSize(quickAllocateCache.allocateInfo.maxSerialBlockSize);

            m_quickAllocate[minSize][m_FileInfo.pageNum] = quickAllocateCache;

            m_FileInfo.pageNum += Num;

            WriteToFile(sizeof(VerifyInfo_t), &m_FileInfo, sizeof(FileInfo_t));

            return (m_FileInfo.pageNum - Num);
        }


    private:
        VerifyInfo_t m_VerifyInfo;

        FileInfo_t m_FileInfo;

        BlockIndexFieldHead_t m_BlockIndexFieldHead;

        std::string m_FileName;

        std::map <FILE_STORAGE_UNSIGNED_INT, std::map <FILE_STORAGE_UNSIGNED_INT, QuickAllocateCache_t> > m_quickAllocate;

        BaseFileIO * m_Storage;

        FILE_STORAGE_UNSIGNED_CHAR * m_BlockIndex;

    private:

    };
}
