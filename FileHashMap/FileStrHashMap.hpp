#include <stdio.h>
#include <stdlib.h>
#include "FileStorage_4.hpp"
#define FileHashMapVerifyInfo "FileHashMap"

namespace LYW_CODE
{
    class FileHashMap
    {
    private:
        typedef struct _MapInfo
        {
            char verifyInfo[16];
            FileStorageHandle bucket; //type array
            unsigned int bucketSize;
            unsigned long size;
        } MapInfo_t;

        typedef struct _Data
        {
            unsigned int len;
            unsigned char data[0];
        } Data_t;

        typedef struct _DataNode
        {
            /*storage mode LV*/
            FileStorageHandle key; //type - Data_t *
            FileStorageHandle value; //type - Data_t *
            FileStorageHandle nextBlock; //type - DataNode_t *
        } DataNode_t;

    public:
        class iterator
        {

        friend FileHashMap;
        public:
            void * key;
            unsigned int keyLen;

            void * value;
            unsigned int valueLen;

        public:
            iterator()
            {
                key = NULL;
                keyLen = 0;

                value = NULL;
                valueLen = 0;
                m_Map = NULL;
            }

            iterator & operator ++ (int)
            {
                if (m_Map != NULL)
                {
                    m_NextBlock = m_Map->readBlock( m_NextBlock, &key, keyLen, &value, valueLen);
                }
                return *this;
            }

            bool operator != ( const iterator & it)
            {
                if ((it.key != key || it.value != value))
                {
                    return true;
                }
                return false ;
            }
        private:
            void setMap (FileHashMap * map)
            {
                m_Map = map;
            }

            void setNextBlock(FileStorageHandle nextBlock)
            {
                m_NextBlock = nextBlock;
            }
            
        private:
            FileHashMap * m_Map;
            FileStorageHandle m_NextBlock;
        };
        FileHashMap(const std::string & mapName) : m_FileStorage (mapName)
        {
            m_Bucket = NULL;

            if (!LoadMap())
            {
                CreateMap();
            }
        }

        ~FileHashMap()
        {
            if (m_Bucket != NULL)
            {
                ::free(m_Bucket);
                m_Bucket = NULL;
            }

            if (m_Iterator.key != NULL)
            {
                ::free(m_Iterator.key);
                m_Iterator.key = NULL;
            }

            if (m_Iterator.value != NULL)
            {
                ::free(m_Iterator.value );
                m_Iterator.value = NULL;
            }
        }
        
        bool add(void * key, unsigned int lenOfKey, void * value, unsigned int lenOfValue)
        {
            unsigned int index = APHash ((char *)key, lenOfKey);
            FileStorageHandle nodeHandle = 0;
            nodeHandle = m_Bucket[index];
            Data_t * keyData = NULL;
            unsigned int keyDataSize = 0;
            Data_t valueData;
            unsigned int valueDataSize = 0;
            unsigned int tmpLen;
            DataNode_t dataNode;

            FileStorageHandle newNodeHandle = 0;
            DataNode_t newDataNode;


            valueDataSize = sizeof(Data_t) + lenOfValue;

            /*Found in List*/
            while(nodeHandle != 0)
            {
                if (m_FileStorage.read(nodeHandle, &dataNode, sizeof(DataNode_t)) <= 0)
                {
                    return false;
                }

                /*cmpkey*/
                /*read key*/
                tmpLen = m_FileStorage.size(dataNode.key);
                if (tmpLen > keyDataSize)
                {
                    keyDataSize = tmpLen;
                    if (keyData != NULL)
                    {
                        ::free(keyData);
                    }
                    keyData = (Data_t *)::malloc(tmpLen);
                }

                if (m_FileStorage.read(dataNode.key, keyData, keyDataSize) <= 0)
                {
                    return false;
                }

                /*compare key*/
                if (keyData->len == lenOfKey && memcmp(keyData->data, key, lenOfKey) == 0)
                {
                    /*free value*/
                    m_FileStorage.free(dataNode.value);

                    /*update value*/
                    dataNode.value = m_FileStorage.allocate(valueDataSize);
                    m_FileStorage.write(dataNode.value, 0, &lenOfValue, sizeof(valueData.len));
                    m_FileStorage.write(dataNode.value, sizeof(valueData.len), value, lenOfValue);
                    m_FileStorage.write(nodeHandle, &dataNode, sizeof(DataNode_t));
                    return true;
                }

                if (dataNode.nextBlock != 0 )
                {
                    nodeHandle = dataNode.nextBlock;
                }
                else
                {
                    break;
                }
            }


            if (keyData != NULL)
            {
                ::free(keyData);
            }

            /*storage new Data node*/
            newNodeHandle = m_FileStorage.allocate(sizeof(DataNode_t));
            
            newDataNode.key = m_FileStorage.allocate(sizeof(Data_t) + lenOfKey);
            m_FileStorage.write(newDataNode.key, 0, &lenOfKey, sizeof(valueData.len));
            m_FileStorage.write(newDataNode.key, sizeof(valueData.len), key, lenOfKey);

            newDataNode.value = m_FileStorage.allocate(valueDataSize);
            m_FileStorage.write(newDataNode.value, 0, &lenOfValue, sizeof(valueData.len));
            m_FileStorage.write(newDataNode.value, sizeof(valueData.len), value, lenOfValue);

            newDataNode.nextBlock= 0;

            m_FileStorage.write(newNodeHandle, &newDataNode, sizeof(DataNode_t));

            if (m_Bucket[index] == 0)
            {
                m_Bucket[index] = newNodeHandle;
                m_FileStorage.write(m_MapInfo.bucket, m_Bucket, sizeof(FileStorageHandle) * m_MapInfo.bucketSize);
            }
            else
            {
                dataNode.nextBlock = newNodeHandle;
                m_FileStorage.write(nodeHandle, &dataNode, sizeof(DataNode_t));
            }
            
            m_MapInfo.size++;
            //m_FileStorage.write(FIRSTFILESTORAGEBLOCKHANDLE, &m_MapInfo, sizeof(MapInfo_t));
            m_FileStorage.write(m_MapHandle, &m_MapInfo, sizeof(MapInfo_t));
            return  true;
        }



        unsigned int find ( void * key, unsigned int lenOfKey, void * value, unsigned int sizeOfValue)
        {
            unsigned int index = APHash ((char *)key, lenOfKey);
            FileStorageHandle nodeHandle = 0;
            DataNode_t dataNode;
            Data_t * keyData = NULL;
            unsigned int keyDataSize = 0;

            Data_t valueData;

            unsigned int tmpLen;
             
            if ( m_Bucket[index] == 0) 
            {
                return 0;
            }

            nodeHandle = m_Bucket[index];
            while (nodeHandle != 0)
            {
                if ( m_FileStorage.read(nodeHandle, &dataNode, sizeof(DataNode_t)) <= 0) 
                {
                    return 0;
                }

                tmpLen =  m_FileStorage.size(dataNode.key);

                if (tmpLen > keyDataSize)
                {
                    keyDataSize = tmpLen;
                    if (keyData != NULL)
                    {
                        ::free(keyData);
                    }
                    keyData = (Data_t *)::malloc(keyDataSize);
                }

                m_FileStorage.read(dataNode.key, keyData, keyDataSize);
                if (keyData->len == lenOfKey && memcmp(key,keyData->data, lenOfKey) == 0)
                {
                    m_FileStorage.read(dataNode.value, 0, &valueData, sizeof(valueData.len));
                    if (value != NULL)
                    {
                        tmpLen = valueData.len < sizeOfValue ? valueData.len : sizeOfValue;
                        m_FileStorage.read(dataNode.value, sizeof(valueData.len), value, tmpLen);
                    }
                    else
                    {
                        tmpLen = valueData.len;
                    }
                    if (keyData != NULL)
                    {
                        ::free(keyData);
                    }
                    return tmpLen;
                }
                nodeHandle = dataNode.nextBlock;
            }

            if (keyData != NULL)
            {
                ::free(keyData);
            }
 
            return 0;
        }


        bool del (void * key, unsigned int lenOfKey)
        {

            unsigned int index = APHash ((char *)key, lenOfKey);
            FileStorageHandle crtNodeHandle = 0;
            FileStorageHandle preNodeHandle = 0;
            DataNode_t crtDataNode;
            DataNode_t preDataNode;

            Data_t * keyData = NULL;
            unsigned int keyDataSize = 0;
            Data_t valueData;
            unsigned int tmpLen;

            if ( m_Bucket[index] == 0) 
            {
                return true;
            }

            crtNodeHandle = m_Bucket[index];

            while (crtNodeHandle != 0)
            {
                if ( m_FileStorage.read(crtNodeHandle, &crtDataNode, sizeof(DataNode_t)) <= 0) 
                {
                    return false;
                }

                tmpLen =  m_FileStorage.size(crtDataNode.key);

                if (tmpLen > keyDataSize)
                {
                    keyDataSize = tmpLen;
                    if (keyData != NULL)
                    {
                        ::free(keyData);
                    }
                    keyData = (Data_t *)::malloc(keyDataSize);
                }

                m_FileStorage.read(crtDataNode.key, keyData, keyDataSize);
                if (keyData->len == lenOfKey && memcmp(key,keyData->data, lenOfKey) == 0)
                {

                    /*list delete node*/
                    if (preNodeHandle != 0)
                    {
                        preDataNode.nextBlock = crtDataNode.nextBlock;
                        m_FileStorage.write(preNodeHandle, &preDataNode, sizeof(DataNode_t));
                        m_FileStorage.free(crtDataNode.key);
                        m_FileStorage.free(crtDataNode.value);
                        m_FileStorage.free(crtNodeHandle);

                    }
                    else
                    {

                        m_FileStorage.free(crtDataNode.key);
                        m_FileStorage.free(crtDataNode.value);
                        m_FileStorage.free(crtNodeHandle);
                        m_Bucket[index] = crtDataNode.nextBlock;
                        m_FileStorage.write(m_MapInfo.bucket, m_Bucket, sizeof(FileStorageHandle) * m_MapInfo.bucketSize);
                    }

                    if (keyData != NULL)
                    {
                        ::free(keyData);
                    }

                    m_MapInfo.size--;
                    //m_FileStorage.write(FIRSTFILESTORAGEBLOCKHANDLE, &m_MapInfo, sizeof(MapInfo_t));
                    m_FileStorage.write(m_MapHandle, &m_MapInfo, sizeof(MapInfo_t));
                    return true;
                }

                /*save crtDataNode to preDataNode*/
                memcpy(&preDataNode, &crtDataNode, sizeof(DataNode_t));
                preNodeHandle = crtNodeHandle;
                crtNodeHandle = crtDataNode.nextBlock;
            }

            if (keyData != NULL)
            {
                ::free(keyData);
            }
            return true;
        }


        unsigned long size()
        {
            return m_MapInfo.size;
        }

    public:

        iterator & begin()
        {
            FileStorageHandle nextDataNodeHandle = 0;
            m_Iterator.setMap(this);
            if (m_Iterator.key != NULL )
            {
                ::free(m_Iterator.key);
                m_Iterator.key = NULL;
            }

            if (m_Iterator.value != NULL )
            {
                ::free(m_Iterator.value);
                m_Iterator.value = NULL;
            }

            if (size() == 0)
            {
                m_Iterator.setNextBlock(nextDataNodeHandle);
                return m_Iterator;
            }

            /*find begin block handle*/
            for (int iLoop = 0; iLoop < m_MapInfo.bucketSize; iLoop++)
            {
                if (m_Bucket[iLoop] != 0)
                {
                    /*read first block*/
                    nextDataNodeHandle = readBlock(m_Bucket[iLoop], &m_Iterator.key, m_Iterator.keyLen, &m_Iterator.value, m_Iterator.valueLen) ;
                    m_Iterator.setNextBlock(nextDataNodeHandle);
                    break;
                }
            }
            return m_Iterator;
        }

        iterator end()
        {
            return iterator();
        }

        FileStorageHandle readBlock(FileStorageHandle dataNodeHandle , void ** key, unsigned int & keyLen, void ** value, unsigned int & valueLen)
        {
            DataNode_t dataNode;
            Data_t tmpData;

            if ((void *)(*key) != NULL )
            {
                ::free((*key));
                m_Iterator.key = *key = NULL;
            }

            if ((void *)(*value) != NULL )
            {
                ::free((*value));
                m_Iterator.value =  *value = NULL;
            }

            if (dataNodeHandle == 0)
            {
                return 0;
            }

            m_FileStorage.read(dataNodeHandle, &dataNode, sizeof(DataNode_t));

            /*read key*/
            m_FileStorage.read(dataNode.key, 0, &keyLen,sizeof(tmpData.len));

            m_Iterator.key = *key = ::malloc(keyLen);

            m_FileStorage.read(dataNode.key, sizeof(tmpData.len), *key, keyLen);

            /*read value*/
            m_FileStorage.read(dataNode.value , 0, &valueLen,sizeof(tmpData.len));

            m_Iterator.value = *value = ::malloc(valueLen);

            m_FileStorage.read(dataNode.value, sizeof(tmpData.len), *value, valueLen);

            if (dataNode.nextBlock == 0)
            {
                unsigned int index = APHash((char *)(*key), keyLen);
                for (int iLoop = index + 1; iLoop < m_MapInfo.bucketSize; iLoop++)
                {
                    if (m_Bucket[iLoop] != 0)
                    {
                        return m_Bucket[iLoop];
                    }
                }
            }
            return dataNode.nextBlock;
        }
        
    private:
        bool LoadMap()
        {
            memset(&m_MapInfo, 0x00, sizeof(MapInfo_t));
            m_MapHandle = m_FileStorage.getUserHandle(0);
            if (m_MapHandle == 0) 
            {
                return false;
            }

            //if (m_FileStorage.read(FIRSTFILESTORAGEBLOCKHANDLE, &m_MapInfo,sizeof(m_MapInfo)) > 0)
            if (m_FileStorage.read(m_MapHandle, &m_MapInfo,sizeof(m_MapInfo)) > 0)
            {
                /*check Map VerifyInfo*/
                if (memcmp(m_MapInfo.verifyInfo, FileHashMapVerifyInfo, strlen(FileHashMapVerifyInfo)) == 0)
                {
                    /*allocate bucket buffer*/
                    m_Bucket = (FileStorageHandle *)::malloc(sizeof(FileStorageHandle) * m_MapInfo.bucketSize);

                    /*load bucket info*/
                    if (m_FileStorage.read(m_MapInfo.bucket, m_Bucket, sizeof(FileStorageHandle) * m_MapInfo.bucketSize) <= 0)
                    {
                        ::free(m_Bucket);
                        m_Bucket = NULL;
                        return false;
                    }
                    return true;
                }
            }
            return false;
        }

        bool CreateMap()
        {
            /*clear storage File*/
            m_FileStorage.clearFile();
            
            /*Init Hash Map*/
            memcpy(m_MapInfo.verifyInfo, FileHashMapVerifyInfo, strlen(FileHashMapVerifyInfo));
            m_MapInfo.bucketSize = 1024;
            m_MapInfo.bucket = 0;
            m_MapInfo.size = 0;
            
            /*allocate Map Info storage buffer , must be first block*/

            m_MapHandle = m_FileStorage.allocate(sizeof(MapInfo_t));

            m_FileStorage.setUserHandle(0, m_MapHandle);

            //if (m_FileStorage.allocate(sizeof(MapInfo_t)) != FIRSTFILESTORAGEBLOCKHANDLE)
            //{
            //    m_FileStorage.clearFile();
            //    return false;
            //}

            /*allcote bucket storage buffer*/
            if ((m_MapInfo.bucket = m_FileStorage.allocate(sizeof(FileStorageHandle) * m_MapInfo.bucketSize)) <= 0)
            {
                m_FileStorage.clearFile();
                return false;
            }

            m_FileStorage.fset(m_MapInfo.bucket, 0x00);

            /*write to storage buffer*/
            //m_FileStorage.write(FIRSTFILESTORAGEBLOCKHANDLE, &m_MapInfo, sizeof(MapInfo_t));
            m_FileStorage.write(m_MapHandle, &m_MapInfo, sizeof(MapInfo_t));

            /*cache bucket info*/
            m_Bucket = (FileStorageHandle *)::malloc(sizeof(FileStorageHandle) * m_MapInfo.bucketSize);
            ::memset(m_Bucket, 0x00, sizeof(FileStorageHandle) * m_MapInfo.bucketSize);
            return true;
        }

        unsigned int APHash ( const char *str, int len )
        {
            unsigned int hash = 0;
            int i;
            for (i=0; i < len; i++)
            {
                if ((i & 1) == 0)
                {
                    hash ^= ((hash << 7) ^ (*str++) ^ (hash >> 3));
                }
                else
                {
                    hash ^= (~((hash << 11) ^ (*str++) ^ (hash >> 5)));
                }
            }
            return hash % m_MapInfo.bucketSize;
        }

    private:
        LYW_CODE::FileStorage<> m_FileStorage;
        FileStorageHandle * m_Bucket;
        std::string m_MapName;
        MapInfo_t m_MapInfo;
        iterator m_Iterator;

        FileStorageHandle m_MapHandle;
    };
}
