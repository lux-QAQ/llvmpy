def f(x):
    return 2*x*x+x
def find():
    l=-10
    r=0
    while r-l>0.00001:
        mid = (r-l)/2+l
        if f(mid)>0:
            l=mid
        else:
            r=mid
    return l
res=find()
print(res)