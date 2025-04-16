def main():
    c = [ 1 , 2 ]
    print ( c[ 1 ] )
    c[ 1 ] = 3
    print ( c[ 1 ] )
    c = { 1 : 2 , 2 : 3 }
    c[ 2 ] = c[ 1 ] + 2
    print ( c[ 2 ] )
    return 0