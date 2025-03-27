def testwhile (a, b):
    if a > b:
        return a
    elif a == b:
        return - 1
    elif a < b:
        return b

res = testwhile (10)
print (res)