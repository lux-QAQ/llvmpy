#ifndef RUNTIME_LEVEL_H
#define RUNTIME_LEVEL_H
// RUNTIME: 运行时系统
// COMPAT: 类型兼容系统



#define RUNTIME_COMPAT_LIST_ELEMENT_CONSISTENCY_CHECK // python允许向列表中添加任何元素，所以这一选项默认是关闭的。即使开启他只是会检查列表元素并发出应该但是不会阻止这一行为。为了更强的性能可以关闭该选项。因为对于列表元素的检查是递归的。



#endif 