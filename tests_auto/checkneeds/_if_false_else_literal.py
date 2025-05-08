# filepath: /home/ljs/code/llvmpy/tests_auto/checkneeds/_if_false_else_literal.py
def main():
    executed_if_block = False
    executed_else_block = False
    if False:
        executed_if_block = True
    else:
        executed_else_block = True
    
    if executed_else_block:
        if not executed_if_block:
            return 0 # Test passed: else block executed, if block not executed
        else:
            return 2 # Test failed: if block was executed
    else:
        return 1 # Test failed: else block was NOT executed