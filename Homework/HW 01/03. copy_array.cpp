#include <bits/stdc++.h>
using namespace std;  

template <class T, 
	typename enable_if<is_array<T>::value, T>::type * tt = nullptr> 
	void my_copy(T begin, T end, T dest_begin) {
	cout << "Copying array with memcpy." << endl;
	memcpy(dest_begin, begin, (end - begin) * sizeof(begin));
}

template <class T, 
	typename enable_if<!(is_array<T>::value), T>::type * tt = nullptr> 
	void my_copy(T begin, T end, T dest_begin) {
	cout << "Copying array as usual." << endl;
	std::copy(begin, end, dest_begin);
}

int main(){
	int *a = new int[3];
	int *b = new int[3];
	for(int i = 0; i < 3; i++) {
		a[i] = i;
		b[i] = -1;
	}
	//copy<int*>(b, a);
	my_copy<typeof(a)>(a, a + 3, b);
	for(int i = 0; i < 3; i++) {
		cout << b[i] << endl;
	}
	
	int c[3] = {3, 4, 5};
	int d[3] = {-2, -2, -2};
	//copy<int[3]>(d, c);
	my_copy<typeof(c)>(c, c + 3, d);
	for(int i = 0; i < 3; i++) {
		cout << d[i] << endl;
	}
	
	vector<int> v1 = {10, 11, 12};
	vector<int> v2 = {-3, -3, -3};
	my_copy<typeof(v1.begin())>(v1.begin(), v1.end(), v2.begin());
	for(int i = 0; i < 3; i++) {
		cout << v2[i] << endl;
	}
}
