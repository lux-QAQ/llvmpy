def test_always_return(y):
    x = 0
    while y < 10:
        x = x + 1
        return x # Return inside loop
        continue
    return 99 # Code after loop
def while_and_fib():
    ans = 0
    a=0
    count  = 0
    while 1:
        def fib(a):
            if a <=2:
                def _fib(a):
                    if a == 0:
                        return 0
                    elif a == 1:
                        return 1
                    elif a==2:
                        return 1
            else :
                def _fib(a):
                    return fib(a-1) + fib(a-2)
            return _fib(a)
        if fib(a) <= 55:
            pass
        else:
            ans = fib(a)
            break
        a += 1
    return ans
def main():
    print(test_always_return(0))
    print("while_and_fib")
    print(while_and_fib())
    return 0
    