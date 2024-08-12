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
	//��Ҫ�ȴ��̳߳�����̷߳��� Ŀǰ�����߳�����״̬ 
	// һ.û��ȡ��������̻߳������� �� ����ִ��������߳�

	std::unique_lock<std::mutex> lock(taskQueMtx_);//�Ȼ�ȡ�����ٸı������������ɱ�������ԭ��
	notEmpty_.notify_all();

	threadExit_.wait(lock, [&]()->bool {return threads_.size() == 0;});//threadExit_���������ȴ������߳��˳�

}

bool ThreadPool::checkPoolRunning() const
{
	return isPoolRunning_;
}

void ThreadPool::start(int size)
{
	//����̳߳��Ѿ��������򲻿���������������
	isPoolRunning_ = true;
	initThreadSize_ = size;
	curThreadSize_ = size;

	for (int i = 0; i < initThreadSize_; i++)
	{
		//����thread�̶߳���ʱ�����̺߳���threadFunc�󶨵�thread�̶߳���
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
//���ù���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkPoolRunning()) { return; }
	poolMode_ = mode;
}

//�����̳߳�������ֵ
void ThreadPool::setTaskQueMaxThreadHold(int threadHold)
{
	if (checkPoolRunning()) { return; }
	taskQueMaxThreadHold_ = threadHold;
}

//����cachedģʽ���̵߳�������ֵ
void ThreadPool::setCachedThreadMaxThreadHold(int threadHold)
{
	if (checkPoolRunning()) { return; }

	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		treadMaxThreadHold_ = threadHold;
	}
}

//���̳߳��ύ������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//�߳�ͨ��  �ȴ���������п���
	//while (taskQue_.size() == taskQueMaxThreadHold_)
	//{notFull_.wait(lock);}
	//�������������߳�ʱһ���Ὣ���ͷţ���Ȼ����̻߳�ȡ���������޷����������γ�������
	//�û��ύ������������ܳ���һ�룬�����ж��ύʧ�ܣ�����

	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreadHold_; }))
	{
		//��ʾ����һ�룬�����ύ��Ȼʧ��(������л�ʱ����)
		std::cout << "task is full,submit task fail !" << std::endl;

		//��Ҫ���Ƕ�����������ڣ�Ӧ�ý�sp��װ��Result������
		return  Result(sp,false);
	}

	//����п��࣬������������������
	taskQue_.emplace(sp);
	taskSize_++;

	//���������������в��գ���notEmpty�Ͻ���֪ͨ
	notEmpty_.notify_all();

	//cachedģʽ ������С�����ģʽ  �������ȽϺ�ʱ����ֻ��ͨ��FIXģʽ���д���
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < treadMaxThreadHold_)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadId = ptr->getid();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();//�����߳�
		curThreadSize_++;
		idleThreadSize_++;
	}

	return Result(sp);
}

//���������������������
void ThreadPool::threadFunc(int threadId)
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	while(true) //ÿ���߳�ѭ��������
	{
		std::shared_ptr<Task> task;
		{
			//�Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			std::cout << std::this_thread::get_id()<< "-----���Ի�ȡ����..." << std::endl;

			//cachedģʽ�µ��̻߳������
			//����initThreadSize���߳���Ҫ����
			//��ǰʱ�� - ��һ��ִ�е�ʱ�����60s��Ҫ�� ��
			//ÿ�뷵��һ��  �����ǳ�ʱ���ػ��ǵȴ����񷵻�
			//�̳߳���������������̳߳�����ǰ����ȫ��������***********
			while (taskQue_.size() == 0)
			{
				if (!isPoolRunning_)
				{
					threadExit_.notify_all();//����ִ��������߳��˳�ʱ��Ҫ������������
					threads_.erase(threadId);
					std::cout << "threadId:" << std::this_thread::get_id() << "exit" << std::endl;
					return;
				}

				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					// ����������ʱ����
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);

						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{
							//��ʼ���յ�ǰ�̣߳������߳��б���������ر���ֵ�޸�
							//���߳��б���ɾ�����߳�
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
					//�ȴ�notEmpty����
					notEmpty_.wait(lock);
				}
				//�̳߳�Ҫ�����������߳���Դ
				//if (!isPoolRunning_)
				//{
				//	threads_.erase(threadId);
				//	std::cout << "threadId:" << std::this_thread::get_id() << "exit" << std::endl;
				//	threadExit_.notify_all();//�����������߳��˳�ʱ��Ҫ������������
				//	return;//�����̺߳��������ǽ�������߳�
				//}
			}

			idleThreadSize_--;
			std::cout << std::this_thread::get_id() << "------�����ȡ�ɹ�...." << std::endl;

			//�����������ȡ��һ������
			//����ȡ���������Ҫ�ͷ������ñ���߳̿��Ի�ȡ�������Դ�����������
			// ������Ҫ���Ӿֲ������򣬵������������ͻ��Զ��ͷ�
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//֪ͨ��������֪ͨ�����߳���ִ������
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			//֪ͨ���ύ����
			notFull_.notify_all();
		}
		//��ǰ�̸߳���ִ���������
		if (task != nullptr)
		{
			//task->run();//ִ������ִ����ɺ�����ķ���ֵ��setValue�ķ������ص�Result
			task->exec();
			std::cout<< std::this_thread::get_id() << "-------�����Ѵ���...." << std::endl;
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}

	
}

//*************************�߳���ʵ��**************************
int Thread::generateId_ = 0; //�ֲ���̬������Ҫ���ⲿ����
Thread::Thread(ThreadFunc func) :mFunc_(func)
					,threadId_(generateId_++)
{

}
Thread::~Thread() {}

//�����߳�
void Thread::start()
{
	//����һ���߳���ִ���̺߳���
	std::thread t(mFunc_,threadId_);//c++ 11 ��˵�����̶߳���T ���̺߳��� mFunc_;
	t.detach();//���÷����߳�
}

int Thread::getid() const
{
	return threadId_;
}

//***********************Resultʵ��*****************************
Result::Result(std::shared_ptr<Task> task, bool isValid )
				:task_(task),isValid_(isValid)
{
	task->setResult(this);
}

Any Result::get()//�û�����
{
	if (!isValid_)
	{
		return "";
	}

	sem_.wait(); //task�������û��ִ���꣬���������������߳�
	return std::move(any_);
}
void Result::setValue(Any any)//
{
	this->any_ = std::move(any);
	sem_.post();//�Ѿ���ȡ������ķ���ֵ�������ź�����Դ
}

//*************************Task ʵ��***************************

Task::Task()
	:result_(nullptr) 
{}

void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setValue(run());//���﷢����̬����
	}
}

void Task::setResult(Result* res)
{
	this->result_ = res;
}