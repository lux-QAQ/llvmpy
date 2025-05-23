class A:
    def say(self): 
        print("A")

class B(A):
    def say(self):
        print("B")

class C(A):
    def say(self):
        print("C")

class D(B, C):
    def __init__(self):
        print("D.__init__")
    def say(self):
        print("D\a")

D().say()
print(D)
