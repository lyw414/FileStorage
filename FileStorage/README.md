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
    文件按固定长度被分割，块为管理的最小粒度，如1024 或 4096 等，由连续块组成页
页通过页索引维护块信息，页所以一个或者多个块，每个块占用4字节索引信息，用于标识
块状态，目前块索引信息为引用计数与块剩余可用。
    文件开头使用固定头，占用一个或多个块，用于记录文件的基础信息，如块的大小，页
索引占用块数量，以及校验信息。

### 文件格式
    | block | block | block | block | block | block | ... |

    | FileHead (1 or more block) | page (1 or more block) | page (1 or more block) | ... |
                                 /                         \
                                /          page  info       \
                               /                             \
    | page index (1 or more block) | Data ( 1 block) | Data ( 1 block) | ... |

    
    每个Data存在n字节的所有信息，放置于 page index区域中，若page index 占用M个Block 则 page block数据量为 ( m * blockSize) / n + m

    目前 M 固定为 1 （功能暂时不打算实现）

### 文件格式说明

#### FileHead
    verifyInfo:                 用于判断是否为 存储文件，目前为固定字符串
    blockSize:                  块的大小
    blockNumForEachPage:        每个页占用块的数量（通过 blockSize 计算）
    pageNum:                    页的数量
    pageSize:                   页的大小


#### Page 
##### Page Index 
    为 IndexNode 的数组, 说明如下：
    referenceIndex:             块引用计数
    leftLen:                    块剩余可用大小

    因此 页包含的块数量为 1 * blockSize / sizeof(IndexNode), IndexNode 数组的下标与Data对应。

##### Data
    数据存储区：每个数据存储区域最大size为blockSize，IndexNode为数据存储块的索引内容，索引内容存储
于PageIndex数组中，每个分配数据包含固定头DataNode，用于标识数据区域长度，以及下一个数据块开始索引
    (当存储数据大于 blockSize 时，会存储于多个Data中，使用DataNode关联)

### 策略说明

#### 存储句柄
    文件中索引，指向DataNode，通过DataNode读取数据
   
#### 缓存管理

##### 快速分配器 
    缓存索引信息，创建缓存MAP，键值分别为 leftLen 与 BlockIndex, 指向同一资源；通过leftLen Map进行数据分配
    若不创建快速分配器，则每次读取PageIndex完成资源分配工作
#### 分配策略
    若分配数据不足 blockSize ： 查找是否存在存在足够空间的Data区域，存在则分配，不存在则从新开一页；对应的Data
索引维护剩余长度，以及引用计数
    若分配数据长度超过 blockSize ：将数据存储于多个block中，len % blockSize 按 分配数据不足blockSize存储，Data索引维护剩余长度以及引用计数，DataNode 维护多个数据区域（该情况一定为多个完整block + 一个不部分使用的block，以提供数据读取效率）

#### 回收策略
    使用存储句柄进行数据释放，针对数据长度是否超过blockSize进行维护（多个块与1个块），仅维护引用计数，当且仅当引用计数为 0 时，将leftLen修改为blockSize

#### 管理
    对数据仅完成block全部释放后，才可将回收资源再次用于分配。
