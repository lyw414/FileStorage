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
            FILE_STORAGE_UNSIGNED_INT maxSuccessiveBlockSize;
            FILE_STORAGE_UNSIGNED_INT maxSuccessiveBlockIndex;
        } BlockIndexFieldHead_t;

        typedef struct _QuickAllocateCache
        {
            FILE_STORAGE_UNSIGNED_LONG beginIndex;
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
            memset(m_VerifyInfo, 0x00, sizeof(VerifyInfo_t));

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
                
                for (int iLoop = 0; iLoop < m_FileInfo.pageNum; iLoop++)
                {
                    if (ReadFromFile(index, m_BlockIndex, m_FileInfo.blockIndexFieldSize) > 0)
                    {
                        AnalysisIndexField();
                    }

                    index += m_FileInfo.pageSize;
                }
            }
            else
            {
                /*create new file*/
                memcpy(m_VerifyInfo.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO));

                m_FileInfo.pageNum = 0;

                m_FileInfo.pageSize = FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE + (FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE - sizeof(BlockIndexFieldHead_t)) * FILE_STORAEE_BLOCK_SIZE * 8;

                m_FileInfo.blockIndexFieldSize = FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE;

                m_FileInfo.blockSize = FILE_STORAEE_BLOCK_SIZE;

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

        void AnalysisIndexField()
        {
            //m_BlockIndex
            return;
        }


    private:
        VerifyInfo_t m_VerifyInfo;

        FileInfo_t m_FileInfo;

        BlockIndexFieldHead_t m_BlockIndexFieldHead;

        std::string m_FileName;

        std::map <FILE_STORAGE_UNSIGNED_INT, std::map <QuickAllocateCache_t> > m_quickAllocate;

        BaseFileIO * m_Storage;

        FILE_STORAGE_UNSIGNED_CHAR * m_BlockIndex;

    private:
    };
}
