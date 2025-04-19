def sum_ints(ints: list[int]) -> int:
    return sum(ints)

# Correct usage
print("sum_ints([1, 2, 3]) =", sum_ints([1, 2, 3]))

# Incorrect usage: passing list[str] will cause a runtime error
print("sum_ints(['a', 'b', 'c']) =", sum_ints(['a', 'b', 'c']))
