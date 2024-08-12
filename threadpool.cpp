#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THREADHOLD = INT32_MAX;
const int THREAD_MAX_THREADHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;


ThreadPool::ThreadPool():initThreadSize_(4)
						,taskSize_(0)
						,idleThreadSize_(0)
						,taskQueMaxThreadHold_(TASK_MAX_THREADHOLD)
						,treadMaxThreadHold_(THREAD_MAX_THREADHOLD)
						,poolMode_(PoolMode::MODE_FIXED)
						,isPoolRunning_(false)
						,curThreadSize_(0)
{
}

ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	//需要等待线程池里的线程返回 目前所有线程两种状态 
	// 一.没获取到任务的线程还在阻塞 二 还在执行任务的线程

	std::unique_lock<std::mutex> lock(taskQueMtx_);//先获取锁，再改变条件变量，可避免死锁原因
	notEmpty_.notify_all();

	threadExit_.wait(lock, [&]()->bool {return threads_.size() == 0;});//threadExit_条件变量等待其他线程退出

}

bool ThreadPool::checkPoolRunning() const
{
	return isPoolRunning_;
}

void ThreadPool::start(int size)
{
	//如果线程池已经启动，则不可以设置其他参数
	isPoolRunning_ = true;
	initThreadSize_ = size;
	curThreadSize_ = size;

	for (int i = 0; i < initThreadSize_; i++)
	{
		//创建thread线程对象时，将线程函数threadFunc绑定到thread线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getid();
		threads_.emplace(threadId,std::move(ptr));
	}

	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		idleThreadSize_++;
	}
}
//设置工作模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkPoolRunning()) { return; }
	poolMode_ = mode;
}

//设置线程池上限阈值
void ThreadPool::setTaskQueMaxThreadHold(int threadHold)
{
	if (checkPoolRunning()) { return; }
	taskQueMaxThreadHold_ = threadHold;
}

//设置cached模式下线程的上限阈值
void ThreadPool::setCachedThreadMaxThreadHold(int threadHold)
{
	if (checkPoolRunning()) { return; }

	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		treadMaxThreadHold_ = threadHold;
	}
}

//给线程池提交任务（生产任务）
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//线程通信  等待任务队列有空余
	//while (taskQue_.size() == taskQueMaxThreadHold_)
	//{notFull_.wait(lock);}
	//条件变量阻塞线程时一定会将锁释放，不然别的线程获取不到锁，无法消费任务，形成死锁。
	//用户提交任务，最长阻塞不能超过一秒，否则判断提交失败，返回

	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreadHold_; }))
	{
		//表示阻塞一秒，任务提交依然失败(任务队列还时满的)
		std::cout << "task is full,submit task fail !" << std::endl;

		//需要考虑对象的生命周期，应该将sp封装进Result对象中
		return  Result(sp,false);
	}

	//如果有空余，将任务放入任务队列中
	taskQue_.emplace(sp);
	taskSize_++;

	//添加了任务，任务队列不空，在notEmpty上进行通知
	notEmpty_.notify_all();

	//cached模式 ：处理小而块的模式  如果任务比较耗时可能只能通过FIX模式进行处理
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < treadMaxThreadHold_)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getid();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();//启动线程
		curThreadSize_++;
		idleThreadSize_++;
	}

	return Result(sp);
}

//从任务队列里面消费任务
void ThreadPool::threadFunc(int threadId)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(true) //每个线程循环抢夺锁
	{
		std::shared_ptr<Task> task;
		{
			//先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << std::this_thread::get_id()<< "-----尝试获取任务..." << std::endl;

			//cached模式下的线程回收设计
			//超过initThreadSize的线程需要回收
			//当前时间 - 上一次执行的时间大于60s需要回 收
			//每秒返回一次  区分是超时返回还是等待任务返回
			//线程池里的所有任务在线程池析构前必须全部处理完***********
			while (taskQue_.size() == 0)
			{
				if (!isPoolRunning_)
				{
					threadExit_.notify_all();//正在执行任务的线程退出时需要告诉析构函数
					threads_.erase(threadId);
					std::cout << "threadId:" << std::this_thread::get_id() << "exit" << std::endl;
					return;
				}

				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					// 条件变量超时返回
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);

						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{
							//开始回收当前线程，激励线程列表数量的相关变量值修改
							//从线程列表中删除该线程
							threads_.erase(threadId);
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "threadId-----:" << std::this_thread::get_id() << "exit" << std::endl;
							return;
						}
					}
				}
				else
				{
					//等待notEmpty条件
					notEmpty_.wait(lock);
				}
				//线程池要结束，回收线程资源
				//if (!isPoolRunning_)
				//{
				//	threads_.erase(threadId);
				//	std::cout << "threadId:" << std::this_thread::get_id() << "exit" << std::endl;
				//	threadExit_.notify_all();//正在阻塞的线程退出时需要告诉析构函数
				//	return;//结束线程函数，就是结束这个线程
				//}
			}

			idleThreadSize_--;
			std::cout << std::this_thread::get_id() << "------任务获取成功...." << std::endl;

			//从任务队列中取出一个任务
			//当获取到任务后，需要释放锁，让别的线程可以获取锁，可以处理其他任务，
			// 所以需要增加局部作用域，当出作用域供锁就会自动释放
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//通知还有任务，通知其他线程来执行任务
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			//通知可提交任务
			notFull_.notify_all();
		}
		//当前线程负责执行这个任务
		if (task != nullptr)
		{
			//task->run();//执行任务，执行完成后将任务的返回值用setValue的方法返回到Result
			task->exec();
			std::cout<< std::this_thread::get_id() << "-------任务已处理...." << std::endl;
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}

	
}

//*************************线程类实现**************************
int Thread::generateId_ = 0; //局部静态变量需要在外部定义
Thread::Thread(ThreadFunc func) :mFunc_(func)
					,threadId_(generateId_++)
{

}
Thread::~Thread() {}

//启动线程
void Thread::start()
{
	//创建一个线程来执行线程函数
	std::thread t(mFunc_,threadId_);//c++ 11 来说，有线程对象T 和线程函数 mFunc_;
	t.detach();//设置分离线程
}

int Thread::getid() const
{
	return threadId_;
}

//***********************Result实现*****************************
Result::Result(std::shared_ptr<Task> task, bool isValid )
				:task_(task),isValid_(isValid)
{
	task->setResult(this);
}

Any Result::get()//用户调用
{
	if (!isValid_)
	{
		return "";
	}

	sem_.wait(); //task任务如果没有执行完，这里会阻塞任务的线程
	return std::move(any_);
}
void Result::setValue(Any any)//
{
	this->any_ = std::move(any);
	sem_.post();//已经获取了任务的返回值，增加信号量资源
}

//*************************Task 实现***************************

Task::Task()
	:result_(nullptr) 
{}

void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setValue(run());//这里发生多态调用
	}
}

void Task::setResult(Result* res)
{
	this->result_ = res;
}