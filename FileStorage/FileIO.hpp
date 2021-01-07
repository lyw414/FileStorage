#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <string>

namespace LYW_CODE
{
    class BaseFileIO
    {
        public:

            virtual bool IsOpen() = 0;
            
            /**
             * @brief  open file
             *
             * @param Mode      0 append 1 clear and open 
             * @param fileName  file Name  
             *
             * @return 
             */
            virtual int open(const std::string & fileName, int Mode) = 0;

            /**
             * @brief  lseek
             *
             * @param offset    offset
             * @param whence    whence
             *
             * @return  -1 failed 
             *          the resulting offset location as measured in bytes from the beginning of the file 
             */
            virtual long long lseek(unsigned long long offset, int whence) = 0;


            /**
             * @brief           read
             *
             * @param buf       read buf
             * @param sizeOfBuf size of buf
             * @param count     read count
             *
             * @return -1 failed
             *         >0 read date len
             */
            virtual int read(void *buf, size_t sizeOfBuf, size_t count) = 0;

            /**
             * @brief 
             *
             * @param buf       write buf
             * @param count     write data count
             *
             * @return  <0      failed
             *          >0      write data len
             */
            virtual int write(const void *buf, size_t count) = 0;

    
            /**
             * @brief close file handle
             */
            virtual void close() = 0;
    

            /**
             * @brief  get file size
             *
             * @return -1 failed
             *         >0 file size
             */
            virtual int size() = 0;

            
            /**
             * @brief  truncate file 
             *
             * @param len offset len
             *
             * @return    0   success
             *          < 0   failed
             */
            virtual int ftruncate(off_t len) = 0;

            virtual ~BaseFileIO() {};
    };


    class FileIO : public BaseFileIO
    {
        public:
            FileIO()
            {
                m_handle = -1;
            }

            virtual ~FileIO()
            {
                if (m_handle > 0)
                {
                    ::close(m_handle);
                    m_handle = -1;
                }
            }

            bool IsOpen()
            {
                if (m_handle > 0)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            

            /**
             * @brief  open file
             *
             * @param Mode      0:open 1:clear and open 
             *
             * @parma fileName  file Name 
             *
             * @return  -1 open failed 
             *           1 open OK 
             */
            int open(const std::string & fileName, int Mode) 
            {
                if (fileName == "")
                {
                    return -1;
                }

                m_FileName = fileName;
                switch (Mode)
                {
                    case 0:
                        if ((m_handle = ::open(m_FileName.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) < 0)
                        {
                            m_handle = -1;
                            return -1;
                        }
                        break;
                    case 1:
                    default :

                        if ((m_handle = ::open(m_FileName.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0)
                        {
                            m_handle = -1;
                            return -1;
                        }
                }
                return 1;
            }


            /**
             * @brief  lseek
             *
             * @param offset    offset
             * @param whence    whence
             *
             * @return  -1 failed 
             *          the resulting offset location as measured in bytes from the beginning of the file 
             */
            long long lseek(unsigned long long offset, int whence)
            {
                if (m_handle > 0)
                {
                    return ::lseek(m_handle, offset, whence);
                }
                else
                {
                    return -1;
                }
            }


            /**
             * @brief           read
             *
             * @param buf       read buf
             * @param sizeOfBuf size of buf
             * @param count     read count
             *
             * @return -1 failed
             *         >0 read date len
             */
            int read(void *buf, size_t sizeOfBuf, size_t count)
            {
                if (m_handle < 0 || buf == NULL || sizeOfBuf < count)
                {
                    return -1;
                }
                return ::read(m_handle, buf, count);
            }

            /**
             * @brief 
             *
             * @param buf       write buf
             * @param count     write data count
             *
             * @return  <0      failed
             *          >0      write data len
             */
            int write(const void *buf, size_t count)
            {
                if (buf == NULL || m_handle < 0)
                {
                    return -1;
                }

                return ::write(m_handle, buf, count);
            }

    
            /**
             * @brief close file handle
             */
            void close()
            {
                if (m_handle > 0)
                {
                    ::close(m_handle);
                    m_handle = -1;
                }
            }

            /**
             * @brief  get file size
             *
             * @return -1 failed
             *         >0 file size
             */
            int size()
            {
                if (m_handle > 0)
                {
                    ::lseek(m_handle, 0, SEEK_SET);
                    return ::lseek(m_handle, 0, SEEK_END);
                }
                else
                {
                    return -1;
                }
            }

            /**
             * @brief       truncate file
             *
             * @param len   offset len
             *
             * @return      0   success
             *            < 0   failed
             */
            int ftruncate(off_t len)
            {
                if (m_handle > 0)
                {
                    return ::ftruncate(m_handle, len);
                }
                else
                {
                    return -1;
                }
            }


        private:
            std::string m_FileName;
            int m_handle;
    };
}
