#include <iostream>

using namespace std;

void sort(int arr[],int size)
{
	for(int i = 0;i<size -1;i++)
	{
		for(int j = i;j<size-1;j++)
		{
			if( arr[i] < arr[j])
			{
				int tmp = arr[i];
			       arr[i] = arr[j];
		       		arr[j] = tmp;	       
			}
		}
	}		

}
int main()
{
	int arr[] = {3,5,1,7,9,2};
	int size = sizeof(arr)/sizeof(arr[0]);
	sort(arr,size);
	return 0;
} 
