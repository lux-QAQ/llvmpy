def main(): 
     a = 2 
     if a == 2: 
         def test(): 
             print("a is 2") 
             return "a is 2" 
     else: 
         def test(): 
             print("a is not 2") 
             return "a is not 2" 
     test()  