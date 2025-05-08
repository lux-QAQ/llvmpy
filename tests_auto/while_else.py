def test1():
    # 测试1：循环正常结束，应执行 else
    i = 0
    count = 0
    else_executed = False

    while i < 3:
        count = count + 1
        i = i + 1
    else:
        else_executed = True

    if count == 3:
        if else_executed:
            return True
    return False


def test2():
    # 测试2：在 i == 2 时 break，中途跳出，不执行 else
    i = 0
    count = 0
    else_executed = False

    while i < 5:
        if i == 2:
            break
        count = count + 1
        i = i + 1
    else:
        else_executed = True

    if count == 2:
        if not else_executed:
            return True
    return False


def test3():
    # 测试3：使用 continue，不影响 else 执行
    i = 0
    count = 0
    else_executed = False

    while i < 3:
        i = i + 1
        if i == 2:
            continue
        count = count + 1
    else:
        else_executed = True

    if count == 2:
        if else_executed:
            return True
    return False


def test4():
    # 测试4：空循环体，只执行 else
    x = 10
    count = 0
    else_executed = False

    while x < 5:
        count = count + 1
    else:
        else_executed = True

    if count == 0:
        if else_executed:
            return True
    return False


def test5():
    # 测试5：嵌套 while…else，内层总是 break，不执行内层 else；外层正常结束，执行外层 else
    i = 0
    inner_count = 0
    inner_else_executed = False
    outer_else_executed = False

    while i < 3:
        j = 0
        while j < 3:
            if j == 1:
                break
            inner_count = inner_count + 1
            j = j + 1
        else:
            inner_else_executed = True
        i = i + 1
    else:
        outer_else_executed = True

    if inner_count == 3:
        if not inner_else_executed:
            if outer_else_executed:
                return True
    return False


def test6():
    # 测试6：带负数范围和 continue 的组合
    n = -3
    count = 0
    else_executed = False

    while n < 3:
        n = n + 1
        if n == 0:
            continue
        count = count + 1
    else:
        else_executed = True

    if count == 5:
        if else_executed:
            return True
    return False


def test7():
    # 测试7：while True + break + else 不执行
    count = 0
    else_executed = False

    while True:
        count = count + 1
        if count == 1:
            break
    else:
        else_executed = True

    if count == 1:
        if not else_executed:
            return True
    return False

def test8(y):
    x = 0
    while y < 10:
        x = x + 1
        return x # Return inside loop
        continue
    return 99 # Code after loop

def test9():
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
    all_passed = True

    if test1():
        print("\033[32m测试1：循环正常结束，应执行 else - PASSED\033[0m")
    else:
        print("\033[31m测试1：循环正常结束，应执行 else - FAILED\033[0m")
        all_passed = False

    if test2():
        print("\033[32m测试2：中途 break 不执行 else - PASSED\033[0m")
    else:
        print("\033[31m测试2：中途 break 不执行 else - FAILED\033[0m")
        all_passed = False

    if test3():
        print("\033[32m测试3：continue 不影响 else 执行 - PASSED\033[0m")
    else:
        print("\033[31m测试3：continue 不影响 else 执行 - FAILED\033[0m")
        all_passed = False

    if test4():
        print("\033[32m测试4：空循环体只执行 else - PASSED\033[0m")
    else:
        print("\033[31m测试4：空循环体只执行 else - FAILED\033[0m")
        all_passed = False

    if test5():
        print("\033[32m测试5：嵌套循环 break 与 else 行为 - PASSED\033[0m")
    else:
        print("\033[31m测试5：嵌套循环 break 与 else 行为 - FAILED\033[0m")
        all_passed = False

    if test6():
        print("\033[32m测试6：负数范围与 continue 组合 - PASSED\033[0m")
    else:
        print("\033[31m测试6：负数范围与 continue 组合 - FAILED\033[0m")
        all_passed = False

    if test7():
        print("\033[32m测试7：while True + break 不执行 else - PASSED\033[0m")
    else:
        print("\033[31m测试7：while True + break 不执行 else - FAILED\033[0m")
        all_passed = False
    if test8(0) == 1:
        print("\033[32m测试8：while中插入 return - PASSED\033[0m")
    else:
        print("\033[31m测试8：while中插入 return - FAILED\033[0m")
        all_passed = False
    
    if test9() == 89:
        print("\033[32m测试9：while内复杂函数定义 - PASSED\033[0m")
    else:
        print("\033[31m测试9：while内复杂函数定义 - FAILED\033[0m")
        all_passed = False

    if all_passed:
        print("\033[32m所有测试均通过\033[0m")
        return 0
    else:
        print("\033[31m有测试未通过\033[0m")
        return 1

