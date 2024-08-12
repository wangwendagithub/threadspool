//#pragma once 只在vs下面有用，在其他编译器下可能失效，ifndef为通用预计
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>


//Any类型 c++ 17支持
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator = (const Any&) = delete;
	Any(Any&&) = default;
	Any& operator = (Any&&) = default;

	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data)) {}

	//这个方法能把Any对象里面存储的data数据提取出来
	template <typename T>
	T cast_()
	{
		//怎么从base_找到指向的派生对象，并提取出成员变
		//基类指针转派生类指针  RTTI 
		Derive<T> *pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	//基类类型
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	//派生类类型
	template<typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data_(data) {}
		T data_;
	};

private:
	//定义一个基类的指针
	std::unique_ptr<Base> base_;
};

//实现一个信号量类
class Semaphore
{
public:
	Semaphore(int limit = 0):resLimit_(limit)
	{
		
	}
	~Semaphore() =  default;

	//获取一个信号量资源
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		//等待信号量有资源，没有资源的话会阻塞当前线程
		cond_.wait(lock, [&]()->bool { return resLimit_ > 0; });
		resLimit_--;
	}

	//增加一个信号量资源
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
	
private:
	
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

//任务抽象基类

class Result;
class Task
{
public:
	Task();
	~Task() = default;


	//用户可自定义任意任务类型，继承Task，重写run方法，实现自定义任务处理
	virtual Any run() = 0;//
	void exec();
	void setResult(Result* res);

private:
	Result* result_;
};

class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;
	void setValue(Any any);
	Any get();

private:
	Any any_;//存储任务的返回值
	Semaphore sem_;//线程通信的信号量
	std::shared_ptr<Task> task_;//指向对应获取返回值的任务对象
	std::atomic_bool isValid_;//返回值是否有效

};
//线程池支持的模式
enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED,
};
//线程类型
class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func);
	~Thread();

	void start();
	int getid() const ;

private:
	ThreadFunc mFunc_;
	static int generateId_;
	int threadId_;//保存线程ID ，用以删除线程及其他
};
//线程池类型
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	//禁止使用拷贝构造和拷贝赋值
	ThreadPool(const ThreadPool& ) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

	void start(int size = std::thread::hardware_concurrency());

	void setMode(PoolMode mode);//设置工作模式
	void setTaskQueMaxThreadHold(int threadHold);//设置任务队列上限阈值

	//设置cached模式下线程的上限阈值
	void setCachedThreadMaxThreadHold(int threadHold);
	


	Result submitTask(std::shared_ptr<Task> sp);//给线程池提交任务

private:
	void threadFunc(int threadId); //消费任务

	bool checkPoolRunning() const ;

private:

	int initThreadSize_;//初始的线程数量
	//std::vector<std::unique_ptr<Thread>> threads_; //线程列表
	std::unordered_map<int, std::unique_ptr<Thread>>threads_;
	int treadMaxThreadHold_;//线程的上限阈值
	std::atomic_int curThreadSize_; //记录当前线程的总数量
	std::atomic_int  idleThreadSize_;	//空闲线程的数量

	std::queue<std::shared_ptr<Task>> taskQue_; //任务队列,使用强智能指针，防止用户传递临时对象
	std::atomic_int taskSize_;//任务数量 
	int taskQueMaxThreadHold_;//任务队列上限阈值

	std::mutex taskQueMtx_;//保证任务队列的额线程安全
	std::condition_variable notFull_;//任务队列不满
	std::condition_variable notEmpty_;//任务队列不空
	std::condition_variable threadExit_;//等待线程资源全部回收


	//表示当前线程池的启动状态
	std::atomic_bool isPoolRunning_;
	PoolMode poolMode_;

};
#endif THREADPOOL
