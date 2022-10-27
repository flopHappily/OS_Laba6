#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <fstream>
#include <chrono>
#include <utility>
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
    std::condition_variable g_queueEmpty;
    std::condition_variable g_queueFull;
    int buffer_size;
    bool isOpen;
public:
    buffer_channel(int size){
        g_exceptions.clear();
        buffer_size = size;
        isOpen = true;
    }
    void send(T value){
        unique_lock<mutex> lock_channel(lockChannel);
        if(!isOpen){
            g_exceptions.push_back(runtime_error("Tried to send data throw a closed channel"));
            return;
        }else {
            while (buffer_size == buffer.size())
                g_queueFull.wait(lock_channel);
            buffer.push(value);
            lock_channel.unlock();
            g_queueEmpty.notify_all();
        }
    };
    pair<T,bool> get(){
        lockChannel.lock();
        if(!isOpen && buffer.empty()){
            lockChannel.unlock();
            g_queueEmpty.notify_all();
            return pair<T,bool>(T(),false);
        }
        lockChannel.unlock();
        unique_lock<mutex> lock_channel(lockChannel);
        while(buffer.empty())
            g_queueEmpty.wait(lock_channel);
        T t = buffer.front();
        buffer.pop();
        g_queueFull.notify_all();
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

mutex mat_mutex;

class Data{
public:
    int i1,k1,j1,k2,p1,k3;
    Data():i1(0),k1(0),j1(0),k2(0),p1(0),k3(0){};
    Data(int a,int b,int c,int d,int e, int g):i1(a),k1(b),j1(c),k2(d),p1(e),k3(g){};
};

ostream& operator<<(ostream& os, vector<vector<int>> res){
    for(int i = 0;i<res.size();i++){
        for(int j = 0;j<res[0].size();j++)
            os << res[i][j] << ' ';
        os << '\n';
    }
    return os;
}
istream& operator>>(istream& in, vector<vector<int>> &res){
    int n,m;
    in >> n >> m;
    res.resize(n,vector<int>(m));
    for(int i = 0;i<res.size();i++){
        for(int j = 0;j<res[0].size();j++)
            in >> res[i][j];
    }
    return in;
}

void calcBlock(vector<vector<int>> const &mat1,vector<vector<int>> const &mat2,vector<vector<int>> &res,Data data){
    lock_guard<mutex> lock(mat_mutex);
    for(int i = data.i1;i<data.k1;i++)
        for (int j = data.j1; j < data.k2; j++)
            for (int p = data.p1; p < data.k3;p++)
                res[i][j] += mat1[i][p] * mat2[p][j];
}

void sendBlocks(int k,int m,int n,buffer_channel<Data> &channel){
    int i =0,j,p;
    while(i+k<=n){
        j = 0;
        while(j+k<=n){
            p = 0;
            while(p+k<=m) {
                channel.send(Data(i, i + k, j, j + k, p, p + k));
                p+=k;
            }
            if(m%k!=0) {
                channel.send(Data( i, i + k, j, j + k, p, p + m % k));
            }
            j+=k;
        }
        if(n%k!=0){
            p = 0;
            while(p+k<=m) {
                channel.send(Data( i, i + k, j, j + n%k, p, p + k));
                p+=k;
            }
            if(m%k!=0) {
                channel.send(Data( i, i + k, j, j + n % k, p, p + m % k));
            }
        }
        i+=k;
    }
    if(n%k!=0){
        j = 0;
        while(j+k<=n){
            p = 0;
            while(p+k<=m) {
                channel.send(Data( i, i + n%k, j, j + k, p, p + k));
                p+=k;
            }
            if(m%k!=0)
                channel.send(Data(i,i+n%k,j,j+k,p,p+m%k));
            j+=k;
        }
        if(n%k!=0){
            p = 0;
            while(p+k<m) {
                channel.send(Data( i, i + n%k, j, j + n%k, p, p + k));
                p+=k;
            }
            if(m%k!=0)
                channel.send(Data(i,i+n%k,j,j+n%k,p,p+m%k));
        }
    }
    channel.close();
}

void calc(vector<vector<int>> const &mat1,vector<vector<int>> const &mat2,vector<vector<int>> &res,buffer_channel<Data> &channel){
    pair<Data,bool> Block;
    Block = channel.get();
    while(Block.second){
        calcBlock(mat1,mat2,res,Block.first);
        Block = channel.get();
    }
}


void MatricesByBlocks(int k,vector<vector<int>> const &mat1,vector<vector<int>> const &mat2,vector<vector<int>> &res){
    buffer_channel<Data> channel(k);
    vector<thread> calculationThreads;
    thread sendingThread = thread(sendBlocks,k,mat2.size(),res.size(),ref(channel));
    for(int i = 0;i<2;i++)
        calculationThreads.push_back(thread(calc,ref(mat1),ref(mat2),ref(res),ref(channel)));
    sendingThread.join();
    cout << " All packets send\n";
    for(int i = 0;i<2;i++)
        calculationThreads[i].join();
}


int main() {
    cout << fixed << setprecision(9) << left;
    vector<vector<int>> mat1;
    vector<vector<int>> mat2;
    long long t;
    ifstream fin("../dat.txt");
    fin >> mat1 >> mat2;
    int n = mat1.size();
    for(int i = 1;i<=n;i++) {
        cout << "Threads count: "<< i;
        vector<vector<int>> res(mat1.size(),vector<int>(mat1.size(),0));
        auto start = chrono::high_resolution_clock::now();
        MatricesByBlocks(i, mat1, mat2, res);
        auto end = chrono::high_resolution_clock::now();
        cout << "   Time: " <<chrono::duration<double>(end-start).count()<< endl;
    }
    return 0;
}
