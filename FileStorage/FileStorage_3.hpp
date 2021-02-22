#include "FileIO.hpp"
#include <list>
#include <string.h>
#include <map>


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
#define FILE_STORAGE_BLOCK_SIZE 1024

typedef FILE_STORAGE_UNSIGNED_LONG  FileStorageHandle;
namespace LYW_CODE
{
    template <typename IOType = FileIO>
    class FileStorage
    {
    public:
    private:
        typedef struct _FixedHead
        {
            FILE_STORAGE_CHAR verifyInfo[32];

            FILE_STORAGE_UNSIGNED_INT blockSize;

            FILE_STORAGE_UNSIGNED_INT blockNumForEachPage;

            FILE_STORAGE_UNSIGNED_LONG pageNum;

            FILE_STORAGE_UNSIGNED_LONG pageSize;

            FILE_STORAGE_UNSIGNED_LONG userHandle[16];


        } FixedHead_t;

        typedef struct _IndexNode
        {
            FILE_STORAGE_UNSIGNED_SHORT referenceIndex;
            FILE_STORAGE_UNSIGNED_SHORT leftLen;
        } IndexNode_t;

        typedef struct _DataNode
        {
            FILE_STORAGE_UNSIGNED_LONG len;
            FILE_STORAGE_UNSIGNED_LONG next;
        } DataNode_t;

        typedef struct _FreeBlockInfoCache
        {
            FILE_STORAGE_UNSIGNED_SHORT referenceIndex;
            FILE_STORAGE_UNSIGNED_SHORT leftLen;
        } FreeBlockInfoCache_t;

    public:
        FileStorage (const std::string & fileName)
        {
            m_FileName = fileName;
            freeBlockCacheClear();
            m_Storage = new IOType;
            memset(&m_FixedHead, 0x00, sizeof(FixedHead_t));
            m_MaxUsedBlock = 0;
        }

        ~FileStorage()
        {
            if (m_Storage != NULL)
            {
                delete m_Storage;
                m_Storage = NULL;
            }

            if ( m_Block != NULL)
            {
                ::free(m_Block);
                m_Block = NULL;
            }
        }


        FileStorageHandle getUserHandle(FILE_STORAGE_UNSIGNED_INT index)
        {
            if (!IsInit())
            {
                return 0;
            }

            if (index >= sizeof(m_FixedHead.userHandle))
            { 
                return 0;
            }

            return m_FixedHead.userHandle[index];
        }

        bool setUserHandle(FILE_STORAGE_UNSIGNED_INT index, FileStorageHandle handle)
        {
            if (!IsInit())
            {
                return 0;
            }

            if (index >= sizeof(m_FixedHead.userHandle))
            { 
                return 0;
            }

            m_FixedHead.userHandle[index] = handle;

            writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));
        }

        FileStorageHandle allocate(FILE_STORAGE_UNSIGNED_INT size)
        {

            DataNode_t dataNode;

            FILE_STORAGE_UNSIGNED_LONG preBeginIndex = 0;

            FileStorageHandle handle = 0;

            FreeBlockInfoCache_t freeInfo;

            FreeBlockInfoCache_t preFreeInfo;

            IndexNode_t indexNode;

            FILE_STORAGE_UNSIGNED_INT leftLen = 0;

            FILE_STORAGE_UNSIGNED_LONG needBlockNum = 0;

            FILE_STORAGE_UNSIGNED_LONG beginIndex = 0;

            bool isNewPage = false;

            typename std::map <FILE_STORAGE_UNSIGNED_LONG, FreeBlockInfoCache_t> :: iterator it;

            if (!IsInit())
            {
                return 0;
            }

            
            leftLen = size % (m_FixedHead.blockSize - sizeof(DataNode_t));

            needBlockNum = (size - leftLen) / (m_FixedHead.blockSize - sizeof(DataNode_t));


            if (leftLen == 0) 
            {
                leftLen = m_FixedHead.blockSize - sizeof(DataNode_t);
            }
            else
            {
                needBlockNum += 1;
            }

            
            while (needBlockNum > 0) 
            {
                for (FILE_STORAGE_UNSIGNED_INT iLoop = leftLen + sizeof(DataNode_t); iLoop <= m_FixedHead.blockSize; iLoop++)
                {
                    if (!m_freeCache[iLoop].empty()) 
                    {

                        it = m_freeCache[iLoop].begin();

                        beginIndex = it->first;

                        if (m_MaxUsedBlock < beginIndex / m_FixedHead.blockSize)
                        {
                            m_MaxUsedBlock = beginIndex / m_FixedHead.blockSize;
                        }

                        freeInfo = it->second;

                        if (handle == 0) 
                        {
                            preBeginIndex = beginIndex;

                            handle = beginIndex + m_FixedHead.blockSize - freeInfo.leftLen;

                            dataNode.len = leftLen;

                            dataNode.next = 0;

                            preFreeInfo.referenceIndex = freeInfo.referenceIndex;

                            preFreeInfo.leftLen = freeInfo.leftLen;
                        }

                        if (preBeginIndex != beginIndex) 
                        {
                            /*write Data Head*/
                            dataNode.next = beginIndex + m_FixedHead.blockSize - freeInfo.leftLen;

                            writeToFile(preBeginIndex + m_FixedHead.blockSize - preFreeInfo.leftLen, &dataNode, sizeof(DataNode_t));
                            preBeginIndex = beginIndex;

                            dataNode.len = leftLen;

                            dataNode.next = 0;

                            preFreeInfo.referenceIndex = freeInfo.referenceIndex;

                            preFreeInfo.leftLen = freeInfo.leftLen;
 
                        }

                        freeInfo.leftLen -= (leftLen + sizeof(DataNode_t));

                        freeInfo.referenceIndex++;

                        m_freeCache[iLoop].erase(beginIndex);

                        m_freeCache[freeInfo.leftLen][beginIndex] = freeInfo;

                        indexNode.leftLen = freeInfo.leftLen;

                        indexNode.referenceIndex = freeInfo.referenceIndex;

                        WritePageIndex(beginIndex,indexNode);

                        break;
                    }
                }

                if (beginIndex == 0) 
                {
                    if (isNewPage)
                    {
                        return 0;
                    }
                    allocatePage();
                    isNewPage = true;
                    continue;
                }


                leftLen = m_FixedHead.blockSize - sizeof(DataNode_t);
                needBlockNum--;
            }

            
            writeToFile(preBeginIndex + m_FixedHead.blockSize - preFreeInfo.leftLen, &dataNode, sizeof(DataNode_t));

            return handle;
        }



        //int write(FileStorageHandle handle, unsigned int pos, void * buf, unsigned long lenOfBuf)


        int write(FileStorageHandle handle, FILE_STORAGE_UNSIGNED_LONG pos, void * buf, FILE_STORAGE_UNSIGNED_LONG lenOfBuf)
        {
            FILE_STORAGE_UNSIGNED_LONG leftLen = lenOfBuf;

            FILE_STORAGE_UNSIGNED_LONG posLeftLen = pos;
            FILE_STORAGE_UNSIGNED_LONG writeLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            DataNode_t dataNode;
            if (!IsInit())
            {
                return 0;
            }

            do
            {
                readFromFile(beginIndex , &dataNode, sizeof(DataNode_t));
                if (posLeftLen >= dataNode.len) 
                {
                    posLeftLen -= dataNode.len;

                    if (dataNode.next == 0)
                    {
                        return 0;
                    }
                }
                else
                {
                    if (leftLen <= dataNode.len - posLeftLen)
                    {
                        writeToFile(beginIndex + sizeof(DataNode_t) + posLeftLen, (unsigned char *)buf + writeLen, leftLen);
                        writeLen += leftLen;
                        return writeLen;
                    } 
                    else
                    {
                        writeToFile(beginIndex+ sizeof(DataNode_t) + posLeftLen, (unsigned char *)buf + writeLen, dataNode.len - posLeftLen);

                        writeLen += (dataNode.len - posLeftLen);

                        posLeftLen = 0;

                        if (dataNode.next == 0)
                        {
                            return 0;
                        }

                        beginIndex = dataNode.next;
                    }
                }
            } while(true);
        }

        int write(FileStorageHandle handle, void * buf, FILE_STORAGE_UNSIGNED_LONG lenOfBuf)
        {
            FILE_STORAGE_UNSIGNED_LONG leftLen = lenOfBuf;
            FILE_STORAGE_UNSIGNED_LONG writeLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            DataNode_t dataNode;
            if (!IsInit())
            {
                return 0;
            }

            do
            {
                readFromFile(beginIndex , &dataNode, sizeof(DataNode_t));

                if (leftLen <= dataNode.len)
                {
                    writeToFile(beginIndex + sizeof(DataNode_t), (unsigned char *)buf + writeLen, leftLen);
                    writeLen += leftLen;
                    return writeLen;
                } 
                else
                {
                    writeToFile(beginIndex+ sizeof(DataNode_t), (unsigned char *)buf + writeLen, dataNode.len);

                    writeLen += dataNode.len;

                    if (dataNode.next == 0)
                    {
                        return 0;
                    }

                    beginIndex = dataNode.next;
                }
            } while(true);
        }

        int read (FileStorageHandle handle, FILE_STORAGE_UNSIGNED_LONG pos, void * buf,FILE_STORAGE_UNSIGNED_LONG len)
        {
            FILE_STORAGE_UNSIGNED_LONG leftLen = len;
            FILE_STORAGE_UNSIGNED_LONG readLen = 0;
            FILE_STORAGE_UNSIGNED_LONG posLeftLen = pos;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            DataNode_t dataNode;

            if (!IsInit())
            {
                return 0;
            }

            do
            {
                readFromFile(beginIndex, &dataNode, sizeof(DataNode_t));

                if (posLeftLen >= dataNode.len) 
                {
                    posLeftLen -= dataNode.len;
                    if ( dataNode.next == 0) 
                    {
                        return 0;
                    }
                }
                else
                {
                    if (leftLen <= dataNode.len - posLeftLen)
                    {
                        readFromFile(beginIndex + sizeof(DataNode_t) + posLeftLen, (unsigned char *)buf + readLen, leftLen);
                        readLen += leftLen;
                        return readLen;
                    } 
                    else
                    {
                        readFromFile(beginIndex+ sizeof(DataNode_t) + posLeftLen, (unsigned char *)buf + readLen, dataNode.len - posLeftLen);

                        readLen += (dataNode.len - posLeftLen);

                        posLeftLen = 0;

                        if (dataNode.next == 0)
                        {
                            return readLen;
                        }

                        beginIndex = dataNode.next;
                    }
                }
            } while(true);


        }
        int read (FileStorageHandle handle, void * buf,FILE_STORAGE_UNSIGNED_LONG len)
        {
            FILE_STORAGE_UNSIGNED_LONG leftLen = len;
            FILE_STORAGE_UNSIGNED_LONG readLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            DataNode_t dataNode;
            if (!IsInit())
            {
                return 0;
            }

            do
            {
                readFromFile(beginIndex, &dataNode, sizeof(DataNode_t));

                if (leftLen <= dataNode.len)
                {
                    readFromFile(beginIndex + sizeof(DataNode_t), (unsigned char *)buf + readLen, leftLen);
                    readLen += leftLen;
                    return readLen;
                } 
                else
                {
                    readFromFile(beginIndex+ sizeof(DataNode_t), (unsigned char *)buf + readLen, dataNode.len);

                    readLen += dataNode.len;

                    if (dataNode.next == 0)
                    {
                        return readLen;
                    }

                    beginIndex = dataNode.next;
                }
            } while(true);
        }


        bool free(FileStorageHandle handle)
        {
            DataNode_t dataNode;
            FileStorageHandle beginIndex = handle;
            if (!IsInit())
            {
                return false;
            }

            if (handle < m_FixedHead.blockSize)
            {
                return false;
            }

            do
            {
                readFromFile(beginIndex, &dataNode, sizeof(DataNode_t));
                freeBlock(beginIndex);
                beginIndex = dataNode.next;

            } while(beginIndex != 0);

            return true;
        }

        void showBlockInfo()
        {
            if (!IsInit())
            {
                return ;
            }

            //typename std::map <FILE_STORAGE_UNSIGNED_LONG, FreeBlockInfoCache_t> :: iterator it;
            printf("total pageNum [%lld] pageBlcokNum [%d] MaxBlock [%lld]\n", m_FixedHead.pageNum, m_FixedHead.blockNumForEachPage, m_MaxUsedBlock);
            for (FILE_STORAGE_UNSIGNED_INT iLoop = 0; iLoop <= m_FixedHead.blockSize; iLoop++)
            {
                if (!m_freeCache[iLoop].empty())
                {
                    printf("LeftLen [%d] [%ld]\n", iLoop, m_freeCache[iLoop].size());
                }
            }
            
        }


        FILE_STORAGE_UNSIGNED_LONG size(FileStorageHandle handle)
        {
            FILE_STORAGE_UNSIGNED_LONG len = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;
            DataNode_t dataNode;
            if (!IsInit())
            {
                return 0;
            }

            do
            {
                readFromFile(beginIndex, &dataNode, sizeof(DataNode_t));
                len += dataNode.len;

                beginIndex = dataNode.next;

            } while(beginIndex != 0);

            return len;
        }

        void fset(FileStorageHandle handle, unsigned char ch)
        {

            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;
            DataNode_t dataNode;
            if (!IsInit())
            {
                return;
            }

            memset(m_Block, ch, m_FixedHead.blockSize);

            do
            {
                readFromFile(beginIndex, &dataNode, sizeof(DataNode_t));
                writeToFile(beginIndex + sizeof(DataNode_t), m_Block, dataNode.len);
                beginIndex = dataNode.next;
            } while(beginIndex != 0);
        }


        void clearFile()
        {
            m_Storage->close();
            m_Storage->open(m_FileName, 1);
            m_Storage->close();
        }

    private:

        void clearFileEnd()
        {
            typename std::map <FILE_STORAGE_UNSIGNED_LONG, FreeBlockInfoCache_t> :: iterator it;

            bool tag = false;

            while(m_MaxUsedBlock > 0)
            {
                it = m_freeCache[m_FixedHead.blockSize].find(m_MaxUsedBlock * m_FixedHead.blockSize);
                if (it != m_freeCache[m_FixedHead.blockSize].end()) 
                {
                    m_MaxUsedBlock -= 1;
                    tag = true;
                }
                else
                {
                    if ((m_MaxUsedBlock - 1) % m_FixedHead.blockNumForEachPage == 0) 
                    {
                        /*clear page*/
                        tag = true;
                        m_FixedHead.pageNum = (m_MaxUsedBlock - 1) / m_FixedHead.blockNumForEachPage;

                        writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));

                        for (FILE_STORAGE_UNSIGNED_INT iLoop = 1; iLoop <  m_FixedHead.blockNumForEachPage; iLoop++)
                        {
                            m_freeCache[m_FixedHead.blockSize].erase((m_MaxUsedBlock + iLoop) * m_FixedHead.blockSize);
                        }
                        m_MaxUsedBlock -= 1;
                    }
                    else
                    {
                        break;
                    }
                }
            } 

            if (tag) 
            {
                m_Storage->ftruncate((m_MaxUsedBlock + 1) * m_FixedHead.blockSize);
            }

        }

        void freeBlock(FILE_STORAGE_UNSIGNED_LONG index)
        {
            IndexNode_t indexNode;
            FreeBlockInfoCache_t freeInfo;

            typename std::map <FILE_STORAGE_UNSIGNED_LONG, FreeBlockInfoCache_t> :: iterator it;

            FILE_STORAGE_UNSIGNED_LONG writeIndex;

            FILE_STORAGE_UNSIGNED_LONG blockIndex;

            FILE_STORAGE_UNSIGNED_LONG tmp;

            tmp = index / m_FixedHead.blockSize;

            blockIndex = tmp * m_FixedHead.blockSize;

            writeIndex = m_FixedHead.blockSize  + ((tmp - 1) / m_FixedHead.blockNumForEachPage) * m_FixedHead.pageSize + ((tmp - 1 - 1) % m_FixedHead.blockNumForEachPage) * sizeof(IndexNode_t);

            for (FILE_STORAGE_UNSIGNED_INT iLoop = 0; iLoop <= m_FixedHead.blockSize; iLoop++)
            {
                if (!m_freeCache[iLoop].empty())
                {
                    it = m_freeCache[iLoop].find(blockIndex);
                    if (it != m_freeCache[iLoop].end()) 
                    {
                        it->second.referenceIndex -= 1;
                        freeInfo = it->second;
                        if (it->second.referenceIndex == 0) 
                        {
                            freeInfo.referenceIndex = 0;
                            freeInfo.leftLen = m_FixedHead.blockSize;
                            m_freeCache[iLoop].erase(blockIndex);
                            m_freeCache[freeInfo.leftLen][blockIndex] = freeInfo;
                        }

                        indexNode.referenceIndex = freeInfo.referenceIndex;
                        indexNode.leftLen = freeInfo.leftLen;
                        writeToFile(writeIndex, &indexNode, sizeof(IndexNode_t));
                        break;
                    }
                }
            }

           clearFileEnd();

        }


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

            FILE_STORAGE_UNSIGNED_LONG index = m_FixedHead.blockSize;

            FILE_STORAGE_UNSIGNED_LONG endIndex = m_FixedHead.blockSize + m_FixedHead.pageNum * m_FixedHead.pageSize;

            if (m_Block != NULL)
            {
                ::free(m_Block);
            }

            m_Block = (FILE_STORAGE_UNSIGNED_CHAR *)::malloc(m_FixedHead.blockSize);

            while (index < endIndex)
            {
                ret = readFromFile(index, m_Block, m_FixedHead.blockSize);
                index += m_FixedHead.blockSize;

                if (ret == m_FixedHead.blockSize)
                {
                    pIndexField = (IndexNode_t *)m_Block;
                    for (FILE_STORAGE_INT iLoop = 0; iLoop < m_FixedHead.blockNumForEachPage - 1; iLoop++)
                    {
                        cacheInfo.referenceIndex = pIndexField[iLoop].referenceIndex;
                        cacheInfo.leftLen = pIndexField[iLoop].leftLen;
                        if (cacheInfo.leftLen != m_FixedHead.blockSize) 
                        {
                            m_MaxUsedBlock = index / m_FixedHead.blockSize + iLoop;
                        }
                        m_freeCache[cacheInfo.leftLen][index + m_FixedHead.blockSize * iLoop] = cacheInfo;
                    }
                }

                index += (m_FixedHead.blockNumForEachPage  - 1)* m_FixedHead.blockSize;
           }
        }

        void createNewStorageFile()
        {
            memset(&m_FixedHead, 0x00, sizeof(FixedHead_t));

            memcpy(m_FixedHead.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO));

            m_FixedHead.pageNum = 0;

            m_FixedHead.blockSize = FILE_STORAGE_BLOCK_SIZE;

            m_FixedHead.blockNumForEachPage = FILE_STORAGE_BLOCK_SIZE / sizeof(IndexNode_t) + 1;

            m_FixedHead.pageSize = m_FixedHead.blockSize * m_FixedHead.blockNumForEachPage;

            m_MaxUsedBlock = 0;

            if (m_Block != NULL)
            {
                ::free(m_Block);
            }

            m_Block = (FILE_STORAGE_UNSIGNED_CHAR *)::malloc(m_FixedHead.blockSize);

            writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));
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

        void WritePageIndex(FILE_STORAGE_UNSIGNED_LONG index, IndexNode_t & indexNode)
        {
            FILE_STORAGE_UNSIGNED_LONG writeIndex;

            FILE_STORAGE_UNSIGNED_LONG tmp;

            tmp = index / m_FixedHead.blockSize;

            writeIndex = m_FixedHead.blockSize  + ((tmp - 1) / m_FixedHead.blockNumForEachPage) * m_FixedHead.pageSize + ((tmp - 1 - 1) % m_FixedHead.blockNumForEachPage) * sizeof(IndexNode_t);

            writeToFile(writeIndex, &indexNode, sizeof(IndexNode_t));
        }

        void allocatePage()
        {
            IndexNode_t * pIndexField = NULL;

            FreeBlockInfoCache_t cacheInfo;

            FILE_STORAGE_UNSIGNED_LONG writeIndex = m_FixedHead.blockSize + m_FixedHead.pageNum * m_FixedHead.pageSize;
            m_FixedHead.pageNum++;

            writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));

            pIndexField = (IndexNode_t *)m_Block;

            for (FILE_STORAGE_INT iLoop = 0; iLoop < m_FixedHead.blockNumForEachPage - 1; iLoop++)
            {
                cacheInfo.referenceIndex = pIndexField[iLoop].referenceIndex = 0;
                cacheInfo.leftLen = pIndexField[iLoop].leftLen = m_FixedHead.blockSize;
                m_freeCache[cacheInfo.leftLen][writeIndex + m_FixedHead.blockSize * (iLoop + 1)] = cacheInfo;
            }

            writeToFile(writeIndex, m_Block, m_FixedHead.blockSize);

        }

        
    private:
        FixedHead_t m_FixedHead;

        std::string  m_FileName;

        BaseFileIO * m_Storage;

        FILE_STORAGE_UNSIGNED_CHAR * m_Block;

        std::map <FILE_STORAGE_UNSIGNED_LONG, FreeBlockInfoCache_t> m_freeCache[FILE_STORAGE_BLOCK_SIZE + 1];

        FILE_STORAGE_UNSIGNED_LONG m_MaxUsedBlock;

    };

    #define FIRSTFILESTORAGEBLOCKHANDLE 2048
}
