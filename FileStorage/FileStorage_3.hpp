#include "FileIO.hpp"
#include <list>
#include <string.h>


#ifdef __linux__
#define FILE_STORAGE_LONG long long 
#define FILE_STORAGE_UNSIGNED_LONG unsigned long long 
#define FILE_STORAGE_INT int 
#define FILE_STORAGE_UNSIGNED_INT unsigned int
#define FILE_STORAGE_CHAR char 
#define FILE_STORAGE_UNSIGNED_CHAR unsigned char 

#define FILE_STORAGE_SHORT short
#define FILE_STORAGE_UNSIGNED_SHORT unsigned short
#endif

#define FILE_STORAGE_VERIFY_INFO "****LYW STORAGE****"
#define FILE_STORAGE_BLOCK_SIZE 4096


namespace LYW_CODE
{

    template <typename IOType = FileIO>
    class FileStorage
    {
    public:
        typedef FILE_STORAGE_UNSIGNED_LONG  FileStorageHandle;
    private:
        typedef struct _FixedHead
        {
            FILE_STORAGE_CHAR verifyInfo[32];

            FILE_STORAGE_UNSIGNED_INT blockSize;

            FILE_STORAGE_UNSIGNED_INT blockNumForEachPage;

            FILE_STORAGE_UNSIGNED_LONG pageNum;

            FILE_STORAGE_UNSIGNED_LONG pageSize;


        } FixedHead_t;

        typedef struct _IndexNode
        {
            FILE_STORAGE_UNSIGNED_SHORT referenceIndex;
            FILE_STORAGE_UNSIGNED_SHORT leftLen;
        } IndexNode_t;

        typedef struct _DataNode
        {
            //FILE_STORAGE_UNSIGNED_LONG beginIndex;
            FILE_STORAGE_UNSIGNED_LONG len;
            FILE_STORAGE_UNSIGNED_LONG next;
            FILE_STORAGE_UNSIGNED_CHAR data[0];
        } DataNode_t;

        typedef struct _FreeBlockInfoCache
        {
            FILE_STORAGE_UNSIGNED_SHORT referenceIndex;
            FILE_STORAGE_UNSIGNED_SHORT leftLen;
            FILE_STORAGE_UNSIGNED_LONG beginIndex;
        } FreeBlockInfoCache_t;


    public:
        FileStorage (const std::string & fileName)
        {
            m_FileName = fileName;
            freeBlockCacheClear();
            m_Storage = new IOType;
            memset(&m_FixedHead, 0x00, sizeof(FixedHead_t));
        }

        ~FileStorage()
        {
            if (m_Storage != NULL)
            {
                delete m_Storage;
                m_Storage = NULL;
            }

            if (m_Block != NULL) 
            {
                ::free(m_Block);
                m_Block = NULL;
            }
        }

        FileStorageHandle allocate(FILE_STORAGE_UNSIGNED_INT size)
        {
            DataNode_t dataNode;

            FreeBlockInfoCache_t freeInfo;

            FILE_STORAGE_UNSIGNED_INT leftLen = 0;

            FILE_STORAGE_UNSIGNED_LONG needBlockNum = 0;

            memset(&FreeInfo, 0x00, sizeof(FreeBlockInfoCache_t));

            FreeInfo.beginIndex = 0;

            if (!IsInit())
            {
                return 0;
            }
            
            leftLen = size % (m_FixedHead.blockSize - sizeof(DataNode_t));
            needBlockNum = (size - leftLen) / (m_FixedHead.blockSize - sizeof(DataNode_t));

            if (leftLen != 0) 
            {
                needBlockNum += 1;
            }

            
            for (FILE_STORAGE_UNSIGNED_INT iLoop = leftLen + sizeof(DataNode_t); iLoop <= m_FixedHead.blockSize; iLoop++)
            {
                std::list <FreeBlockInfoCache_t> m_freeCache[FILE_STORAGE_BLOCK_SIZE + 1];
                if (!m_freeCahce[iLoop].empty()) 
                {
                    freeInfo = m_freeCahce[iLoop].front();
                    m_freeCahce[iLoop].pop_front();
                    break;
                }
            }

            if (freeInfo.beginIndex == 0)  
            {
                /*no free */
                
            }
            else
            {

            }
        }


    private:
    public:
        
        bool IsInit()
        {
            if (m_Storage->IsOpen())
            {
                return true;
            }

            freeBlockCacheClear();

            if (!m_Storage->open(m_FileName, 0))
            {
                return false;
            }

            if (LoadStorageFile() < 0)
            {
                m_Storage->close();
                return false;
            }

            return true;
        }


        FILE_STORAGE_INT LoadStorageFile()
        {
            FILE_STORAGE_INT ret = 0;

            memset(&m_FixedHead, 0x00,sizeof(FixedHead_t));

            /*read head and check*/
            ret = readFromFile(0, &m_FixedHead, sizeof(FixedHead_t));
            if (ret > 0 && memcmp(m_FixedHead.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO)) == 0)
            {
                /*anlysis index block*/
                loadFreeBlockInfoCahce();
            }
            else
            {
                createNewStorageFile();
            }

            return 0;
        }



        void loadFreeBlockInfoCahce()
        {
            FILE_STORAGE_INT ret;

            FreeBlockInfoCache_t cacheInfo;

            IndexNode_t * pIndexField = NULL;

            FILE_STORAGE_UNSIGNED_LONG index = m_FixedHead.blockSize * 2;
            FILE_STORAGE_UNSIGNED_LONG endIndex = m_FixedHead.blockSize + m_FixedHead.pageNum * m_FixedHead.pageSize;

            if (m_Block != NULL)
            {
                ::free(m_Block);
            }

            m_Block = (FILE_STORAGE_UNSIGNED_CHAR *)::malloc(m_FixedHead.blockSize);

            while (index < endIndex)
            {
                ret = readFromFile(index, m_Block, FILE_STORAGE_BLOCK_SIZE);
                if (ret == FILE_STORAGE_BLOCK_SIZE)
                {
                    pIndexField = (IndexNode_t *)m_Block;

                    for (FILE_STORAGE_INT iLoop = 0; iLoop < m_FixedHead.blockNumForEachPage; iLoop++)
                    {
                        cacheInfo.beginIndex = index + m_FixedHead.blockSize * iLoop;
                        cacheInfo.referenceIndex = pIndexField[iLoop].referenceIndex;
                        cacheInfo.leftLen = pIndexField[iLoop].leftLen;
                        m_freeCache[cacheInfo.leftLen].push_back(cacheInfo);
                    }
                }

                index += m_FixedHead.blockNumForEachPage *  m_FixedHead.blockSize;
           }

        }

        void createNewStorageFile()
        {
            IndexNode_t * pIndexField = NULL;

            memcpy(m_FixedHead.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO));

            m_FixedHead.pageNum = 0;

            m_FixedHead.blockSize = FILE_STORAGE_BLOCK_SIZE;

            m_FixedHead.blockNumForEachPage = FILE_STORAGE_BLOCK_SIZE / sizeof(IndexNode_t);

            m_FixedHead.pageSize = m_FixedHead.blockSize * m_FixedHead.blockNumForEachPage;

            if (m_Block != NULL)
            {
                ::free(m_Block);
            }

            m_Block = (FILE_STORAGE_UNSIGNED_CHAR *)::malloc(m_FixedHead.blockSize);

            writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));

            pIndexField = (IndexNode_t *)m_Block;

            for (FILE_STORAGE_INT iLoop = 0; iLoop < m_FixedHead.blockNumForEachPage; iLoop++)
            {
                pIndexField[iLoop].referenceIndex = 0;

                pIndexField[iLoop].leftLen = m_FixedHead.blockSize;
            }

            writeToFile(m_FixedHead.blockSize, m_Block, m_FixedHead.blockSize);

        }


        void localNewPage()
        {
            m_FixedHead.
        }

        FILE_STORAGE_INT writeToFile(FILE_STORAGE_UNSIGNED_LONG beginIndex, void * data, size_t lenOfData) 
        {                                            
            if (m_Storage->lseek(beginIndex, SEEK_SET) < 0)
            {                                        
                m_Storage->close();                  
                return -2;                           
            }                                        
                                                     
            if(m_Storage->write(data, lenOfData) < 0)
            {                                        
                m_Storage->close();
                return -2;
            }

            return lenOfData;
        }


        FILE_STORAGE_INT readFromFile(FILE_STORAGE_UNSIGNED_LONG beginIndex, void * data, size_t lenOfData) 
        {
            FILE_STORAGE_INT ret = 0;

            size_t readLen = 0;
            
            /*judge data*/
            if (data == NULL || lenOfData == 0)
            {
                return -1;
            }

            /*seek to index*/
            if (m_Storage->lseek(beginIndex, SEEK_SET) < 0)
            {
                return -2;
            }

            /*read untill lenOfData*/
            while (readLen < lenOfData)
            {
                if((ret = m_Storage->read((char *)data + readLen, lenOfData, lenOfData - readLen)) < 0)
                {
                    return -2;
                }
                else if (ret == 0)
                {
                    return -2;
                }
                readLen += ret;
            }
            return readLen;
        }


        void freeBlockCacheClear()
        {
            for (int iLoop = 0; iLoop < FILE_STORAGE_BLOCK_SIZE + 1; iLoop++)
            {
                m_freeCache[iLoop].clear();
            }
            return;
        }



    private:
        FixedHead_t m_FixedHead;

        std::string  m_FileName;

        BaseFileIO * m_Storage;

        FILE_STORAGE_UNSIGNED_CHAR * m_Block;

        std::list <FreeBlockInfoCache_t> m_freeCache[FILE_STORAGE_BLOCK_SIZE + 1];

    };
}
