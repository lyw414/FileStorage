# Hash Map 
基于 FileStorage 取代原有的内存，实现建议的HASHMAP，MAP本身没有进行优化未优化点如下：
1、桶定长，未做变长处理
2、求余运算未优化
3、存储为直链，没有做红黑树优化

# Use FileStorage
构造时 指定 MAP名，MAP名与文件一一对应
1、add      插入MAP KEY-VALUE
2、find     查询MAP KEY
3、del      删除MAP KEY
4、iterator 迭代器 用于MAP遍历
（MAP的优化空间很大，暂时不做优化，针对 小存储足够使用）
