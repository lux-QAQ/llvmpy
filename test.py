def test(a,b):
    print(a)
    print(b)
    return a
def test2():
    return [999]
def main():
    b=test([1,2],2) # 多传参，函数类型不固定
    a=test(1.1,[1,2,3])# 多传参，函数类型不固定
    b2=test2()# 列表返回
    print(b2)
    print(a)
    print(b)
    print(b2)
    return 0