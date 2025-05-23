# llvmpy v3.0
中文 | [English](./README_EN.md)

# 简介
**这是一个`Python`的前端编译器，主要用于把`Python`代码编译成`LLVM`的`IR`代码，最后生成可执行文件的过程由`Clang`来完成。**


目前来说这仅仅只是一个玩具的地步，因为要实现真正的编译器不应该一上来就开始搓代码。而是应该先了解`Python`的各种语法和特性之后，先确定下来整体架构在开始编写代码。但是我却逆其道而行之，只是按照自己的印象来实现一个简易的`Python`编译器，估计连`Python`的子集都算不上，因为有很多功能确实和原版`Python`有出入。我不知道我的兴趣还能够支撑这个项目多久。

---
## 功能介绍
> 目前该项目完全是按照`example.py`中所需要的功能来实现的，其他的功能暂时没有实现。所以其实暂时没有什么介绍的必要性
### 数据类型
- [x] 高精度计算(GMP默认256精度):
  - [x] 支持`float`浮点数
  - [x] 支持`int`整数
  - [x] 支持`bool`布尔值
  - [ ] 支持`complex`复数(暂无计划)

### 特殊类型
- [x] 支持`None`空值
- [x] 支持`Any`任意类型
- [x] 支持函数对象
- [ ] 自定义类对象
- [ ] 支持`Ellipsis`省略号 (暂无计划)

### 容器
- [x] 支持`string`字符串
  - [x] 支持转义字符
  - [x] 支持字符串拼接
  - [ ] `f-string` (暂无计划)
- [x] 支持`list`列表
  - [x] 支持多维列表 ,支持混合类型
- [x] 支持`dict`字典
  - [x] 支持混合类型的 **key-value**(RunTime中支持hash的都可以**key-value**)
- [ ] 支持`set`集合(暂无计划)
- [ ] 支持`tuple`元组(暂无计划)
---

### 语法
- [x] 支持复杂的**加减乘除乘法**运算符
  - [x] 支撑`string`字符串拼接
  - [x] 支持`list`的拼接(`[1,2] + [3,4]*2`)
- [x] 支持大多数运算符:
  - [x] 支持`==` `!=` `>` `<` `>=` `<=`
  - [ ] 支持逻辑短路
  - [ ] 支持`is` `in` `not in` `is not`
  - [ ] 支持`&` `|` `^` `<<` `>>` (Lexer和RunTime支持了但是,Parser和CodeGen没有)
  - [ ] 支持容器对象的表比较
- [x] 支持`if-elif-else`语句
- [x] 支持`def`函数定义 :
  - [x] 支持泛型
  - [x] 支持函数对象
  - [x] 支撑函数内定义函数
- [ ] 支持`lambda`表达式
- [ ] 支持列表推导式


- [x] 支持`while-else`语句
- [x] 支持`for-else`语句
- [ ] 支持`try-except`语句
- [ ] 支持`with`语句

- [ ] 支持`class` 

- [ ] 支持`async` `await`语句(暂无计划)



## 多文件
- [x] 目前只支持单文件main()作为入口点,全局执行也可以
- [ ] 支持多文件编译
- [ ] 支持`import` `from-import` `import as` `from-import as`语句
- [ ] 支持`__name__` `__main__`语句 (暂无计划)
- [ ] 支持`__all__` (暂无计划)
- [ ] 支持`__init__.py`
---
## 测试案例
详细的功能测试请参考`tests_auto/`中的测试用例.     
`alltest.sh`是一个自动化测试脚本,可以自动化运行所有的测试用例:
- `tests_auto/checkneeds` : 检查return 0\return 1\if-else之类最基础功能的是否正常因为他们是接下来其他测试的依赖.他们是特殊的,所以这下面的测试用例需要同名的`*.sh`脚本特判
- `tests_auto/` : 测试复杂功能,通过自我的`if-else-if`语句来实现,所以需要`tests_auto/checkneeds`的支持
## 简单使用
`compile.sh`是一个简单的编译脚本,可以直接编译`./test.py`文件,并且运行

## 关于性能
- 调用`GMP`的`mpz`来实现高精度计算,但是性能上来说还是有点差的,因为`mpz`的性能比`int`要差很多
- `LLVM`的`IR`代码生成器是一个简单的实现,所以性能上来说还是有点差的,因为没有做很多的优化
- 使用了`libffi`来实现未知签名调用,比原生的`C函数`调用要慢1.5倍左右
- 静态分析做的特别差

## License
![License: CC BY-NC-SA 4.0](./assets/images/by-nc-sa.svg)

本项目采用  
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-nc-sa/4.0/)  
进行许可。
