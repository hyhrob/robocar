﻿#ifndef YMQ_LISTENNER_HPP_
#define YMQ_LISTENNER_HPP_

#include "_inl.hpp"
#include "ymq_socket.hpp"
#include <functional>
#include <mutex>
#include <vector>

#ifdef WIN32

#elif (defined(APPLE))
    #include <sys/event.h>
    #include <sys/types.h>
#elif (defined(LINUX))
    #include <sys/epoll.h> 
#endif

namespace ymq
{
class ymq_listenner : public ymq_socket
{
  public:
    ymq_listenner(char* url)
    {
        this->kq_ = 0;
        this->running_ = false;
        this->obj_thread_ = nullptr;

        fn_received_data_ = nullptr;

        url_ = url;
        int p1 = url_.rfind('/') + 1;
        int p2 = url_.rfind(':');
        ip_ = url_.substr(p1, p2-p1);

        p2 += 1;
        std::string str_port = url_.substr(p2, url_.length()-p2);
        port_ = atoi(str_port.c_str());
    }
    virtual ~ymq_listenner()
    {
        stop();
    }

    void set_received_callback(fn_received_data_handle fn)
    {
        fn_received_data_ = fn;
    }

    virtual bool start()
    {
        stop();

        if(!ymq_socket::start()) return false;

#ifdef WIN32
        /* Create IOCP handle*/
#elif (defined(APPLE))
        /* Create kqueue. */
        this->kq_ = kqueue();
#elif (defined(LINUX))
        this->kq_ = create_epoll1(0);
#endif
        
        /* Create listener on port*/
        if(!create_listener(port_))
        {
            return false;
        }

        /*Register events on kqueue*/
        register_event(sock_);
        
        /*Start events thread*/
        obj_thread_ = new std::thread(&ymq_listenner::process_event, this);

        return true;
    }

    virtual void stop()
    {
        ymq_socket::stop();

        running_ = false;
        if (obj_thread_)
        {
            close(kq_);
            obj_thread_->join();
            delete obj_thread_;
            obj_thread_ = nullptr;
        }
    }

    virtual int send(char* data, int data_len)
    {
        int client_index = 0;
        int sent_len = 0;
        int total_sent = 0;
        int sock = 0;
        while(client_index < clients_fd_.size())
        {
            sock = clients_fd_[client_index];
            total_sent = 0;
            while(total_sent<data_len)
            {
                sent_len = ::send(sock, data, data_len, 0);
                if(sent_len>0)
                {
                    total_sent += sent_len;
                }
                else
                {
                    remove_client(sock);
                    client_index--;
                    break;
                }
            }

            // next client
            client_index++;
        }
        return data_len;
    }

    virtual int recv(char* buff, int buff_len)
    {

    }

  private:
    void process_event()
    {
        running_ = true;
#ifdef WIN32
        /* IOCP event handle*/
#elif (defined(APPLE))
    {
        struct kevent *events = new struct kevent[MAX_SOCKET_EVENT_COUNT];
        int ret = 0;
        while (running_)
        {
            ret = kevent(kq_, NULL, 0, events, MAX_SOCKET_EVENT_COUNT, NULL);
            if(-1==ret)
            {
                continue;
            }

            // handle events
            for (int i = 0; i < nevents; i++)
            {
                int sock = events[i].ident;
                int data = events[i].data;

                if (sock == sock_)
                {
                    int conn_cnt = data;
                    for (int i = 0; i < conn_cnt; i++)
                    {
                        accept_conn(data);
                    }
                }
                else
                {
                    int data_size = data;
                    receive_data(sock, data_size);
                }
            }
        }
        delete[] events;
    }
#elif (defined(LINUX))
    {
        int timeout = 1000; // ms
        int nfds;  
        struct epoll_event* events = new struct epoll_event[ MAX_SOCKET_EVENT_COUNT];
        while(running_)
        {
            nfds = epoll_wait(kq_, events, LISTEN_BACKLOG, timeout);  
            if(-1==nfds)
            {
                continue;
            }

            // handle events
            for(i=0;i<nfds;++i)  
            {  
                if(events[i].data.fd==sock_)
                {  
                    accept_conn(); 
                }  
                else if( events[i].events&EPOLLIN ) // received data, read socket  
                {
                    if (events[i].data.fd < 0)
                        continue;
                        
                    receive_data(events[i].data.fd, -1);

                    /*
                    n = read(sockfd, line, MAXLINE)) < 0    // read data
                    ev.data.ptr = md;     //md is the data will be sent
                    ev.events=EPOLLOUT|EPOLLET;  
                    epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);// modify fd state flag, write event will be triged next tune
                    */
                    
                }  
                else if(events[i].events&EPOLLOUT) //has data need to be sent，write socket  
                {  



                    /*
                    struct myepoll_data* md = (myepoll_data*)events[i].data.ptr;    // get data  
                    sockfd = md->fd;
                    send( sockfd, md->ptr, strlen((char*)md->ptr), 0 );        // send data
                    ev.data.fd=sockfd;  
                    ev.events=EPOLLIN|EPOLLET;  
                    epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev); //modify fd state flag, wait for next tune to read data
                    */
                }  
                else  
                {


                }
            }
        }
        delete[] events;
    }  
#endif
    }

    bool create_listener(int port)
    {
        struct sockaddr_in addr;
        addr.sin_len = sizeof(addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        /* Set all bits of the padding field to 0 */
        memset(addr.sin_zero, '\0', sizeof addr.sin_zero);  

        int ret = ::bind(sock_, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == -1)
        {
            std::cerr << "bind()　failed:" << errno << std::endl;
            return false;
        }

        if( ::listen(sock_, LISTEN_BACKLOG) == -1)
        {
            std::cerr << " listen()　failed:" << errno << std::endl;
            return false;
        }

        return true;
    }

    bool register_event(int fd)
    {
#ifdef WIN32

#elif (defined(APPLE))
        /* Initialize kevent structure. */
        struct kevent event;
        EV_SET(&event, fd, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, NULL);

        /* Attach event to the kqueue. */
        int s = kevent(kq_, &event, 1, NULL, 0, NULL);
        if (s == -1)
        {
            return false;
        }
#elif (defined(LINUX))
        struct epoll_event event;  
        event.data.fd = fd;  
        event.events = EPOLLIN | EPOLLET;  
        int s = epoll_ctl (kq_, EPOLL_CTL_ADD, fd, &event);  
        if (s == -1)  
        {
            return false;
        }  
#endif

        return true;
    }

    bool unregister_event(int fd)
    {
#ifdef WIN32

#elif (defined(APPLE))
        struct kevent event;
        EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        int s = kevent(kq_, &event, 1, NULL, 0, NULL);
        if (s == -1)
        {
            return false;
        }
#elif (defined(LINUX))
        struct epoll_event event;  
        event.data.fd = fd;  
        event.events = EPOLLIN | EPOLLET;  
        int s = epoll_ctl (kq_, EPOLL_CTL_DEL, fd, &event);  
        if (s == -1)  
        {
            return false;
        }
#endif

        return true;
    }

    bool accept_conn()
    {
        struct sockaddr peer_addr;
        socklen_t peer_addr_size = sizeof(struct sockaddr);

        int client = ::accept(sock_, (struct sockaddr *) &peer_addr, &peer_addr_size);
        if (client == -1)
        {
            std::cerr << "Accept failed. ";
            return false;
        }

        if (!register_event(client))
        {
            close(client);
            std::cerr << "Register client failed. ";
            return false;
        }

        std::lock_guard<std::mutex> l(mtx_);
        clients_fd_.push_back(client);

        return true;
    }

    void receive_data(int sock, int availBytes)
    {
#ifdef WIN32

#elif (defined(APPLE))
        int bytes = 0;
        int total_bytes = 0;
        while(total_bytes<availBytes)
        {
            bytes = ::recv(sock, buf_, MAX_RECV_BUFF_SIZE, 0);
            if (bytes == 0 || bytes == -1)
            {
                remove_client(sock);
                std::cerr << "client close or recv failed. ";
                return;
            }
            
            if(fn_received_data_)
            {
                fn_received_data_(buf_, bytes);
            }

            total_bytes+=bytes;
        }

        printf("availBytes=%d, recv=%d\n", availBytes, total_bytes);
#elif (defined(LINUX))

        int bytes = ::recv(sock, buf_, MAX_RECV_BUFF_SIZE, 0);
        if(bytes>0)
        {
        struct epoll_event event;  


        n = read(sockfd, line, MAXLINE)) < 0    //读  
        ev.data.ptr = md;     //md为自定义类型，添加数据  
        ev.events=EPOLLOUT|EPOLLET;  
        epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);//修改标识符，等待下一个循环时发送数据，异步处理的精髓  
        while(total_bytes<availBytes)
        {
            if (bytes == 0 || bytes == -1)
            {
                remove_client(sock);
                std::cerr << "client close or recv failed. ";
                return;
            }
            
            if(fn_received_data_)
            {
                fn_received_data_(buf_, bytes);
            }

            total_bytes+=bytes;
        }

        printf("availBytes=%d, recv=%d\n", availBytes, total_bytes);

#endif
    }

    void remove_client(int sock)
    {
        std::lock_guard<std::mutex> l(mtx_);
        auto s = std::find(std::begin(clients_fd_), std::end(clients_fd_), sock);
        if (s != std::end(clients_fd_))
        {
            clients_fd_.erase(s);
        }
        unregister_event(sock);
        close(sock);
    }

  private:
    int kq_;
    bool running_;

    std::string url_;
    std::string ip_;
    int port_;
    
    char buf_[MAX_RECV_BUFF_SIZE];
    fn_received_data_handle fn_received_data_;
    std::thread *obj_thread_;

    std::mutex mtx_;
    std::vector<int> clients_fd_;
};
} // namespace ymq

#endif //!YMQ_LISTENNER_HPP_