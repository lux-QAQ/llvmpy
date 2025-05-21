class A:
    def test(self):
        print("test")
        return 1
class B:
    def test(self):
        print("test2")
        return 2
def main():
    a = A()
    a.test()
    b = B()
    b.test()