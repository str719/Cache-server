#include <bits/stdc++.h>
using namespace std;


int inc(int a) {
	return a + 1;
}


template <typename F, int depth,
	typename enable_if<depth == 1, F>::type * tt = nullptr> 
	F multicall(F (*func)(F), F argument) {
	return func(argument);
}


template <typename F, int depth,
	typename enable_if<(depth > 1), F>::type * tt = nullptr> 
	F multicall(F (*func)(F), F argument) {
	return multicall<F, depth - 1>(func, func(argument));
}


int main(){
	cout << multicall<int, 5>(inc, 10) << endl;
}
