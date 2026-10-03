#include<iostream>
using namespace std;
short n;
int fib[47];
int main(){
	cin>>n;
	fib[0]=fib[1]=1;
	for(int i=2;i<47;i++)fib[i]=fib[i-1]+fib[i-2];
	cout<<fib[n-1];
} 
