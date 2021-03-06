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
            FILE_STORAGE_UNSIGNED_INT pageDataSize;
            FILE_STORAGE_UNSIGNED_INT blockIndexFieldSize;
            FILE_STORAGE_UNSIGNED_INT blockSize;
        } FileInfo_t;

        typedef struct _BlockIndexFieldHead
        {
            FILE_STORAGE_UNSIGNED_INT maxSerialBlockSize;
            FILE_STORAGE_UNSIGNED_INT maxSerialBlockIndex;
        } BlockIndexFieldHead_t;

        typedef struct _QuickAllocateInfo
        {
            FILE_STORAGE_UNSIGNED_INT freeBlockBeginIndexInPage;
            FILE_STORAGE_UNSIGNED_INT freeBlockSize;
        } QuickAllocateInfo_t;



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


        /**
        * @brief                allocate buffer
        *
        * @param size           allocate size
        *
        * @return               handle (please use check)
        *            
        */
        FileStorageHandle allocate(FILE_STORAGE_UNSIGNED_INT size)
        {
            FileStorageHandle handle;

            memset(&handle, 0x00, sizeof(FileStorageHandle));

            if (!IsInit())
            {
                return handle;
            }

            if (quickAllocate(handle, size))
            {
                return handle;
            }
            
            if (normalAllocate(handle, size))
            {
                return handle;
            }
            return handle;
        }

        /**
        * @brief                Free buffer
        *
        * @param handle         handle
        *
        */
        void Free(const FileStorageHandle & handle)
        {
            //FILE_STORAGE_UNSIGNED_INT FreePageNum = 0;
            //if (handle.size % m_FileInfo.pageDataSize == 0)

        }

    private:

        /**
        * @brief                if quick allocate set use quick allocate, need thinking more than one page
        *
        * @param handle         handle
        * @param size           allocate size
        *
        * @return   true        success
        *           false       failed
        */
        bool quickAllocate(const FileStorageHandle & handle, FILE_STORAGE_UNSIGNED_INT size)
        {

            FILE_STORAGE_UNSIGNED_INT needPageNum = 0;

            FILE_STORAGE_UNSIGNED_INT freePageIndex = 0;

            FILE_STORAGE_UNSIGNED_INT minFreeSize = 0;

            FILE_STORAGE_UNSIGNED_INT freeBlockBeginIndexInPage = 0;
            QuictAllocateInfo_t quickAllocateInfo;

            std::map <FILE_STORAGE_UNSIGNED_INT, std::map <FILE_STORAGE_UNSIGNED_INT, QuickAllocateInfo_t> > :: iterator it;

            std::map <FILE_STORAGE_UNSIGNED_INT, QuickAllocateInfo_t> :: iterator it1;

            /*if not use quick allocate*/
            if (FILE_STORAGE_IS_USE_CACHE != 1)
            {
                return false;
            }
            
            /*calculate allocate size*/
            handle.size = calculateAllocateSizeBySize(size);

            /*calculate need page*/
            needPageNum = handle.size / m_FileInfo.pageDataSize;
            
            if (handle.size % m_FileInfo.pageDataSize != 0)
            {
                needPageNum++;
            }

            /*find free block in cache*/
            for (it = m_quickAllocate.begin(); it != m_quickAllocate.end(); it++) 
            {
                /*find enough block*/
                if (it->first >= handle.size && it->second.size() > 0)
                {
                    minFreeSize = it->first;
                    it1 = it->second.begin();
                    freePageIndex = it1->first;
                    memcpy(&quickAllocateInfo, &it1->second, sizeof(QuickAllocateInfo_t)):
                    break;
                }
            }

            /*not found , allocate*/
            if (it == m_quickAllocate.end())
            {
                alloacePages(needPageNum, freePageIndex, quickAllocateInfo)
                minFreeSize = CalculateMinAllocateSize(quickAllocateInfo.maxSerialBlockSize);
            }

            handle.beginIndex = sizeof(VerifyInfo_t) + sizeof(FileInfo_t) + freePageIndex * m_FileInfo.pageSize + quickAllocateInfo.freeBlockBeginIndexInPage;

            /*update index field*/
            updataIndexFieldInFileForAllocate(freePageIndex, quickAllocateInfo, handle.size);

            return true;
        }


        /**
        * @brief                    normal allocate use file block index
        *
        * @param handle             handle
        * @param size               allocate size
        *
        * @return 
        */
        bool normalAllocate(const FileStorageHandle & handle, FILE_STORAGE_UNSIGNED_INT size)
        {

            FILE_STORAGE_UNSIGNED_INT needPageNum = 0;

            FILE_STORAGE_UNSIGNED_INT freePageIndex = 0;

            FILE_STORAGE_UNSIGNED_INT serialFreePageNum = 0;

            FILE_STORAGE_UNSIGNED_INT freeSize = 0;

            FILE_STORAGE_UNSIGNED_INT freeBlockBeginIndexInPage = 0;

            FILE_STORAGE_UNSIGNED_LONG index = 0;

            QuictAllocateInfo_t quickAllocateInfo;

            memset(&quickAllocateInfo, 0x00, sizeof(QuictAllocateInfo_t));

            BlockIndexFieldHead_t * blockIndexFieldHead =(BlockIndexFieldHead_t *)m_BlockIndex;

            /*calculate allocate size*/
            handle.size = calculateAllocateSizeBySize(size);

            /*calculate need page*/
            needPageNum = handle.size / m_FileInfo.pageDataSize;
            
            if (handle.size % m_FileInfo.pageDataSize != 0)
            {
                needPageNum++;
            }

            /*find free block in file*/
            index = sizeof(VerifyInfo_t) + sizeof(FileInfo_t);
            for (FILE_STORAGE_UNSIGNED_INT iLoop = 0; iLoop < m_FileInfo.pageNum; iLoop++)
            {
                readFromFile(index, blockIndexFieldHead, sizeof(BlockIndexFieldHead_t));
                if (blockIndexFieldHead->maxSerialBlockSize == m_FileInfo.pageDataSize) 
                {
                    serialFreePageNum++;
                    freeSize = m_FileInfo.pageDataSize * serialFreePageNum;
                }
                else if (blockIndexFieldHead->maxSerialBlockSize == 0) 
                {
                    serialFreePageNum = 0;
                    continue;
                }
                else
                {
                    serialFreePageNum = 0;
                    freeSize = m_FileInfo.pageDataSize;
                }

                if (handle.size <= freeSize)
                {
                    if (needPageNum > 1) 
                    {
                        freePageIndex = iLoop + 1 - serialFreePageNum;
                        quickAllocateInfo.freeBlockBeginIndexInPage = m_FileInfo.blockIndexFieldSize;
                        quickAllocateInfo.freeBlockSize = freeSize;
                    }
                    else
                    {
                        freePageIndex = iLoop;
                        quickAllocateInfo.freeBlockBeginIndexInPage = blockIndexFieldHead->maxSerialBlockIndex;
                        quickAllocateInfo.freeBlockSize = blockIndexFieldHead->maxSerialBlockSize;
                    }
                    
                }

            }

            /*not found , allocate*/
            if (quickAllocateInfo.freeBlockSize == 0)  
            {
                alloacePages(needPageNum, freePageIndex, quickAllocateInfo)
            }
            
            handle.beginIndex = sizeof(VerifyInfo_t) + sizeof(FileInfo_t) + freePageIndex * m_FileInfo.pageSize + quickAllocateInfo.freeBlockBeginIndexInPage;

            /*update index field*/
            updataIndexFieldInFileForAllocate(freePageIndex, quickAllocateInfo, handle.size);

            return true;

        }

        /**
        * @brief                    update index Field
        *
        * @param pageIndex          pageIndex
        * @param quickAllocateInfo  Allocate info
        * @param size               need size
        *
        * @return   true            success
        *           false           failed
        */
        bool updataIndexFieldInFileForAllocate(FILE_STORAGE_UNSIGNED_INT pageIndex, QuickAllocateInfo_t quickAllocateInfo, FILE_STORAGE_UNSIGNED_INT size)
        {
            FILE_STORAGE_UNSIGNED_LONG index = 0;

            FILE_STORAGE_UNSIGNED_INT allocatePageNum = 0;

            FILE_STORAGE_UNSIGNED_INT needPageNum;

            FILE_STORAGE_UNSIGNED_INT minFreeSize = 0;

            FILE_STORAGE_UNSIGNED_INT oldMinFreeSize = 0;

            QuickAllocateInfo_t tmpQuickAllocateInfo;

            BlockIndexFieldHead_t * blockIndexFieldHead =(BlockIndexFieldHead_t *)m_BlockIndex;

            index = sizeof(VerifyInfo_t) + sizeof(FileInfo_t) + pageIndex * m_FileInfo.pageSize;

            /*allocate page num*/
            allocatePageNum = quickAllocateInfo.freeBlockSize / m_FileInfo.pageDataSize;
            if (quickAllocateInfo.freeBlockSize % m_FileInfo.pageDataSize != 0)
            {
                allocatePageNum++;
            }

            /*need page num*/
            needPageNum = size / m_FileInfo.pageDataSize;
            if (size % m_FileInfo.pageDataSize != 0)
            {
                needPageNum++;
            }

            if (allocatePageNum == 1)
            {
                /*read index field from file*/
                readFromFile(index, m_BlockIndex, m_FileInfo.blockIndexFieldSize);
                oldMinFreeSize = CalculateMinAllocateSize(blockIndexFieldHead->maxSerialBlockSize);

                /*Generate index field*/
                bitSet((unsigned char *)m_BlockIndex, sizeof(BlockIndexFieldHead_t) * 8  + quickAllocateInfo.freeBlockBeginIndexInPage/m_FileInfo.blockSize, size/m_FileInfo.blockSize, 1);

                /*Generate Index head*/
                generateIndexHead((FILE_STORAGE_UNSIGNED_CHAR *)m_BlockIndex, m_FileInfo.blockIndexFieldSize);

                /*write to file*/
                writeToFile(index, m_BlockIndex, m_FileInfo.blockIndexFieldSize);

                /*update quick allocate*/
                if (define FILE_STORAGE_IS_USE_CACHE != 1)
                {
                    return;
                }
                
                /*Generate quick allocate cache*/
                tmpQuickAllocateInfo.freeBlockSize = blockIndexFieldHead->maxSerialBlockSize;
                tmpQuickAllocateInfo.freeBlockBeginIndexInPage = blockIndexFieldHead->maxSerialBlockIndex;

                minFreeSize = CalculateMinAllocateSize(blockIndexFieldHead->maxSerialBlockSize);
minFreeSize 
                if (minFreeSize != oldMinFreeSize)
                {
                    /*erase old info*/
                    m_quickAllocate[oldMinFreeSize].erase(pageIndex);
                }

                m_quickAllocate[minFreeSize][pageIndex] = tmpQuickAllocateInfo;

            }
            else
            {
                /*Set Occupy, upate to file*/
                memset(blockIndexFieldHead, 0xFF, m_FileInfo.blockIndexFieldSize);

                blockIndexFieldHead->maxSerialBlockIndex = m_FileInfo.pageSize;
                blockIndexFieldHead->maxSerialBlockSize = 0;

                for (int iLoop = 0; iLoop < needPageNum - 1; iLoop++)
                {
                    writeToFile(index, m_BlockIndex, m_FileInfo.blockIndexFieldSize) 
                    index += m_FileInfo.pageSize;
                }

                /*last page need set*/
                /*Generate index field*/
                bitSet((unsigned char *)m_BlockIndex, sizeof(BlockIndexFieldHead_t) * 8  + quickAllocateInfo.freeBlockBeginIndexInPage/m_FileInfo.blockSize, (size % m_FileInfo.pageDataSize) /m_FileInfo.blockSize, 1);

                /*Generate Index head*/
                generateIndexHead((FILE_STORAGE_UNSIGNED_CHAR *)m_BlockIndex, m_FileInfo.blockIndexFieldSize);

                /*write to file*/
                writeToFile(index, m_BlockIndex, m_FileInfo.blockIndexFieldSize);

                /*update quick allocate*/
                if (FILE_STORAGE_IS_USE_CACHE != 1)
                {
                    return;
                }
                
                /*Generate quick allocate cache*/
                tmpQuickAllocateInfo.freeBlockSize = blockIndexFieldHead->maxSerialBlockSize;
                tmpQuickAllocateInfo.freeBlockBeginIndexInPage = blockIndexFieldHead->maxSerialBlockIndex;

                minFreeSize = CalculateMinAllocateSize(blockIndexFieldHead->maxSerialBlockSize);

                m_quickAllocate[minFreeSize][pageIndex + needPageNum - 1] = tmpQuickAllocateInfo;

                /*left page need set*/
                if (allocatePageNum > needPageNum) 
                {
                    tmpQuickAllocateInfo.freeBlockSize = n_FileInfo.pageDataSize * (allocatePageNum - needPageNum);
                    tmpQuickAllocateInfo.freeBlockBeginIndexInPage = m_FileInfo.blockIndexFieldSize;

                     minFreeSize = CalculateMinAllocateSize(tmpQuickAllocateInfo.freeBlockSize);
                    m_quickAllocate[minFreeSize][pageIndex + needPageNum] = tmpQuickAllocateInfo;
                }

                /*erase old allocate info*/
                minFreeSize = CalculateMinAllocateSize(quickAllocateInfo.freeBlockSize);
                m_quickAllocate[minFreeSize].erase(pageIndex);
            }

            return true;
        }

        bool updataIndexFieldInFileForFree(UpdateIndexFieldTag_t tag, FILE_STORAGE_UNSIGNED_INT pageIndex,  const QuickAllocateInfo_t & quickAllocateInfo)
        {
        }
    
        /**
        * @brief                    allocate Page
        *
        * @param Num                Page Num
        * @param fistPageIndex      first page index
        *
        * @return                   fist Page Index
        */
        bool alloacePages(FILE_STORAGE_UNSIGNED_INT Num, FILE_STORAGE_UNSIGNED_INT & firstPageIndex, QuickAllocateInfo_t & quickAllocateInfo)
        {
            BlockIndexFieldHead_t blockIndexFieldHead = m_BlockIndex;
            if (Num == 0)
            {
                return false;
            }

            FILE_STORAGE_UNSIGNED_LONG index = 0;

            index = sizeof(VerifyInfo_t) + sizeof(FileInfo_t) + m_FileInfo.pageNum * m_FileInfo.pageSize;

            for (int iLoop = 0; iLoop < Num; iLoop++)
            {
                /*update page block index field*/
                memset(m_BlockIndex, 0x00, m_FileInfo.blockIndexFieldSize);
                blockIndexFieldHead.maxSerialBlockSize = m_FileInfo.pageDataSize;
                blockIndexFieldHead.maxSerialBlockIndex = m_FileInfo.blockIndexFieldSize;
                WriteToFile(index, &m_BlockIndex, m_FileInfo.blockIndexFieldSize);
                index += m_FileInfo.pageSize;
            }

            firstPageIndex = m_FileInfo.pageNum;

            quickAllocateInfo.freeBlockBeginIndexInPage = m_FileInfo.blockIndexFieldSize;

            quickAllocateInfo.freeBlockSize = m_FileInfo.pageDataSize * Num;

            if (FILE_STORAGE_IS_USE_CACHE == 1)
            {
                /*add to cahce*/
                FILE_STORAGE_UNSIGNED_INT minSize = 0;
                minSize = CalculateMinAllocateSize(quickAllocateInfo.freeBlockSize);
                m_quickAllocate[minSize][m_FileInfo.pageNum] = quickAllocateInfo;
            }

            /*Update File Info to File*/
            m_FileInfo.pageNum += Num;

            WriteToFile(sizeof(VerifyInfo_t), &m_FileInfo, sizeof(FileInfo_t));

            return true;
        }
        /**
        * @brief                    calculate allocate size by need size
        *
        * @param size               need size
        *
        * @return                   allocate size
        */
        FILE_STORAGE_UNSIGNED_INT calculateAllocateSizeBySize(FILE_STORAGE_UNSIGNED_INT size)
        {
            FILE_STORAGE_UNSIGNED_INT allocateSize = (size / m_FileInfo.blockSize) * m_FileInfo.blockSize;
            if (size % m_FileInfo.blockSize != 0)
            {
                allocateSize += m_FileInfo.blockSize;
            }
            return allocateSize;
        }


        /**
        * @brief            judge File Storage is init, if not init it
        *
        * @return  true     Initialize success or has intialized
        *          false    Initialize failed
        */
        bool IsInit()
        {
            /*if file is open means has been initialized*/
            if (m_Storage->IsOpen())
            {
                return true;
            }
            
            /*clear quickAllocateCache*/
            m_quickAllocate.clear();

            
            /*open storage file*/
            if (!m_Storage->open(m_FileName, 0))
            {
                return false;
            }

            /*Load storage file*/
            if (LoadStorageFile() < 0)
            {
                /*load file failed*/
                m_Storage->close();
                return false;
            }

            return true;
        }


        /**
        * @brief            Load Storage file
        *
        * @return  >  0     success load success
        *          =  0     success create new file
        *          <  0     failed errno
        */
        FILE_STORAGE_INT LoadStorageFile()
        {
            FILE_STORAGE_INT ret = 0;

            memset(m_VerifyInfo, 0x00, sizeof(VerifyInfo_t));

            FILE_STORAGE_UNSIGNED_INT maxFreeBlockBeginIndexInPage = 0;

            /*read verify info*/            
            ret = readFromFile(0, &m_VerifyInfo, sizeof(VerifyInfo_t) < 0);
            /*check verify info*/
            if ((ret <= 0) || !CheckVerifyInfo())
            {
                /*verify failed, create new file*/
                createNewStorageFile();
            }
            else
            {
                /*verify sucess anlysis file*/
                LoadQuickAllocateCache();
            }

            return true;
        }

        /**
        * @brief                read from storage file load quick allocate cache
        */
        void LoadQuickAllocateCache()
        {
            FILE_STORAGE_UNSIGNED_LONG index = 0;

            FILE_STORAGE_UNSIGNED_INT serialEmptyPageNum = 0;

            QuickAllocateInfo_t quickAllocateInfo;

            BlockIndexFieldHead_t * blockIndexFieldHead = (BlockIndexFieldHead_t *)m_BlockIndex;

            FILE_STORAGE_UNSIGNED_INT minFreeSize = 0;
            
            /*if not use quick allocate*/
            if (FILE_STORAGE_IS_USE_CACHE != 1)
            {
                return ;
            }
            
            /*read File info*/
            index += sizeof(VerifyInfo_t);

            if (readFromFile(index, &m_FileInfo, sizeof(FileInfo_t)) <= 0)
            {
                return;
            }
            index += sizeof(FileInfo_t);
            
            /*reallocate m_BlockIndex*/
            if (m_BlockIndex != NULL)
            {
                ::free(m_BlockIndex);
            }
            m_BlockIndex = ::malloc(m_FileInfo.blockIndexFieldSize);
            
            /*analysis all page's blockIndexField*/
           for (FILE_STORAGE_UNSIGNED_INT iLoop = 0; iLoop < m_FileInfo.pageNum; iLoop++)
           {
                if (ReadFromFile(index, m_BlockIndex, m_FileInfo.blockIndexFieldSize) > 0)
                {
                    if (blockIndexFieldHead->maxSerialBlockSize == m_FileInfo.pageDataSize)
                    {
                        /*serial empty page add*/
                        serialEmptyPageNum++;
                    }
                    else
                    {
                        /*serial empty page end*/
                        if (serialEmptyPageNum != 0)
                        {
                            /*exist serial empty page add to quick cache*/
                            /*calculate min free size */
                            if ((minFreeSize = CalculateMinAllocateSize(serialEmptyPageNum * m_FileInfo.pageDataSize)) > 0)
                            {
                                quickAllocateInfo.freeBlockBeginIndexInPage = m_FileInfo.blockIndexFieldSize;
                                quickAllocateInfo.freeBlockSize = m_FileInfo.pageDataSize;

                                m_quickAllocate[minFreeSize][iLoop - serialEmptyPageNum] = quickAllocateInfo;
                            }
                        }

                        /*serial empty page end*/
                        serialEmptyPageNum = 0;

                        /*is page has free block*/
                        if (blockIndexFieldHead->maxSerialBlockSize > 0)
                        {
                            /*calculate min free size */
                            if ((minFreeSize = CalculateMinAllocateSize(blockIndexFieldHead->maxSerialBlockSize)) > 0)
                            {
                                quickAllocateInfo.freeBlockBeginIndexInPage = blockIndexFieldHead->maxSerialBlockIndex;
                                quickAllocateInfo.freeBlockSize = blockIndexFieldHead->maxSerialBlockSize;
                                m_quickAllocate[minFreeSize][iLoop - serialEmptyPageNum] = quickAllocateInfo;
                            }
                        }
                    }
                }
                else
                {
                    serialEmptyPageNum++;
                }
           }

           /*serial empty page end*/
           if (serialEmptyPageNum != 0)
           {
               /*exist serial empty page add to quick cache*/
               /*calculate min free size */
               if ((minFreeSize = CalculateMinAllocateSize(serialEmptyPageNum * m_FileInfo.pageDataSize)) > 0)
               {

                    quickAllocateInfo.freeBlockBeginIndexInPage = m_FileInfo.blockIndexFieldSize;
                    quickAllocateInfo.freeBlockSize = m_FileInfo.pageDataSize;
                   m_quickAllocate[minFreeSize][iLoop - serialEmptyPageNum] = m_FileInfo.blockIndexFieldSize;
               }
           }
        }

        
        /**
        * @brief                    Create new storage file
        */
        void createNewStorageFile()
        {
            memcpy(m_VerifyInfo.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO));

            m_FileInfo.pageNum = 0;

            m_FileInfo.pageSize = FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE + (FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE - sizeof(BlockIndexFieldHead_t)) * FILE_STORAEE_BLOCK_SIZE * 8;

            m_FileInfo.blockIndexFieldSize = FILE_STORAGE_BLOCK_INDEX_FIELD_SIZE;

            m_FileInfo.pageDataSize = m_FileInfo.pageSize - m_FileInfo.blockIndexFieldSize;

            WriteToFile(0, &m_VerifyInfo, sizeof(VerifyInfo_t));

            WriteToFile(sizeof(VerifyInfo_t), &m_FileInfo, sizeof(FileInfo_t));

            if (m_BlockIndex != NULL)
            {
                ::free(m_BlockIndex);
                m_BlockIndex = NULL;
            }

            m_BlockIndex = ::malloc(m_FileInfo.blockIndexFieldSize);
        }

        /**
        * @brief                    check verify info
        *
        * @param verifyInfo         verifyInfo
        *
        * @return 
        */
        bool CheckVerifyInfo(const VerifyInfo_t * verifyInfo)
        {
            if (memcmp(verifyInfo.verifyInfo, FILE_STORAGE_VERIFY_INFO, strlen(FILE_STORAGE_VERIFY_INFO)) == 0)
            {
                return true;
            }
            return false;
        }

        /**
        * @brief                    read from storage untill len
        *
        * @param beginIndex         begin Index
        * @param data               data buffer
        * @param lenOfData          read len
        *
        * @return  > 0              success read len
        *          < 0              failed errno
        */
        FILE_STORAGE_INT readFromFile(FILE_STORAGE_UNSIGNED_LONG beginIndex, void * data, size_t lenOfData) 
        {
            FILE_STORAGE_INT ret = 0;
            size_t readLen = 0;
            
            /*judge data*/
            if (data == NULL || readLen == 0)
            {
                return -1;
            }

            /*seek to index*/
            if (m_Storage->lseek(beginIndex, SEEK_SET) < 0)
            {
                return -2;
            }

            /*read untill lenOfData*/
            while (leftLen < lenOfData)
            {
                if((ret = m_Storage->read((const void *)data + readLen, lenOfData, lenOfData - readLen)) < 0)
                {
                    return -2;
                }
                else if (ret = 0)
                {
                    return -2;
                }

                readLen += ret;
            }

            return readLen;
        }

        /**
        * @brief                write data to storage file
        *
        * @param beginIndex     begin index
        * @param data           write data buffer
        * @param lenOfData      length of write data
        *
        * @return  >= 0         success write len    
        *          <  0         failed errno
        */                                            
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

        /**
        * @brief                set buffer as bit value
        *
        * @param buf            buffer 
        * @param beginIndex     bit beginIndex
        * @param bitNum         length of bit
        * @param setValue       0 or 1
        */
        void bitSet(FILE_STORAGE_UNSIGNED_CHAR * buf, FILE_STORAGE_UNSIGNED_INT beginIndex, FILE_STORAGE_UNSIGNED_INT bitNum, FILE_STORAGE_UNSIGNED_INT setValue)
        {
            FILE_STORAGE_UNSIGNED_INT beginCharIndex = 0;
            FILE_STORAGE_UNSIGNED_INT beginCharBitIndex = 0;
            FILE_STORAGE_UNSIGNED_INT endCharIndex = 0;
            FILE_STORAGE_UNSIGNED_INT endCharBitIndex = 0;

            FILE_STORAGE_UNSIGNED_CHAR ch;
            FILE_STORAGE_UNSIGNED_CHAR tmp;


            /*begin char*/
            beginCharIndex = beginIndex / 8;
            beginCharBitIndex = beginIndex % 8;

            /*end char*/
            endCharIndex = (beginIndex +  bitNum) / 8;
            endCharBitIndex = (beginIndex + bitNum) % 8;

            /*only one char*/
            if (beginCharIndex == endCharIndex)
            {
                tmp = 0xFF;
                switch(setValue == 1)
                {
                case 0:
                    tmp = tmp >> beginCharBitIndex;
                    tmp = tmp << (8 - endCharBitIndex);
                    tmp = ~tmp;
                    buf[beginCharIndex] &= tmp;
                    break;
                case 1:
                default:
                    tmp = tmp >> beginCharBitIndex;
                    tmp = tmp << (8 - endCharBitIndex);
                    buf[beginCharIndex] |= tmp;
                    break;
                }
            }
            else
            {
                switch(setValue == 1)
                {
                case 0:
                    /*begin char*/
                    tmp = 0xFF;
                    buf[beginCharIndex] &= (tmp << (8 - beginCharBitIndex));

                    /*end char*/
                    buf[endCharIndex] &= (tmp >> endCharBitIndex);

                    /*other char*/
                    if (endCharIndex - beginIndex + 1 > 2)
                    {
                        memset(buf + beginIndex + 1, 0x00, (endCharIndex - beginIndex) - 1);
                    }
                    break;
                case 1:
                default:
                    /*begin char*/
                    tmp = 0xFF;
                    buf[beginCharIndex] |= (tmp >> beginCharBitIndex);
                    /*end char*/
                    buf[endCharIndex] |= (tmp << (8 - endCharBitIndex));

                    /*other char*/
                    if (endCharIndex - beginIndex + 1 > 2)
                    {
                        memset(buf + beginIndex + 1, 0xFF, (endCharIndex - beginIndex) - 1);
                    }
                }
            }
        }

        /**
        * @brief                    generate block index head by index field
        *
        * @param blockIndex         blokc index
        * @param lenOfBlockIndex    length of block index
        */
        void generateIndexHead(FILE_STORAGE_UNSIGNED_CHAR * blockIndex, FILE_STORAGE_UNSIGNED_INT lenOfBlockIndex)
        {

            FILE_STORAGE_UNSIGNED_INT maxSerialFreeBlockNum = 0;
            FILE_STORAGE_UNSIGNED_INT serialFreeBlockNum = 0;
            FILE_STORAGE_UNSIGNED_INT maxBitIndex = 0;
            FILE_STORAGE_UNSIGNED_CHAR ch;
            FILE_STORAGE_UNSIGNED_CHAR tmp;
            FILE_STORAGE_UNSIGNED_CHAR * indexBuf;

            BlockIndexFieldHead_t * blockIndexFieldHead = (BlockIndexFieldHead_t)blockIndex;

            /*skip head*/
            indexBuf = blockIndex + sizeof(BlockIndexFieldHead_t); 

            tmp = 0x80;

            for (FILE_STORAGE_UNSIGNED_INT iLoop = 0; iLoop < m_FileInfo.pageDataSize / m_FileInfo.blockSize / 8;iLoop++)
            {
                ch = indexBuf[iLoop];
                for (int bitLoop = 0; bitLoop < 8; bitLoop++)
                {
                    if ((ch & (tmp >> bitLoop)) != 0x00 )
                    {
                        if (serialFreeBlockNum > maxSerialFreeBlockNum)
                        {
                            /*found bigger serial free block*/
                            maxBitIndex = iLoop * 8 + bitLoop - serialFreeBlockNum;
                            maxSerialFreeBlockNum = serialFreeBlockNum;
                        }
                        serialFreeBlockNum = 0;
                    }
                    else
                    {
                        serialFreeBlockNum++;
                    }
                }
            }

            /*end check*/
            if (serialFreeBlockNum > maxSerialFreeBlockNum)
            {
                /*found bigger serial free block*/
                maxBitIndex = m_FileInfo.pageDataSize / m_FileInfo.blockSize  - serialFreeBlockNum;
                maxSerialFreeBlockNum = serialFreeBlockNum;
            }

            /*translate to page index and size*/
            blockIndexFieldHead->maxSerialBlockIndex = m_FileInfo.blockSize * maxBitIndex + sizeof(BlockIndexFieldHead_t);
            blockIndexFieldHead->maxSerialBlockSize = maxSerialFreeBlockNum * m_FileInfo.blockSize;
        }

    private:

        VerifyInfo_t m_VerifyInfo;

        FileInfo_t m_FileInfo;

        BlockIndexFieldHead_t m_BlockIndexFieldHead;

        std::string m_FileName;
        
        
        /*QuickAllocate Map : KEY-MinAllocateSize VALUE-PageAllocateInfo Map*/
        /*PageAllocateInfo Map : KEY - pageIndex VALUE-Max Allocate block beginIndex in page*/
        std::map <FILE_STORAGE_UNSIGNED_INT, std::map <FILE_STORAGE_UNSIGNED_INT, QuickAllocateInfo_t> > m_quickAllocate;

        BaseFileIO * m_Storage;

        FILE_STORAGE_UNSIGNED_CHAR * m_BlockIndex;
    }
}

