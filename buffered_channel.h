#include <queue>
#include <utility>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace std;

template <typename T>
class buffer_channel{
private:
    vector<exception>  g_exceptions;
    mutex lockChannel;
    std::queue<T> buffer;
    std::condition_variable g_queueCheck;
    int buffer_size;
    bool isOpen;
public:
    buffer_channel(int size){
        g_exceptions.clear();
        buffer_size = size;
        isOpen = 1;
    }
    void send(T value){
        lockChannel.lock();
        if(!isOpen){
            g_exceptions.push_back(runtime_error("Tried to send data throw a closed channel"));
            lockChannel.unlock();
            return;
        }else {
            lockChannel.unlock();
            unique_lock<mutex> lock_channel(lockChannel);
            while (buffer_size == buffer.size())
                g_queueCheck.wait(lock_channel);
            buffer.push(value);
            if (buffer.size() == 1)
                g_queueCheck.notify_all();
            lock_channel.unlock();
        }
    };
    pair<T,bool> get(){
        lockChannel.lock();
        if(!isOpen && buffer.size() == 0){
            lockChannel.unlock();
            return pair<T,bool>(T(),false);
        }
        lockChannel.unlock();
        unique_lock<mutex> lock_channel(lockChannel);
        while(buffer.size()>0)
            g_queueCheck.wait(lock_channel);
        T t = buffer.front();
        buffer.pop();
        if(buffer.size() + 1 == buffer_size)
            g_queueCheck.notify_all();
        lock_channel.unlock();
        return pair<T,bool>(t,true);
    }
    void close(){
        lockChannel.lock();
        isOpen = false;
        lockChannel.unlock();

    };
    vector<exception> exceptions(){
        lockChannel.lock();
        vector<exception> temp(g_exceptions);
        lockChannel.unlock();
        return temp;
    }
};