//#pragma once ֻ��vs�������ã��������������¿���ʧЧ��ifndefΪͨ��Ԥ��
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


//Any���� c++ 17֧��
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

	//��������ܰ�Any��������洢��data������ȡ����
	template <typename T>
	T cast_()
	{
		//��ô��base_�ҵ�ָ����������󣬲���ȡ����Ա��
		//����ָ��ת������ָ��  RTTI 
		Derive<T> *pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	//��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	//����������
	template<typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data_(data) {}
		T data_;
	};

private:
	//����һ�������ָ��
	std::unique_ptr<Base> base_;
};

//ʵ��һ���ź�����
class Semaphore
{
public:
	Semaphore(int limit = 0):resLimit_(limit)
	{
		
	}
	~Semaphore() =  default;

	//��ȡһ���ź�����Դ
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		//�ȴ��ź�������Դ��û����Դ�Ļ���������ǰ�߳�
		cond_.wait(lock, [&]()->bool { return resLimit_ > 0; });
		resLimit_--;
	}

	//����һ���ź�����Դ
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

//����������

class Result;
class Task
{
public:
	Task();
	~Task() = default;


	//�û����Զ��������������ͣ��̳�Task����дrun������ʵ���Զ���������
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
	Any any_;//�洢����ķ���ֵ
	Semaphore sem_;//�߳�ͨ�ŵ��ź���
	std::shared_ptr<Task> task_;//ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_;//����ֵ�Ƿ���Ч

};
//�̳߳�֧�ֵ�ģʽ
enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED,
};
//�߳�����
class Thread
{
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func);
	~Thread();

	void start();
	int getid() const ;

private:
	ThreadFunc mFunc_;
	static int generateId_;
	int threadId_;//�����߳�ID ������ɾ���̼߳�����
};
//�̳߳�����
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	//��ֹʹ�ÿ�������Ϳ�����ֵ
	ThreadPool(const ThreadPool& ) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

	void start(int size = std::thread::hardware_concurrency());

	void setMode(PoolMode mode);//���ù���ģʽ
	void setTaskQueMaxThreadHold(int threadHold);//�����������������ֵ

	//����cachedģʽ���̵߳�������ֵ
	void setCachedThreadMaxThreadHold(int threadHold);
	


	Result submitTask(std::shared_ptr<Task> sp);//���̳߳��ύ����

private:
	void threadFunc(int threadId); //��������

	bool checkPoolRunning() const ;

private:

	int initThreadSize_;//��ʼ���߳�����
	//std::vector<std::unique_ptr<Thread>> threads_; //�߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>>threads_;
	int treadMaxThreadHold_;//�̵߳�������ֵ
	std::atomic_int curThreadSize_; //��¼��ǰ�̵߳�������
	std::atomic_int  idleThreadSize_;	//�����̵߳�����

	std::queue<std::shared_ptr<Task>> taskQue_; //�������,ʹ��ǿ����ָ�룬��ֹ�û�������ʱ����
	std::atomic_int taskSize_;//�������� 
	int taskQueMaxThreadHold_;//�������������ֵ

	std::mutex taskQueMtx_;//��֤������еĶ��̰߳�ȫ
	std::condition_variable notFull_;//������в���
	std::condition_variable notEmpty_;//������в���
	std::condition_variable threadExit_;//�ȴ��߳���Դȫ������


	//��ʾ��ǰ�̳߳ص�����״̬
	std::atomic_bool isPoolRunning_;
	PoolMode poolMode_;

};
#endif THREADPOOL
