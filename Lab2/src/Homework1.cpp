#include<iostream>
#include<vector>

using namespace std;
const int N = 40;

inline void sum(int* p, int n, vector<int>& d[]) //inline fuction
{
	int i;
	*p = 0;
	for (i = 0; i < n; ++i)
		*p = *p + d[i];//using vector data
}

int main() 
{
	int i;
	int accum = 0;
	vector<int> data;
	for (i = 0; i < N; ++i)
		data[i] = i;
	sum(&accum, N, data);//using the function
	cout << "sum is " << accum << endl;
	return 0;
}