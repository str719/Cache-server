#include <bits/stdc++.h>
using namespace std;

class incrementor {
public:
	operator++ ();
};

class non_incrementor {
};

template<typename T> class has_increment_operator {
	template <typename T1> static char test(typeof(&T1::operator++));
	template <typename T1> static int test(...);    

public:
    enum { value = (is_arithmetic<T>::value || sizeof(test<T>(0)) == sizeof(char)) };
	static void print_result() {
		if (value) {
			cout << "YES" << endl;
		} else {
			cout << "NO" << endl;
		}
	}
};

int main(){
	has_increment_operator<int>::print_result();
	has_increment_operator<incrementor>::print_result();
	has_increment_operator<non_incrementor>::print_result();
}
