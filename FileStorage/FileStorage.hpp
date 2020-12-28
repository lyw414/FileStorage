#include "FileIO.hpp"
#include <map>
#include <string.h>

#define FileStorageHandle unsigned long
#define FileStorageCheckInfo "**LYW_CODE**"

namespace LYW_CODE
{
    typedef enum _BlockState
    {
        UsedBlock,
        WaitMergerBlock
    } BlockState_t;

    typedef struct _VerifyInfo
    {
        char buf[16];
    } VerifyInfo_t;

    typedef struct _FixedHead
    {
        unsigned long fileEndIndex;
    } FixedHead_t;
    
    /*data block include two part first : index info second : data*/
    typedef struct _StorageBlock
    {
        FileStorageHandle blockBeginIndex;
        unsigned int lenOfData;
        BlockState_t blockState;
    } StorageBlock_t;

    template <typename IOType = FileIO>
    class FileStorage
    {
    public:
        FileStorage(const std::string &fileName)
        {
            m_Storage = new IOType;

            m_FileName = fileName;

            m_UsedBlock.clear();

            m_FreeBlockBeginIndex.clear();

            m_FreeBlockEndIndex.clear();
        }

        ~FileStorage()
        {
            if (m_Storage != NULL)
            {
                delete m_Storage;
                m_Storage = NULL;
            }
        }
        int read (FileStorageHandle handle, unsigned int pos, void * buf, unsigned long len)
        {
            std::map<FileStorageHandle,StorageBlock_t> :: iterator it;
            int ret = 0;

            if (buf == NULL || len == 0)
            {
                return -3;
            }
            if (!IsInit())
            {
                return -1;
            }

            it = m_UsedBlock.find(handle);
            
            if (it != m_UsedBlock.end())
            {
                if (m_Storage->lseek(handle + sizeof(StorageBlock_t) + pos, SEEK_SET) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                len = len < (it->second.lenOfData - pos) ? len : (it->second.lenOfData - pos);

                if((ret = m_Storage->read(buf, len, len)) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                return ret;
            }
            else
            {
                /*not found*/
                return -2;
            }
        }



        /**
         * @brief               read data by handle
         *
         * @param handle        storage handle
         * @param buf           read buffer
         * @param len           read length
         *
         * @return  >   0       success     read data len
         *          <   0       failed      errorcode
         */
        int read (FileStorageHandle handle, void * buf, unsigned long len)
        {
            std::map<FileStorageHandle,StorageBlock_t> :: iterator it;
            int ret = 0;

            if (buf == NULL || len == 0)
            {
                return -3;
            }
            if (!IsInit())
            {
                return -1;
            }

            it = m_UsedBlock.find(handle);
            
            if (it != m_UsedBlock.end())
            {
                if (m_Storage->lseek(handle + sizeof(StorageBlock_t), SEEK_SET) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                len = len < it->second.lenOfData ? len : it->second.lenOfData;

                if((ret = m_Storage->read(buf, len, len)) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                return ret;
            }
            else
            {
                /*not found*/
                return -2;
            }
        }

        int write(FileStorageHandle handle, unsigned int pos, void * buf, unsigned long lenOfBuf)
        {
            int ret = 0;
            std::map<FileStorageHandle,StorageBlock_t> :: iterator it;
            if (!IsInit())
            {
                return -1;
            }
            
            if (buf == NULL || lenOfBuf == 0)    
            {
                return -3;
            }
            
            it = m_UsedBlock.find(handle);

            if (it != m_UsedBlock.end())
            {
                if (lenOfBuf > it->second.lenOfData - pos)
                {
                    return -4;
                }

                if (m_Storage->lseek(handle + sizeof(StorageBlock_t) + pos, SEEK_SET) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                if((ret = m_Storage->write(buf, lenOfBuf)) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                return lenOfBuf;
            }
            else
            {
                return -2;
            }
        }



        int write(FileStorageHandle handle, void * buf, unsigned long lenOfBuf)
        {
            int ret = 0;
            std::map<FileStorageHandle,StorageBlock_t> :: iterator it;
            if (!IsInit())
            {
                return -1;
            }
            
            if (buf == NULL || lenOfBuf == 0)    
            {
                return -3;
            }
            
            it = m_UsedBlock.find(handle);

            if (it != m_UsedBlock.end())
            {
                if (lenOfBuf > it->second.lenOfData)
                {
                    return -4;
                }

                if (m_Storage->lseek(handle + sizeof(StorageBlock_t), SEEK_SET) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                if((ret = m_Storage->write(buf, lenOfBuf)) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                return lenOfBuf;
            }
            else
            {
                return -2;
            }
        }

        /**
         * @brief           allocate buffer in storage file
         *
         * @param size      size of buffer
         *
         * @return  > 0     success handle
         *          = 0     failed
         */
        FileStorageHandle allocate (unsigned long size)
        {
            unsigned long blockSize = size + sizeof(StorageBlock_t);
            StorageBlock_t storageBlock;
            int ret = 0;

            if (!IsInit())
            {
                return 0;
            }
        
            std::map<FileStorageHandle,StorageBlock_t> :: iterator it;
            for (it = m_FreeBlockBeginIndex.begin(); it != m_FreeBlockBeginIndex.end(); it++)
            {
                if (it->second.lenOfData > blockSize)
                {
                    StorageBlock_t tmpBlock;
                    memcpy(&tmpBlock,&(it->second), sizeof(StorageBlock_t));
                    tmpBlock.lenOfData -= blockSize;

                    storageBlock.blockBeginIndex = it->second.blockBeginIndex + sizeof(StorageBlock_t) + it->second.lenOfData - blockSize;
                    storageBlock.blockState = UsedBlock;
                    storageBlock.lenOfData = size;
                    
                    /*Update Free Block*/ 
                    if ((ret = WriteToFile(tmpBlock.blockBeginIndex, (void * )&tmpBlock, sizeof(StorageBlock_t))) < 0)
                    {
                        return ret;
                    }

                    m_FreeBlockEndIndex.erase(it->first + sizeof(StorageBlock_t) + it->second.lenOfData);
                    m_FreeBlockEndIndex[storageBlock.blockBeginIndex] = tmpBlock;

                    it->second.lenOfData = tmpBlock.lenOfData;

                    /*Update Used Block*/
                    if ((ret = WriteToFile(storageBlock.blockBeginIndex, (void * )&storageBlock, sizeof(StorageBlock_t))) < 0)
                    {
                        return ret;
                    }

                    m_UsedBlock[storageBlock.blockBeginIndex] = storageBlock;
                    return storageBlock.blockBeginIndex;

                }
                else if (it->second.lenOfData == size)
                {
                    memcpy(&storageBlock,&(it->second), sizeof(StorageBlock_t));
                    storageBlock.blockState = UsedBlock;
                    if ((ret = WriteToFile(storageBlock.blockBeginIndex, (void * )&storageBlock, sizeof(StorageBlock_t))) < 0)
                    {
                        return ret;
                    }

                    m_UsedBlock[storageBlock.blockBeginIndex] = storageBlock;
                    m_FreeBlockBeginIndex.erase(storageBlock.blockBeginIndex);
                    m_FreeBlockEndIndex.erase(storageBlock.blockBeginIndex + sizeof(StorageBlock_t) + storageBlock.lenOfData);

                    return  storageBlock.blockBeginIndex;
                }
            }

            /*not found enough free block*/
            storageBlock.blockBeginIndex = m_FixedHead.fileEndIndex;
            storageBlock.blockState = UsedBlock;
            storageBlock.lenOfData = size;
                    
            /*Update Used Block*/
            if ((ret = WriteToFile(storageBlock.blockBeginIndex, (void * )&storageBlock, sizeof(StorageBlock_t))) < 0)
            {
                return ret;
            }

            m_UsedBlock[storageBlock.blockBeginIndex] = storageBlock;

            /*update fixed head*/
            m_FixedHead.fileEndIndex += blockSize;
            if ((ret = WriteToFile(sizeof(VerifyInfo_t), (void * )&m_FixedHead, sizeof(FixedHead_t))) < 0)
            {
                m_FixedHead.fileEndIndex -= blockSize;
                return ret;
            }

            return storageBlock.blockBeginIndex;
        }
         
        /**
         * @brief                   free block
         *
         * @param handle            handle
         *
         *
         * return   true            success
         *          false           failed
         */
        bool Free(FileStorageHandle handle)
        {
            StorageBlock_t storageBlock;
            int ret = 0;
            std::map<FileStorageHandle,StorageBlock_t> :: iterator it;
            unsigned int beginIndex;
            unsigned int endIndex;
            unsigned int tmpIndex;

            if (!IsInit())
            {
                return false;
            }

            /*Get Used Block Info*/
            it = m_UsedBlock.find(handle);

            if (it == m_UsedBlock.end()) 
            {
                return true;
            }
            
            //memcpy(&storageBlock,&(it->second),sizeof(StorageBlock_t));
            beginIndex = it->second.blockBeginIndex;
            endIndex = it->second.blockBeginIndex +  sizeof(StorageBlock_t) + it->second.lenOfData;
            tmpIndex = 0;

            m_UsedBlock.erase(beginIndex);

            /*forward merger, clear cache*/
            it = m_FreeBlockEndIndex.find(beginIndex);
            if (it != m_FreeBlockEndIndex.end())
            {
                tmpIndex = beginIndex;
                beginIndex = it->second.blockBeginIndex;
                m_FreeBlockEndIndex.erase(tmpIndex);
                m_FreeBlockBeginIndex.erase(beginIndex);
            }

            /*backward merger, clear cache*/
            it = m_FreeBlockBeginIndex.find(endIndex);
            if (it != m_FreeBlockBeginIndex.end())
            {
                tmpIndex = endIndex;
                endIndex = it->second.blockBeginIndex + sizeof(StorageBlock_t) + it->second.lenOfData;
                m_FreeBlockBeginIndex.erase(tmpIndex);
                m_FreeBlockEndIndex.erase(endIndex);
            }

            /*check Delete*/
            if (endIndex == m_FixedHead.fileEndIndex)
            {
                m_FixedHead.fileEndIndex -= (endIndex - beginIndex);
                if ((ret = WriteToFile(sizeof(VerifyInfo_t), (void *)&m_FixedHead, sizeof(FixedHead_t))) < 0)
                 {
                    m_FixedHead.fileEndIndex = m_FixedHead.fileEndIndex + storageBlock.lenOfData + sizeof(FixedHead_t);
                    return false;
                 }
                 m_Storage->ftruncate(m_FixedHead.fileEndIndex);
                 return true;
            }

            /*Can't merger*/
            storageBlock.blockBeginIndex = beginIndex;
            storageBlock.lenOfData = endIndex - beginIndex - sizeof(StorageBlock_t);
            storageBlock.blockState = WaitMergerBlock;

            /*Updata file*/
            if ((ret = WriteToFile(storageBlock.blockBeginIndex, (void *)&storageBlock, sizeof(StorageBlock_t))) < 0)
            {
                return false;
            }
            
            /*Updata Cache*/
            m_FreeBlockEndIndex[endIndex] = storageBlock;
            m_FreeBlockBeginIndex[beginIndex] = storageBlock;
            return true;
        }

        void fset(FileStorageHandle handle, unsigned char ch)
        {            
            unsigned long Len = 0;
            std::map<FileStorageHandle,StorageBlock_t> :: iterator it;
            if (!IsInit())
            {
                return;
            }

            it = m_UsedBlock.find(handle);
            if (it != m_UsedBlock.end())
            {
                Len = it->second.lenOfData;
                m_Storage->lseek(it->second.blockBeginIndex + sizeof(StorageBlock_t), SEEK_SET);
                while(Len > 0)
                {
                    m_Storage->write(&ch,1);
                    Len--;
                }
            }
        }

        unsigned long size(unsigned int handle)
        {
            unsigned long Len = 0;
            std::map<FileStorageHandle,StorageBlock_t> :: iterator it;
            if (!IsInit())
            {
                return 0;
            }

            it = m_UsedBlock.find(handle);

            if (it != m_UsedBlock.end())
            {
                return it->second.lenOfData;
            }
            return 0;
        }

        void clearFile()
        {
            m_Storage->close();
            m_Storage->open(m_FileName, 1);
            m_Storage->close();
        }

    public:

    private:
        /**
         * @brief                   write data to file
         *
         * @param beginIndex        begin Index
         * @param data              data 
         * @param lenOfData         length of data
         *
         * @return  > 0             write data len
         *          < 0             error code
         */
        int WriteToFile(unsigned long beginIndex, void * data, size_t lenOfData) 
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

        /**
         * @brief           Load Storage file to memery
         *
         * @param 
         *
         * @return  >= 0    success     length of file
         *          <  0    failed      error code
         */
        int LoadStorageFile()
        {
        
            VerifyInfo_t verifyInfo;
            memset(&verifyInfo, 0x00, sizeof(VerifyInfo_t));

            unsigned long index = 0;

            StorageBlock_t storageBlock;

            /*check verify info*/
            if (m_Storage->lseek(0,SEEK_SET) < 0)
            {
                m_Storage->close();
                return -2;
            }
            int ret = 0;
            if((ret = m_Storage->read((void *)&verifyInfo,sizeof(VerifyInfo_t), sizeof(VerifyInfo_t))) < 0)
            {
                m_Storage->close();
                return -2;
            }
            if (memcmp(verifyInfo.buf, FileStorageCheckInfo, strlen(FileStorageCheckInfo)) == 0)
            {

                /*check OK load file*/
                /*Load Fixed Head*/
                index += sizeof(VerifyInfo_t);
                if (m_Storage->lseek(index,SEEK_SET) < 0)
                {
                    m_Storage->close();
                    return -2;
                }
 
                if(m_Storage->read((void *)&m_FixedHead,sizeof(FixedHead_t), sizeof(FixedHead_t)) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                index += sizeof(FixedHead_t);
                while (index < m_FixedHead.fileEndIndex)
                {
                    if (m_Storage->lseek(index,SEEK_SET) < 0)
                    {
                        m_Storage->close();
                        return -2;
                    }

                    if(m_Storage->read((void *)&storageBlock,sizeof(StorageBlock_t), sizeof(StorageBlock_t)) < 0)
                    {
                        m_Storage->close();
                        return -2;
                    }

                    index = storageBlock.blockBeginIndex + sizeof(StorageBlock_t) + storageBlock.lenOfData;

                    m_UsedBlock[storageBlock.blockBeginIndex] = storageBlock;
                    if (storageBlock.blockState == WaitMergerBlock)
                    {
                        Free(storageBlock.blockBeginIndex);
                    }
                }
                return 1;
            }
            else
            {
                /*unkown file format , rebuild storage file*/
                m_FixedHead.fileEndIndex = sizeof(VerifyInfo_t) + sizeof(FixedHead_t);
                memcpy(verifyInfo.buf, FileStorageCheckInfo, strlen(FileStorageCheckInfo));

                if (m_Storage->lseek(0,SEEK_SET) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                if(m_Storage->write((const void *)&verifyInfo, sizeof(VerifyInfo_t)) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                if(m_Storage->write((const void *)&m_FixedHead, sizeof(FixedHead_t)) < 0)
                {
                    m_Storage->close();
                    return -2;
                }

                return m_FixedHead.fileEndIndex;
            }
        }



        /**
         * @brief           check is init, if not , init
         *
         * @return  true    success    
         *          false   failed  Must't do any operater
         */
        bool IsInit()
        {
            /*File Open means Init OK!*/
            if (m_Storage->IsOpen())
            {
                return true;
            }

            m_UsedBlock.clear();

            m_FreeBlockBeginIndex.clear();

            m_FreeBlockEndIndex.clear();
            
            if (!m_Storage->open(m_FileName, 0))
            {
                /*open file failed*/
                return false;
            }

            if (LoadStorageFile() < 0)
            {
                /*load file failed*/
                return false;
            }

            return true;
        }


    private:

    private:

        BaseFileIO * m_Storage;

        std::string m_FileName;

        FixedHead_t m_FixedHead;

        std::map<FileStorageHandle,StorageBlock_t> m_UsedBlock;

        /*give back but not merger or delete block*/
        std::map<FileStorageHandle,StorageBlock_t> m_FreeBlockBeginIndex;

        /*give back but not merger or delete block*/
        std::map<FileStorageHandle,StorageBlock_t> m_FreeBlockEndIndex;

    };
}  
#define FIRSTFILESTORAGEBLOCKHANDLE (FileStorageHandle)(sizeof(LYW_CODE::VerifyInfo_t) + sizeof(LYW_CODE::FixedHead_t))
