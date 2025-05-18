# 函数对象比较
def func(a):
    return func
def func2(a):
    return func2
def testeq():
    return func==func2
def testneq():
    return func!=func2
def test1():
    if testeq():
        return False
    a= False
    if testeq:
        a= True
    return a

# 复杂链式调用

def funcfunc(a):
    print("funcfunc")
    print(a)
    return funcfunc
def returndict(a):
    return {"funcfunc":a}
def test2():
    dic = {1.2:returndict}
    a=[1,2,dic]
    c=a[2][1.2](returndict)["funcfunc"](funcfunc)["funcfunc"](1.2)(1.2)
    b= funcfunc
    if c==b:
        return True
    else:
        return False
    return False

def test3():
    fun = None
    count = 0
    def _func1():
        return 1
    def pocket(a):
        return [a]
    def numBasePocket(num,obj,pocket):
        c =  0
        while c < num:
            obj = pocket(obj)
            c += 1
        return obj
    chunk1  =[_func1, numBasePocket(5,_func1,pocket), numBasePocket(2,_func1,pocket)]
    chunk2 = [numBasePocket(2,_func1,pocket) , numBasePocket(2,chunk1,pocket)]
    chunk3 = [numBasePocket(2,chunk2,pocket), numBasePocket(2,chunk1,pocket),numBasePocket(99,chunk2,pocket),_func1]
    chunk4= [chunk1, chunk2, chunk3]
    def todo_func1(a,_func1):
        res= 0
        if a== _func1:
            return _func1()
        for i in a:
           res+= todo_func1(i,_func1)
        return res
    #print(chunk4)
    count=todo_func1(chunk4,_func1)
    if count == 19:
        return True
    else:
        return False
    return False

def main():
    allpass=True
    if test1():
        print("test1 pass")
    else:
        print("test1 fail")
        allpass=False
    
    if test2():
        print("test2 pass")
    else:
        print("test2 fail")
        allpass=False
    
    if test3():
        print("test3 pass")
    else:
        print("test3 fail")
        allpass=False
    
    if allpass:
        print("all test pass")
        return 0
    else:
        print("some test fail")
        return 1
