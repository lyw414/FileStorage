#include <stdio.h>
#include <stdlib.h>
#include "FileStorage_4.hpp"
#define FileListVerifyInfo "FileList"

namespace LYW_CODE
{
    class FileList
    {
    private:
        typedef struct _ListInfo
        {
            char verifyInfo[16];
            /*ListNode_t pointer*/
            FileStorageHandle beginNode; 
            FileStorageHandle endNode;
            unsigned long size;
        } ListInfo_t;

        typedef struct _Data
        {
            unsigned int len;
            unsigned char data[0];
        } Data_t;
        
        typedef struct _ListNode
        {
            FileStorageHandle data;
            FileStorageHandle nextListNode;
            FileStorageHandle beforeListNode;
        } ListNode_t;


    bool LoadList()
    {
        memset(&m_listInfo, 0x00, sizeof(ListInfo_t));
        m_listHandle = m_FileStorage.getUserHandle(0);

        if (m_listHandle == 0)
        {
            return false;
        }

        /*read Head*/
        if (m_FileStorage.read(m_listHandle, &m_listInfo, sizeof(ListInfo_t)) > 0)
        {
            /*verify info check*/
            if (memcmp(m_listInfo.verifyInfo, FileListVerifyInfo,  strlen(FileListVerifyInfo)) != 0)
            {
                return false ;
            }
        }
        return true;
    }


    bool CreateList()
    {
        m_FileStorage.clearFile();
        memset(&m_listInfo, 0x00, sizeof(ListInfo_t));
        m_listInfo.size = 0;
        m_listInfo.beginNode = 0;
        m_listInfo.endNode = 0;
        memcpy(m_listInfo.verifyInfo, FileListVerifyInfo, sizeof(FileListVerifyInfo));

        m_listHandle = m_FileStorage.allocate(sizeof(ListInfo_t));

        m_FileStorage.write(m_listHandle, &m_listInfo, sizeof(ListInfo_t));

        m_FileStorage.setUserHandle(0, m_listHandle);

        return true;
    }

    private:
        LYW_CODE::FileStorage<> m_FileStorage;
        ListInfo_t m_listInfo;
        FileStorageHandle m_listHandle;

    public:
        class iterator
        {
        friend FileList;

        private:
            ListNode_t m_currentListNode;
            FileList * m_list;


        public:

            //iterator(FileList * list, ListNode_t currentListNode)
            //{
            //    m_list = list;
            //    memcpy(&m_currentListNode, &currentListNode, sizeof(ListNode_t));
            //}

            iterator()
            {
                m_list = NULL;
                memset(&m_currentListNode, 0x00, sizeof(ListNode_t));
            }

            ~iterator()
            {
                m_list = NULL;
                memset(&m_currentListNode, 0x00, sizeof(ListNode_t));
            }

            iterator & operator = ( const iterator & it)
            {
                memcpy(&m_currentListNode, &it.m_currentListNode, sizeof(ListNode_t));
                m_list = it.m_list;
                return *this;
            }

            iterator & operator ++ (int)
            {
                if (m_currentListNode.nextListNode != 0)
                {
                    m_list->m_FileStorage.read(m_currentListNode.nextListNode, &m_currentListNode, sizeof(ListNode_t));
                }
                else
                {
                    memset(&m_currentListNode, 0x00, sizeof(ListNode_t));
                }
                return *this;
            }

            //iterator & operator -- (int)
            //{
            //    if (m_currentListNode.beforeListNode != 0)
            //    {
            //        m_list->m_FileStorage.read(m_currentListNode.beforeListNode, &m_currentListNode, sizeof(ListNode_t));
            //    }
            //    else
            //    {
            //        memset(&m_currentListNode, 0x00, sizeof(ListNode_t));
            //    }
            //    return *this;
            //}


            bool operator != ( const iterator & it)
            {
                if (m_list != it.m_list || memcmp(&m_currentListNode, &it.m_currentListNode, sizeof(ListNode_t)) != 0)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }

            unsigned int Data(void * data, unsigned int sizeOfData)
            {
                Data_t dataNode;
                unsigned int readLen;
                if (m_currentListNode.data != 0)
                {
                    m_list->m_FileStorage.read(m_currentListNode.data, &dataNode, sizeof(Data_t));
                    readLen = sizeOfData < dataNode.len ? sizeOfData : dataNode.len;
                    return m_list->m_FileStorage.read(m_currentListNode.data, sizeof(Data_t),  data, readLen);
                }
                return 0;
            }

        };


    public:
        FileList(const std::string &ListName) : m_FileStorage(ListName)
        {
            if (!LoadList())
            {
                CreateList();
            }
        }


        iterator begin()
        {
            iterator it;
            memset(&it.m_currentListNode, 0x00, sizeof(ListNode_t));
            if (m_listInfo.beginNode != 0)
            {
                m_FileStorage.read(m_listInfo.beginNode, &it.m_currentListNode, sizeof(ListNode_t));
            }

            it.m_list = this;
            return it;
        }


        iterator end()
        {
            iterator it;
            memset(&it.m_currentListNode, 0x00, sizeof(ListNode_t));
            it.m_list = this;
            return it;
        }


        bool erase(const iterator & it) {return false;}

        bool insert_after(const iterator & it, void * data, unsigned int lenOfData) { return false;}

        bool insert_before(const iterator & it, void * data, unsigned int lenOfData) {return false;}


        bool push_back(void * data, unsigned int lenOfData)
        {
            ListNode_t listNode;
            Data_t dataNode;

            FileStorageHandle listNodeHandle = 0;
            FileStorageHandle dataHandle = 0;
            
            
            /*Data*/
            dataNode.len = lenOfData;
            dataHandle = m_FileStorage.allocate(sizeof(Data_t) + lenOfData);
            m_FileStorage.write(dataHandle, &dataNode, sizeof(Data_t));
            m_FileStorage.write(dataHandle, sizeof(Data_t), data, lenOfData);


            /*List Node*/
            listNode.data = dataHandle;
            listNode.nextListNode = 0;
            listNode.beforeListNode = m_listInfo.endNode;

            listNodeHandle = m_FileStorage.allocate(sizeof(ListNode_t));
            m_FileStorage.write(listNodeHandle,&listNode, sizeof(ListNode_t) );


            /*add to list*/
            if (m_listInfo.size == 0)
            {
                /*empty list*/
                m_listInfo.beginNode = listNodeHandle;
                m_listInfo.endNode = listNodeHandle;
            }
            else
            {
                /*push to end*/
                ListNode_t endListNode;
                m_FileStorage.read(m_listInfo.endNode, &endListNode, sizeof(ListNode_t));
                endListNode.nextListNode = listNodeHandle;
                m_FileStorage.write(m_listInfo.endNode, &endListNode, sizeof(ListNode_t));
                m_listInfo.endNode = listNodeHandle;
            }

            /*update list info*/
            m_listInfo.size++;
            m_FileStorage.write(m_listHandle, &m_listInfo, sizeof(ListInfo_t));
            return true;
        }

        unsigned int pop_front(void * data, unsigned int sizeOfData)
        {
            ListNode_t listNode;
            Data_t dataNode;
            unsigned int readLen = sizeOfData;
            
            if (m_listInfo.size == 0)
            {
                return 0;
            }

            /*read ListNode*/
            m_FileStorage.read(m_listInfo.beginNode, &listNode, sizeof(ListNode_t));

            /*read dataNode*/
            m_FileStorage.read(listNode.data, &dataNode, sizeof(Data_t));

            if (data != NULL)
            {
                /*read Data*/
                readLen =  dataNode.len < sizeOfData ? dataNode.len : sizeOfData;
                m_FileStorage.read(listNode.data, sizeof(Data_t), data, readLen);
            }
            
            /*pop front*/
            m_listInfo.size--;
            /*free Node*/
            m_FileStorage.free(listNode.data);
            m_FileStorage.free(m_listInfo.beginNode);
            
            /*update listInfo*/
            m_listInfo.beginNode = listNode.nextListNode;
            m_FileStorage.write(m_listHandle, &m_listInfo, sizeof(ListInfo_t));
            
            /*update nextNode's beforeListNode*/
            m_FileStorage.read( m_listInfo.beginNode, &listNode, sizeof(listNode));
            listNode.beforeListNode = 0;
            m_FileStorage.write( m_listInfo.beginNode, &listNode, sizeof(listNode));

            return readLen;
        }


        bool push_front(void * data, unsigned int lenOfData )
        {

            ListNode_t listNode;
            Data_t dataNode;

            FileStorageHandle listNodeHandle = 0;
            FileStorageHandle dataHandle = 0;
            
            
            /*Data*/
            dataNode.len = lenOfData;
            dataHandle = m_FileStorage.allocate(sizeof(Data_t) + lenOfData);
            m_FileStorage.write(dataHandle, &dataNode, sizeof(Data_t));
            m_FileStorage.write(dataHandle, sizeof(Data_t), data, lenOfData);


            /*List Node*/
            listNode.data = dataHandle;
            listNode.nextListNode = m_listInfo.beginNode;
            listNode.beforeListNode = 0;

            listNodeHandle = m_FileStorage.allocate(sizeof(ListNode_t));
            m_FileStorage.write(listNodeHandle,&listNode, sizeof(ListNode_t) );


            /*add to list*/
            if (m_listInfo.size == 0)
            {
                /*empty list*/
                m_listInfo.beginNode = listNodeHandle;
                m_listInfo.endNode = listNodeHandle;
            }
            else
            {
                /*push to front*/
                ListNode_t beginListNode;
                m_FileStorage.read(m_listInfo.beginNode, &beginListNode, sizeof(ListNode_t));
                beginListNode.beforeListNode = listNodeHandle;
                m_FileStorage.write(m_listInfo.beginNode, &beginListNode, sizeof(ListNode_t));
                m_listInfo.beginNode = listNodeHandle;
            }

            /*update list info*/
            m_listInfo.size++;
            m_FileStorage.write(m_listHandle, &m_listInfo, sizeof(ListInfo_t));
            return true;

        }

        unsigned int pop_back(void * data, unsigned int sizeOfData)
        {
            ListNode_t listNode;
            Data_t dataNode;
            unsigned int readLen = sizeOfData;
            
            if (m_listInfo.size == 0)
            {
                return 0;
            }

            /*read ListNode*/
            m_FileStorage.read(m_listInfo.endNode, &listNode, sizeof(ListNode_t));

            /*read dataNode*/
            m_FileStorage.read(listNode.data, &dataNode, sizeof(Data_t));

            if (data != NULL)
            {
                /*read Data*/
                readLen =  dataNode.len < sizeOfData ? dataNode.len : sizeOfData;
                m_FileStorage.read(listNode.data, sizeof(Data_t), data, readLen);
            }
            
            /*pop front*/
            m_listInfo.size--;
            /*free Node*/
            m_FileStorage.free(listNode.data);
            m_FileStorage.free(m_listInfo.beginNode);
            
            /*update listInfo*/
            m_listInfo.endNode = listNode.beforeListNode;
            m_FileStorage.write(m_listHandle, &m_listInfo, sizeof(ListInfo_t));
            
            /*update nextNode's beforeListNode*/
            m_FileStorage.read( m_listInfo.endNode, &listNode, sizeof(listNode));
            listNode.nextListNode = 0;
            m_FileStorage.write( m_listInfo.endNode, &listNode, sizeof(listNode));

            return readLen;
        }

        unsigned int front(void * data, unsigned int sizeOfData)
        {
            ListNode_t listNode;
            Data_t dataNode;
            unsigned int readLen = sizeOfData;
            
            if (m_listInfo.size == 0)
            {
                return 0;
            }

            if (data == NULL || sizeOfData == 0)
            {
                return 0;
            }

            /*read ListNode*/
            m_FileStorage.read(m_listInfo.beginNode, &listNode, sizeof(ListNode_t));

            /*read dataNode*/
            m_FileStorage.read(listNode.data, &dataNode, sizeof(Data_t));

            /*read Data*/
            readLen =  dataNode.len < sizeOfData ? dataNode.len : sizeOfData;
            m_FileStorage.read(listNode.data, sizeof(Data_t), data, readLen);
            
            return readLen;
        }

        unsigned int back(void * data, unsigned int sizeOfData)
        {
            ListNode_t listNode;
            Data_t dataNode;
            unsigned int readLen = sizeOfData;
            
            if (m_listInfo.size == 0)
            {
                return 0;
            }

            if (data == NULL || sizeOfData == 0)
            {
                return 0;
            }

            /*read ListNode*/
            m_FileStorage.read(m_listInfo.endNode, &listNode, sizeof(ListNode_t));

            /*read dataNode*/
            m_FileStorage.read(listNode.data, &dataNode, sizeof(Data_t));

            /*read Data*/
            readLen =  dataNode.len < sizeOfData ? dataNode.len : sizeOfData;
            m_FileStorage.read(listNode.data, sizeof(Data_t), data, readLen);
            
            return readLen;
        }

        unsigned int size()
        {
            return m_listInfo.size;
        }
    };
}
