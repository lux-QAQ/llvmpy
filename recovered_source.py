def returntest1( a ):
    return a + 1
def returntest2( a , b ):
    return a[ b ]
def returntest3( a ):
    return a + 2
def returntest4( a ):
    return a + 3
def whiletest( a ):
    while a < 5 :
        a += 1
    return a
def main():
    b = [ 1 , 2 ]
    a = 0.5
    print ( returntest3( a ))
    print ( returntest4( a ))
    print ( returntest1( a ))
    print ( returntest2( b , 1 ))
    print ( whiletest( a ))
    return 0

