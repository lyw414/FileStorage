#include "FileIO.hpp"
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
#define FILE_STORAGE_BLOCK_SIZE 1024


namespace LYW_CODE
{
    typedef FILE_STORAGE_UNSIGNED_LONG  FileStorageHandle;
    template <typename IOType = FileIO>
    class FileStorage
    {
    public:
        typedef FILE_STORAGE_UNSIGNED_LONG  FileStorageHandle;
    private:
        typedef struct _FixedHead
        {
            FILE_STORAGE_CHAR verifyInfo[32];
            FILE_STORAGE_UNSIGNED_LONG blockSize;
            FILE_STORAGE_UNSIGNED_LONG blockUserSize;
            FILE_STORAGE_UNSIGNED_LONG userHandle[8];
            FILE_STORAGE_UNSIGNED_LONG usedBlockEnd;
            FILE_STORAGE_UNSIGNED_LONG freeBlockBegin;
        } FixedHead_t;

        typedef struct _BlockHead
        {
            FILE_STORAGE_UNSIGNED_LONG  preBlock;
            FILE_STORAGE_UNSIGNED_LONG  nextBlock;
            FILE_STORAGE_UNSIGNED_INT leftLen;
            FILE_STORAGE_INT referenceNum;
        } BlockHead_t;


        typedef struct _DataNode
        {
            FILE_STORAGE_UNSIGNED_LONG len;
        } DataNode_t;

    public:
        FileStorage (const std::string & fileName)
        {
            m_FileName = fileName;
            m_Storage = new IOType;
            m_LastBlock = 0;
            m_Block = NULL;
            memset(&m_FixedHead, 0x00, sizeof(FixedHead_t));
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
                return false;
            }

            if (index >= sizeof(m_FixedHead.userHandle))
            { 
                return false;
            }

            m_FixedHead.userHandle[index] = handle;

            writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));
            return true;
        }

        FileStorageHandle allocate(FILE_STORAGE_UNSIGNED_INT size)
        {

            FileStorageHandle handle = 0;

            FILE_STORAGE_UNSIGNED_LONG leftLen = 0;

            BlockHead_t blockHead = {0};

            DataNode_t  dataNode = {0};

            bool isWriteFixedHead = false;

            FILE_STORAGE_UNSIGNED_LONG currentBlockIndex = 0;

            FILE_STORAGE_UNSIGNED_LONG nextBlockIndex = 0;

            if (size == 0) 
            {
                return 0;
            }

            if (!IsInit())
            {
                return 0;
            }

            /*no used block*/ 
            if (m_FixedHead.usedBlockEnd == 0)
            {
                currentBlockIndex = m_FixedHead.usedBlockEnd = allocateBlock();
                blockHead.preBlock = 0;
                blockHead.nextBlock = 0;
                blockHead.referenceNum = 0;
                blockHead.leftLen = m_FixedHead.blockUserSize;
                isWriteFixedHead = true;

            }
            else
            {
                currentBlockIndex = m_FixedHead.usedBlockEnd;
                readFromFile(currentBlockIndex, &blockHead,sizeof(BlockHead_t));

                if (blockHead.leftLen == 0)
                {
                    blockHead.nextBlock = allocateBlock();
                    writeToFile(currentBlockIndex, &blockHead, sizeof(BlockHead_t));

                    currentBlockIndex = m_FixedHead.usedBlockEnd = blockHead.nextBlock;

                    isWriteFixedHead = true;

                    blockHead.leftLen = m_FixedHead.blockUserSize;
                    blockHead.referenceNum = 0;;

                    blockHead.nextBlock = 0;

                    blockHead.preBlock = currentBlockIndex;
                }
            }

            handle = currentBlockIndex + m_FixedHead.blockSize - blockHead.leftLen;

            /*allocate*/
            leftLen = sizeof(dataNode) + size;

            while (leftLen > 0)
            {
                if (blockHead.leftLen < leftLen)
                {
                    leftLen -= blockHead.leftLen;

                    blockHead.leftLen = 0;
                    blockHead.referenceNum++;
                    blockHead.nextBlock = allocateBlock();

                    /*set fixed head*/
                    m_FixedHead.usedBlockEnd = blockHead.nextBlock;
                    isWriteFixedHead = true;

                    writeToFile(currentBlockIndex, &blockHead, sizeof(BlockHead_t));

                    /*set next block head info*/
                    blockHead.leftLen = m_FixedHead.blockUserSize;
                    blockHead.referenceNum = 0;;
                    blockHead.nextBlock = 0;
                    blockHead.preBlock = currentBlockIndex;

                    currentBlockIndex = m_FixedHead.usedBlockEnd;
                }
                else
                {
                    /*write data node*/
                    blockHead.referenceNum++;
                    blockHead.leftLen -= leftLen;
                    writeToFile(m_FixedHead.usedBlockEnd, &blockHead, sizeof(BlockHead_t));
                    break;
                }
            }

            /*writeDataNode*/
            dataNode.len = size;

            if (!writeDataNode(handle, &dataNode))
            {
                printf("%d allocate failed\n", __LINE__);
                return 0;
            }
            
            /*is need update fixed head*/
            if (isWriteFixedHead)
            {
                writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));
            }

            return handle;
        }


        int write(FileStorageHandle handle, void * buf, FILE_STORAGE_UNSIGNED_LONG lenOfBuf)
        {
            DataNode_t dataNode = {0};
            BlockHead_t blockHead = {0};
            FILE_STORAGE_UNSIGNED_LONG blockLeftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG leftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG writeLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            if (!IsInit())
            {
                return 0;
            }



            /*read dataNode*/
            if ((beginIndex = readDataNode(handle, &dataNode)) == 0)
            {

                printf("%d write failed\n", __LINE__);
                return 0;
            }

            /*write data len*/
            writeLen = 0;
            if (dataNode.len < lenOfBuf)
            {
                leftLen = dataNode.len;
            }
            else
            {
                leftLen = lenOfBuf;
            }

            /*write data*/
            while(leftLen > 0)
            {
                blockLeftLen = m_FixedHead.blockSize - beginIndex % m_FixedHead.blockSize;
                if (leftLen <= blockLeftLen)
                {
                    writeToFile(beginIndex, (unsigned char *)buf + writeLen, leftLen);
                    writeLen += leftLen;
                    leftLen = 0;
                    return writeLen;
                }
                else
                {
                    writeToFile(beginIndex, (unsigned char *)buf + writeLen, blockLeftLen);
                    writeLen += blockLeftLen;
                    leftLen -= blockLeftLen;
                    beginIndex = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);
                    readFromFile(beginIndex, &blockHead, sizeof(BlockHead_t));
                    beginIndex = blockHead.nextBlock + sizeof(BlockHead_t);
                    if (blockHead.nextBlock == 0 && leftLen > 0)
                    {
                        printf("%d write failed\n", __LINE__);
                        return 0;
                    }
                }
            }
            return writeLen;
        }



        int write(FileStorageHandle handle, FILE_STORAGE_UNSIGNED_LONG pos, void * buf, FILE_STORAGE_UNSIGNED_LONG lenOfBuf)
        {
            DataNode_t dataNode = {0};
            BlockHead_t blockHead = {0};
            FILE_STORAGE_UNSIGNED_LONG blockLeftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG posLeftLen = pos;
            FILE_STORAGE_UNSIGNED_LONG leftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG writeLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;


            if (!IsInit())
            {
                return 0;
            }

            /*read dataNode*/
            if ((beginIndex = readDataNode(handle, &dataNode)) == 0)
            {
                return 0;
            }

            /*write data len*/
            writeLen = 0;
            if (dataNode.len - pos < lenOfBuf)
            {
                leftLen = dataNode.len - pos;
            }
            else
            {
                leftLen = lenOfBuf;
            }

            /*write data*/
            while(leftLen > 0)
            {
                blockLeftLen = m_FixedHead.blockSize - beginIndex % m_FixedHead.blockSize;

                if (posLeftLen >= blockLeftLen) 
                {
                    posLeftLen -= blockLeftLen;
                }
                else
                {

                    if (leftLen <= blockLeftLen - posLeftLen)
                    {
                        writeToFile(beginIndex + posLeftLen, (unsigned char *)buf + writeLen, leftLen);
                        writeLen += leftLen;
                        leftLen = 0;
                        return writeLen;
                    }
                    else
                    {
                        writeToFile(beginIndex + posLeftLen, (unsigned char *)buf + writeLen, blockLeftLen - posLeftLen);
                        writeLen += (blockLeftLen - posLeftLen);
                        leftLen -= (blockLeftLen - posLeftLen);

                        posLeftLen = 0;

                        beginIndex = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);

                        readFromFile(beginIndex, &blockHead, sizeof(BlockHead_t));
                        beginIndex = blockHead.nextBlock + sizeof(BlockHead_t);
                        if (blockHead.nextBlock == 0 && leftLen > 0)
                        {
                            return 0;
                        }
                    }
                }
            }
            return writeLen;
        }


        int read (FileStorageHandle handle, FILE_STORAGE_UNSIGNED_LONG pos, void * buf,FILE_STORAGE_UNSIGNED_LONG len)
        {
            DataNode_t dataNode = {0};
            BlockHead_t blockHead = {0};
            FILE_STORAGE_UNSIGNED_LONG blockLeftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG posLeftLen = pos;
            FILE_STORAGE_UNSIGNED_LONG leftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG readLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            if (!IsInit())
            {
                return 0;
            }



            /*read dataNode*/
            if ((beginIndex = readDataNode(handle, &dataNode)) == 0)
            {
                return 0;
            }

            /*read data len*/
            readLen = 0;
            if (dataNode.len - pos < len)
            {
                leftLen = dataNode.len - pos;
            }
            else
            {
                leftLen = len;
            }

            /*read data*/
            while(leftLen > 0)
            {
                blockLeftLen = m_FixedHead.blockSize - beginIndex % m_FixedHead.blockSize;
                if (posLeftLen >= blockLeftLen)
                {
                    posLeftLen -= blockLeftLen;
                }
                else
                {
                    if (leftLen <= blockLeftLen - posLeftLen)
                    {
                        readFromFile(beginIndex + posLeftLen, (unsigned char *)buf + readLen, leftLen);
                        readLen += leftLen;
                        leftLen = 0;
                        return readLen;
                    }
                    else
                    {
                        readFromFile(beginIndex + posLeftLen, (unsigned char *)buf + readLen, blockLeftLen);
                        readLen += (blockLeftLen - posLeftLen);
                        leftLen -= (blockLeftLen - posLeftLen);

                        posLeftLen = 0;
                        beginIndex = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);
                        readFromFile(beginIndex, &blockHead, sizeof(BlockHead_t));
                        beginIndex = blockHead.nextBlock + sizeof(BlockHead_t);
                        if (blockHead.nextBlock == 0 && leftLen > 0)
                        {
                            return 0;
                        }
                    }
                }
            }
            return readLen;
        }

        int read (FileStorageHandle handle, void * buf, FILE_STORAGE_UNSIGNED_LONG len)
        {
            DataNode_t dataNode = {0};
            BlockHead_t blockHead = {0};
            FILE_STORAGE_UNSIGNED_LONG blockLeftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG leftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG readLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            if (!IsInit())
            {
                return 0;
            }



            /*read dataNode*/
            if ((beginIndex = readDataNode(handle, &dataNode)) == 0)
            {
                return 0;
            }

            /*read data len*/
            readLen = 0;
            if (dataNode.len < len)
            {
                leftLen = dataNode.len;
            }
            else
            {
                leftLen = len;
            }

            /*read data*/
            while(leftLen > 0)
            {
                blockLeftLen = m_FixedHead.blockSize - beginIndex % m_FixedHead.blockSize;
                if (leftLen <= blockLeftLen)
                {
                    readFromFile(beginIndex, (unsigned char *)buf + readLen, leftLen);
                    readLen += leftLen;
                    leftLen = 0;
                    return readLen;
                }
                else
                {
                    readFromFile(beginIndex, (unsigned char *)buf + readLen, blockLeftLen);
                    readLen += blockLeftLen;
                    leftLen -= blockLeftLen;
                    beginIndex = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);
                    readFromFile(beginIndex, &blockHead, sizeof(BlockHead_t));
                    beginIndex = blockHead.nextBlock + sizeof(BlockHead_t);
                    if (blockHead.nextBlock == 0 && leftLen > 0)
                    {
                        return 0;
                    }
                }
            }
            return readLen;
        }


        bool copy( FileStorageHandle dstHandle, unsigned long dstPos, FileStorageHandle srcHandle, unsigned long srcPos, unsigned long cpyLen)
        {
            return false;
        }

        bool free(FileStorageHandle handle)
        {
            DataNode_t dataNode = {0};
            BlockHead_t blockHead = {0};
            FILE_STORAGE_UNSIGNED_LONG blockLeftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG leftLen = 0;
            FILE_STORAGE_UNSIGNED_LONG readLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            if (!IsInit())
            {
                return false;
            }

            /*read dataNode*/
            if ((beginIndex = readDataNode(handle, &dataNode)) == 0)
            {
                return false;
            }



            /*write data len*/
            readLen = 0;

            leftLen = dataNode.len + sizeof(DataNode_t);
            
            beginIndex = handle;

            /*write data*/
            while(leftLen > 0)
            {
                blockLeftLen = m_FixedHead.blockSize - beginIndex % m_FixedHead.blockSize;

                beginIndex = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);

                readFromFile(beginIndex, &blockHead, sizeof(BlockHead_t));

                //blockHead.referenceNum--;

                freeBlock(beginIndex, &blockHead);

                if (leftLen <= blockLeftLen)
                {
                    return true;
                }
                else
                {
                    leftLen -= blockLeftLen;
                    beginIndex = blockHead.nextBlock;
                    if (blockHead.nextBlock == 0 && leftLen > 0)
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        FILE_STORAGE_UNSIGNED_LONG size(FileStorageHandle handle)
        {
            DataNode_t dataNode;

            if (readDataNode(handle, &dataNode) == 0)
            {
                return 0;
            }

            return dataNode.len;
        }

        void fset(FileStorageHandle handle, unsigned char ch)
        {
            DataNode_t dataNode;
            BlockHead_t blockHead;
            FILE_STORAGE_UNSIGNED_LONG blockLeftLen;
            FILE_STORAGE_UNSIGNED_LONG leftLen;
            FILE_STORAGE_UNSIGNED_LONG writeLen = 0;
            FILE_STORAGE_UNSIGNED_LONG beginIndex = handle;

            if (!IsInit())
            {
                return ;
            }

            /*read dataNode*/
            if ((beginIndex = readDataNode(handle, &dataNode)) == 0)
            {
                return ;
            }

            /*write data len*/
            writeLen = 0;

            leftLen = dataNode.len;

            memset(m_Block,ch,m_FixedHead.blockSize);

            /*write data*/
            while(leftLen > 0)
            {
                blockLeftLen = m_FixedHead.blockSize - beginIndex % m_FixedHead.blockSize;
                if (leftLen <= blockLeftLen)
                {
                    writeToFile(beginIndex, (unsigned char *)m_Block, leftLen);
                    writeLen += leftLen;
                    leftLen = 0;
                    return ;
                }
                else
                {
                    writeToFile(beginIndex, (unsigned char *)m_Block, blockLeftLen);
                    writeLen += blockLeftLen;
                    leftLen -= blockLeftLen;
                    beginIndex = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);
                    readFromFile(beginIndex, &blockHead, sizeof(BlockHead_t));
                    beginIndex = blockHead.nextBlock + sizeof(BlockHead_t);
                    if (blockHead.nextBlock == 0 && leftLen > 0)
                    {
                        return ;
                    }
                }
            }
            return ;

        }

        void clearFile()
        {
            m_Storage->close();
            m_Storage->open(m_FileName, 1);
            m_Storage->close();
        }

    private:
    //public:
        void clearFileEnd()
        {

            BlockHead_t blockHead;

            BlockHead_t freeBlockHead;

            FILE_STORAGE_UNSIGNED_LONG beginIndex = 0;

            while (true)
            {

                beginIndex = m_LastBlock * m_FixedHead.blockSize;

                if (beginIndex == 0)
                {
                    m_Storage->ftruncate((m_LastBlock + 1) * m_FixedHead.blockSize);
                    return;
                }

                readFromFile(beginIndex, &blockHead, sizeof(BlockHead_t));

                if (blockHead.referenceNum != 0)
                {
                    m_Storage->ftruncate((m_LastBlock + 1) * m_FixedHead.blockSize);
                    return;
                }

                if (blockHead.preBlock != 0)
                {
                    readFromFile(blockHead.preBlock, &freeBlockHead, sizeof(BlockHead_t));
                    freeBlockHead.nextBlock = blockHead.nextBlock;
                    writeToFile(blockHead.preBlock, &freeBlockHead, sizeof(BlockHead_t));
                }
                else
                {
                    m_FixedHead.freeBlockBegin = blockHead.nextBlock;
                    writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));
                }


                if (blockHead.nextBlock != 0)
                {
                    readFromFile(blockHead.nextBlock, &freeBlockHead, sizeof(BlockHead_t));
                    freeBlockHead.preBlock = blockHead.preBlock;
                    writeToFile(blockHead.nextBlock, &freeBlockHead, sizeof(BlockHead_t));
                }

                m_LastBlock--;
            }
        }

        void freeBlock(FILE_STORAGE_UNSIGNED_LONG index, BlockHead_t * pBlockHead = NULL)
        {
            BlockHead_t blockHead;

            BlockHead_t freeBlockHead;

            BlockHead_t usedBlockHead;

            FILE_STORAGE_UNSIGNED_LONG beginIndex = m_FixedHead.blockSize * (index / m_FixedHead.blockSize);

            if (pBlockHead == NULL) 
            {
                readFromFile(beginIndex, &blockHead, sizeof(BlockHead_t));
            }
            else
            {
                ::memcpy(&blockHead, pBlockHead, sizeof(BlockHead_t));
            }

            if (blockHead.referenceNum - 1 <= 0)
            {

                if (beginIndex == m_FixedHead.usedBlockEnd)
                {
                    m_FixedHead.usedBlockEnd = blockHead.preBlock;
                }
                else
                {
                    if (blockHead.nextBlock != 0)
                    {
                        readFromFile(blockHead.nextBlock, &usedBlockHead, sizeof(BlockHead_t));
                        usedBlockHead.preBlock = blockHead.preBlock;

                        writeToFile(blockHead.nextBlock, &usedBlockHead, sizeof(BlockHead_t));
                    }
                    
                }

                if (m_FixedHead.freeBlockBegin == 0)
                {
                    m_FixedHead.freeBlockBegin = beginIndex;
                    writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));
                    blockHead.preBlock = 0;
                    blockHead.nextBlock = 0;
                    blockHead.referenceNum = 0;
                    blockHead.leftLen = m_FixedHead.blockUserSize;
                    writeToFile(beginIndex, &blockHead, sizeof(BlockHead_t));
                }
                else
                {
                    readFromFile(m_FixedHead.freeBlockBegin, &freeBlockHead, sizeof(BlockHead_t));
                    freeBlockHead.preBlock = beginIndex;
                    writeToFile(m_FixedHead.freeBlockBegin, &freeBlockHead, sizeof(BlockHead_t));

                    blockHead.preBlock = 0;
                    blockHead.nextBlock = m_FixedHead.freeBlockBegin;
                    blockHead.referenceNum = 0;
                    blockHead.leftLen = m_FixedHead.blockUserSize;
                    writeToFile(beginIndex, &blockHead, sizeof(BlockHead_t));

                    m_FixedHead.freeBlockBegin = beginIndex;
                    writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));
                }

                if (beginIndex / m_FixedHead.blockSize == m_LastBlock)
                {
                    clearFileEnd();
                }
            }
            else
            {
                blockHead.referenceNum--;
                writeToFile(beginIndex, &blockHead, sizeof(BlockHead_t));
            }
        }

        FILE_STORAGE_UNSIGNED_LONG readDataNode(FILE_STORAGE_UNSIGNED_LONG beginIndex, DataNode_t * dataNode)
        {
            FILE_STORAGE_UNSIGNED_LONG leftLen = sizeof(DataNode_t);

            FILE_STORAGE_UNSIGNED_LONG tmp = 0;

            FILE_STORAGE_UNSIGNED_LONG endIndex;

            BlockHead_t blockHead;

            tmp = m_FixedHead.blockSize - beginIndex % m_FixedHead.blockSize;
            
            if (tmp >= leftLen)
            {
                readFromFile(beginIndex, dataNode, sizeof(DataNode_t));
                endIndex =  beginIndex + sizeof(DataNode_t);
                if (endIndex % m_FixedHead.blockSize == 0)
                {
                    tmp = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);
                    readFromFile(tmp, &blockHead, sizeof(BlockHead_t));

                    if (blockHead.nextBlock == 0)
                    {
                        return 0;
                    }
                    
                    endIndex = blockHead.nextBlock + sizeof(BlockHead_t);
                }
            }
            else
            {
                readFromFile(beginIndex, dataNode, tmp);

                leftLen -= tmp;

                tmp = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);

                readFromFile(tmp, &blockHead, sizeof(BlockHead_t));

                if (blockHead.nextBlock == 0)
                {
                    return 0;
                }

                readFromFile(blockHead.nextBlock  + sizeof(BlockHead_t), (char *)&dataNode + sizeof(DataNode_t) - leftLen, leftLen);

                endIndex = blockHead.nextBlock  + sizeof(BlockHead_t) + leftLen;
            }

            return endIndex;

        }

        bool writeDataNode(FILE_STORAGE_UNSIGNED_LONG beginIndex ,DataNode_t * dataNode)
        {
            FILE_STORAGE_UNSIGNED_LONG leftLen = sizeof(DataNode_t);
            FILE_STORAGE_UNSIGNED_LONG tmp = 0;

            BlockHead_t blockHead;

            tmp = m_FixedHead.blockSize - beginIndex % m_FixedHead.blockSize;
            
            if (tmp >= leftLen)
            {
                writeToFile(beginIndex, dataNode, sizeof(DataNode_t));
            }
            else
            {
                writeToFile(beginIndex, dataNode, tmp);

                leftLen -= tmp;

                tmp = m_FixedHead.blockSize * (beginIndex / m_FixedHead.blockSize);
                readFromFile(tmp, &blockHead, sizeof(BlockHead_t));
                if (blockHead.nextBlock == 0)
                {

                    return false;
                }

                writeToFile(blockHead.nextBlock  + sizeof(BlockHead_t), (char *)&dataNode + sizeof(DataNode_t) - leftLen, leftLen);
            }
            return true;
        }

        FILE_STORAGE_UNSIGNED_LONG allocateBlock()
        {
            FILE_STORAGE_UNSIGNED_LONG beginIndex = 0;

            BlockHead_t blockHead;

            if (m_FixedHead.freeBlockBegin == 0)
            {
                /*new*/
                m_LastBlock++;
                beginIndex = m_LastBlock * m_FixedHead.blockSize;
            }
            else
            {
                /*use free*/
                beginIndex = m_FixedHead.freeBlockBegin;
                
                readFromFile(m_FixedHead.freeBlockBegin, &blockHead, sizeof(BlockHead_t));

                m_FixedHead.freeBlockBegin = blockHead.nextBlock;
            }
            return beginIndex;
        }

        bool IsInit()
        {
            if (m_Storage->IsOpen())
            {
                return true;
            }

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
                loadStorageFile();
            }
            else
            {
                createNewStorageFile();
            }
            return 0;
        }


        void loadStorageFile()
        {
            off_t fileLen = 0;

            if ((fileLen = m_Storage->size()) > 0) 
            {
                m_LastBlock = fileLen / m_FixedHead.blockSize;

                if (fileLen % m_FixedHead.blockSize == 0)
                {
                    m_LastBlock--;
                }
            }


            if (m_Block != NULL)
            {
                ::free(m_Block);
            }

            m_Block = (FILE_STORAGE_UNSIGNED_CHAR *)::malloc(m_FixedHead.blockSize);

        }

        void createNewStorageFile()
        {

            memset(&m_FixedHead, 0x00, sizeof(FixedHead_t));
            memcpy(m_FixedHead.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO));

            m_FixedHead.blockSize = FILE_STORAGE_BLOCK_SIZE;
            m_FixedHead.blockUserSize = m_FixedHead.blockSize - sizeof(BlockHead_t);


            if (m_Block != NULL)
            {
                ::free(m_Block);
            }

            m_Block = (FILE_STORAGE_UNSIGNED_CHAR *)::malloc(m_FixedHead.blockSize);

            writeToFile(0, &m_FixedHead, sizeof(FixedHead_t));
            m_LastBlock = 0;

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

    private:
        FixedHead_t m_FixedHead;

        std::string  m_FileName;

        BaseFileIO * m_Storage;

        FILE_STORAGE_UNSIGNED_CHAR * m_Block;

        FILE_STORAGE_UNSIGNED_LONG m_LastBlock;
    };
 
}

