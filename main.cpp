#include <iostream>
#include <chrono>
#include <thread>
#include "threadpool.h"
#include <future>
//#include <tempname>

using ulong = unsigned long long;
class MyTask : public Task
{
public:
	MyTask(int begin, int end) :begin_(begin), end_(end) {}

	//�������run �����ķ���ֵ��Any��
	Any run()
	{
		std::this_thread::sleep_for(std::chrono::seconds(10));
		ulong sum = 0;
		//std::cout << "begin threadFunc tid" << std::this_thread::get_id() << std::endl;

		for (int i = begin_; i < end_; i++)
		{
			sum += i;
		}

		//std::cout << "end threadFunc tid " << std::this_thread::get_id() << std::endl;
		return sum;
	}

private:

	int begin_;
	int end_;

};

template<typename T>
int function(int a, int b)
{
	std::this_thread::sleep_for(std::chrono::seconds(5));
	return a + b;
}

double function2(double a,double b)
{
	std::this_thread::sleep_for(std::chrono::seconds(10));
	return a + b;
}



int main()
{
	std::future<int> a = std::async(function, 10, 20);
	std::future<double> b = std::async(function2, 10.23, 20.23);

	std::cout << "a::::" << a.get() << std::endl;
	std::cout << "b::::" << b.get() << std::endl;

	//{
	//	ThreadPool pool;
	//	pool.setMode(PoolMode::MODE_CACHED);
	//	pool.start();

	//	//�����������ķ��ػ���
	//	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res2 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res3 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res4 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res5 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res6 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res7 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res8 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res9 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	//	Result res10 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));

	//	//ulong sum1 = res1.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum2 = res2.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum3 = res3.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum4 = res4.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum5 = res5.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum6 = res6.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum7 = res7.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum8 = res8.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum9 = res9.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������
	//	//ulong sum10 = res10.get().cast_<ulong>();//get����һ��any���ͣ����תΪ���������

	//	//std::cout << "sum" << sum1 + sum2 + sum3 + sum4 + sum5 + sum6 << std::endl;
	//	
	//}//�����˸þֲ�������ʱ��pool�������������������������
	std::cout << "main over!!!" << std::endl;
	getchar();
	return 0;
}