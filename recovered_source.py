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
    boolvar = 1
    boolvar = - boolvar
    print ( boolvar )
    print ( 测试下'!!' )
    print ( arr[ current_len - 1 ] )
    c = 1
    if c < 0 :
        print ( c小于0 )
    elif c < 1 :
        print ( c小于1 )
    elif c < 2 :
        print ( c小于2 )
    else :
        print ( c大于等于2 )
    return 0
main()