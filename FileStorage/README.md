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

# 实现原理
连续块 ：  BlockInfo + Data | BlockInfo + Data | ....
读取时会将BlokcInfo加载至内存中，用于块的合并、检索等操作
（后续会使用bitMap进行文件块索引，移除内存，大幅提升性能）



