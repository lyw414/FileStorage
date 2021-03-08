# FileIO
文件IO的基类，针对非标准IO的环境需要自行实现IO操作，IO操作为常规的文件读写、seek、truncate、open、close

# FileStorage
模板对象 需指定 FileIO 类型(基类为 BaseFileIO)

1、allocate 分配一块文件空间用于存储,返回块标识

2、write    写入一块文件空间，基于块标识，支持偏移量

3、read     读取一块文件空间，基于块标识，支持偏移量

4、fset     初始化一块文件空间为特定字符，基于块标识

5、Free     释放一块文件空间，基于块标识

6、size     查询一块已分配文件空间的大小，基于块标识

## 实现原理
    文件按固定长度被分割，块为管理的最小粒度，如1024 或 4096 等；
每个块包含BlockInfo，用于维护块状态，且块为双链表节点，节点信息
也存储在BlockInfo中

### 文件格式
| FixedHead (block) | block | block | block | block | block | ... |

                                    /       \
                                   /         \
                                  /  block    \
                             | Block Info | Data | 

    每个Block为双链表节点，节点信息存储于Block Info中，数据存储于Data区域中        

### 文件格式说明

#### FileHead
    verifyInfo:                 用于判断是否为 存储文件，目前为固定字符串
    blockSize:                  块的大小
    userHandle:                 开发给用户存储句柄
    usedBlockEnd                占用块链表结尾节点索引
    freeBlockBegin:             空闲块链表开始节点索引
#### Block
##### Block Info
    preBlock                    前节点
    nextBlock                   后节点
    leftLen                     剩余可分配长度
    referenceNum                引用计数

##### Block Data
    数据顺序存储，若剩余长度不足，则新增尾点，继续连续存储

### 策略说明

#### 存储句柄
    文件中索引，指向DataNode，通过DataNode读取数据
    DataNode                    记录数据长度
   
#### 缓存管理
   暂时未实现,后续会增加,用于减少分配、释放、数据读写的IO次数

#### 分配策略
    查找空闲链，或文件为添加一个块用，添加至占用块链表尾

#### 回收策略
    块的引用计数归零后，将其加入空闲队列
