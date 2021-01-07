# FileIO
文件IO的基类，针对非标准IO的环境需要自行实现IO操作，IO操作为常规的文件读写、seek、truncate、open、close

# FileStorage
模板对象 需指定 FileIO 类型(基类为 BaseFileIO)

1、allocate 分配一块文件空间用于存储,返回块标识

2、write    写入一块文件空间，基于块标识

3、read     读取一块文件空间，基于块标识

4、fset     初始化一块文件空间为特定字符，基于块标识

5、Free     释放一块文件空间，基于块标识

6、size     查询一块已分配文件空间的大小，基于块标识

## 实现原理
连续块 ：  BlockInfo + Data | BlockInfo + Data | ....
读取时会将BlokcInfo加载至内存中，用于块的合并、检索等操作
（后续会使用bitMap进行文件块索引，移除内存，提升加载速度）


### 文件格式

| Verify Info | File Info | Page | Page | .....
                         /         \
                     / block structure \
| Block Index | Block | Block | Block | Block | .... 

### 文件格式说明

#### Verify Info
    文件校验头，用于校验是否为FileStorage文件，目前为固定字符校验，大小为 16 byte

#### File Info
    文件信息：维护 当前文件 Block 数量，Block 的大小等文件整体信息，结构体

#### Page
    存储块：大小固定的存储块，头为定长的块索引区，其余部分数据区，索引用于表示Page状态

#### Block Index
    Block 存储块开头分配空间，大小为  Block Size * Page Num / 8 / Block Size，即使用 1 bit 标识Block状态
    0 : Page 空闲
    1 : Page 占用

#### Block
    最小分配颗粒度，仅分配一个或多个连续Block

### 策略说明

#### 存储句柄
    通过存储分配接口，分配成功后返回用户，用户可以使用该句柄对分配块进行 读、写、释放操作
    typedef strcut _FileStorageHandle
    {
        unsigned long long index;  //文件偏移量
        unsigned int len; // 分配到的空间大小
    } FileStorageHandle;
    
#### 缓存管理

##### 快速分配器 
    第一打开存储文件，通过解析 Block Index 生成 快速分配器
    std::vector <std::list <BlockInfo>> 

    数组 index : 链表中存储块 最大可分配的块 至少为  2 ^ (index + a) , 至多为  2 ^ (index + 1 + a)
    
    （需要分配空间为 59 则将 59 转为 index； 若 a = 2;  index = 4 时 Block至少可以分配 64 字节， 因此在 index >= 4 中找到一个Block 通过解析索引 将空间分配，并从新计算 至少可分配，并移到对应数组的链表中）
    
    缓存分配占用内存与空闲块数量直接相关，若不使用快速分配，则通过遍历Block Index获取可分配块

#### 分配策略
    1. 不足 Block Size, 则分配一个 Block
    2. 其余情况通过快速分配器 查找合适；若不使用快速分配器 则遍历 Block Index； 找到合适大小的空间进行分配；若无合适空间分配 则重新申请 Block

#### 释放策略
    1. 通过解析 FileStorageHandle 换算到 Page 对应的Block Index，维护Block Index；若使用快速分配器则将检查前后是否为空闲块，并进行维护，通过对前后空闲块Size的解析 在对用数组中，找到 Block 并调整长度，调整至合适的地方。


