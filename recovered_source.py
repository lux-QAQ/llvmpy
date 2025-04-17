def fib( n , d , current_len , arr ):
    if n < current_len :
        arr[ n ] = d[ n ]
        return d[ n ]
    i = current_len
    while i < n + 1 :
        d[ i ] = d[ i - 1 ] + d[ i - 2 ]
        i = 1 + i
    arr[ n ] = d[ n ]
    return d[ n ]
def main():
    arr = [ 0 ] * 1000
    d = { 0 : 0 , 1 : 1 , 2 : 1 }
    current_len = 3
    print ( fib( 10 , d , current_len , arr ))
    current_len = 11
    print ( fib( 20 , d , current_len , arr ))
    current_len = 21
    print ( 测试下'!' )
    print ( 测试下'!!' )
    c = 2
    if c < 0 :
        print ( c小于-1 )
    elif c > 0.6 :
        print ( c大于-1 )
    elif c < 1 :
        print ( c小于1 )
    elif c > 1 :
        print ( c大于1 )
    else :
        print ( c等于1 )
    return 0
