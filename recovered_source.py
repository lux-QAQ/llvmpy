def fib( n , d , current_len ):
    if n < current_len :
        return d[ n ]
    i = current_len
    while i < n + 1 :
        d[ i ] = d[ i - 1 ] + d[ i - 2 ]
        i = 1 + i
    return d[ n ]
def main():
    d = { 0 : 0 , 1 : 1 , 2 : 1 }
    current_len = 3
    print ( fib( 10 , d , current_len ))
    current_len = 11
    print ( fib( 20 , d , current_len ))
    current_len = 21
    print ( fib( 30 , d , current_len ))
    return 0

main()