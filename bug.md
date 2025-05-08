1. if-else-elif
``` python
    c=1
    if c <0:
        print("c小于0")
    elif c < 1:
        print("c小于1")
    elif c < 2:
        print("c小于2")
    else:
        print("c大于等于2")
    return 0
```
过程异（fixed）

2. recovered_source.py恢复字符串异常（fixed）

3. std::shared_ptr<PyType> BinaryExprAST::getType() 中说明了getCommonType可能无法处理list[int] + list[float] -> list[float]
 // 使用 ExpressionTypeInferer 进行推导（已经修改了）（ExpressionTypeInferer似乎没什么用）
    cachedType = ExpressionTypeInferer::inferBinaryExprType(opType, lhsType, rhsType); （fixed？）

4. 函数对象(fixed)
5. 函数内定义函数(fixed)
6. stmt里面的函数作用域管理这个和第五条挂钩(fixed)

7. FunctionAST* CodeGenType::getFunctionAST(const std::string& funcName)  TODO: 需要一种方法来访问当前作用域或父作用域的函数定义。(fixed?)



8. def main():
    a=2
    if a==2:
        def fun1():
            print("a is 2")
            return "a is 2"
    else :
        def fun2():
            print("a is not 2")
            return "a is not 2"
    print(fun2()) (fixed)

9. 目前还不支持and not(似乎是and不支持)
10. 逻辑运算符是一个大坑啊！！！！！
11. 容器类型目前还没有支持==类似运算符