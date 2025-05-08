# filepath: /home/ljs/code/llvmpy/tests_auto/checkneeds/_func_cond_true.py
def always_true():
    return True

def main():
    if always_true():
        return 0 # Test passed
    else:
        return 1 # Test failed