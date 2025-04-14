def returntest1(a):
    return a+1      
def returntest2(a,b):
    return a[b] + 999    
def returntest3(a):
    return a+2   
def returntest4(a):
    return a+3   
def whiletest(a):
    b=2
    while b < 5:
        b = b+1
        print(b)
    return a
def fib(a):
    if a < 1:
        return 0
    elif a < 2:
        return 1
    elif a < 3:
        return 1
    elif a < 4:
        return 2
    else:
        return fib(a-1) + fib(a-2)
def testrecursion(a,b):
    while a< b:
        print(fib(a))
        a = a+1
    return 1

def main():
    b=[1,2.5]
    a= 0.5
    print(returntest3(a))
    print(returntest4(a))
    print(returntest1(a)) 
    print(returntest2(b,1)) 
    print(whiletest(a))
    testrecursion(5,10)
    return 0

