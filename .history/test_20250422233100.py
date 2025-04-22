def func_fib(em):
    if em <= 0:
        return 0
    elif em == 1:
        return 1
    elif em == 2:
        return 1
    else:
        return func_fib(em-1) + func_fib(em-2)


def main():
    print(func_fib(6))
    print(func_fib(5))
    print(func_fib(6))
    return 0

