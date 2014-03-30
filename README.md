&> by liangdong01@baidu.com

# 最新变动 #
    1，新增ez_thread类，作为ez_server的多线程底层支撑，可以简化多线程ez_client开发工作。

    2，原有的add_timer/del_timer接口更改为线程安全版本，可以在非ez_poll工作线程发起动作。

    3，新增了run_task接口，它同样是线程安全的，可以在非ez_poll工作线程发起动作，用于异步的传递一个任务到ez_poll线程执行（这种需求在工程中非常常见）。

    4，提供了多线程版本的sample_multi_client, sample_multi_server作为上述接口的示例。
    
    5，valgrind内存测试回归，所有sample正常工作。
    

# 项目介绍 #
    这是C++实现的一个异步网络开发库, 只能够在linux系统下工作.
    开发的初衷是维护一个自实现的, 满足工作需求, 符合工程实践的网络开发库, 可以快速安全的开发业务代码.

# 功能摘要 #
    1.基于epoll作为底层实现,做了fd层以及socket层两级封装,通用性与便捷性并存.
    
    2.在socket层为使用者屏蔽了网络I/O(read/write), 异步连接(connect), 监听连接(listen,accept)等底层调用, 只需关心数据处理(on_message), 连接错误(on_error), 连接关闭(on_close), 以及发送数据(send_message).
    
    3.内置了wakeup接口, 用于唤醒处于挂起状态的poll(epoll_wait)调用, 可以轻松将网络程序扩展至多线程工作模式(one event per thread).
    
    4.对用户负担极小的接口设计与内存管理, 基于面向对象的框架设计符合工程实践, 实现server与client仅需极少的代码.

    5.内置多线程支持ez_thread, 充分发挥单进程性能极限, 使用方法足够简单.

    6.制定了一个简单文本协议, 封装了打包/解包功能, 紧密与整个框架结合, 使用方法简单, 满足常见开发需求.

# 编译方法 #  
    仅支持linux操作系统,

    编译静态库: make
    编译静态库以及测试程序: make test
    清理: make clean

# 设计简介 #
    C++的API类库设计, 我比较关注“分层”这个理念, 即每一层设计都是独立的, 各层都是向上依赖的. 开发者一定要以一个使用者的角度出发, 而不单单从开发者角度思考.

    1.ez_poll是对epoll的基础封装, 其中ez_fd是针对fd以及event callback的抽象基类. ez_poll可以单独工作, 支持任何epoll支持的fd类型. 另外, 还提供了基于map实现的定时器, 保持代码足够简单, 没有为了效率再去设计时间轮算法.
    
    2.基于ez_poll, 对socket做了上层封装, 实现了ez_conn类. 该类继承自ez_fd, 挂载于ez_poll进行网络收发, 包括异步连接操作, 从而保证上层用户只需关注数据的处理与发送. 用户需要继承ez_handler类, 实现on_message/on_error/on_close等接口.
    
    3.基于ez_conn以及ez_poll, 再做上层分装, 实现了ez_server以及ez_client, 方便用户快速的实现服务端以及客户端设计, 其中ez_server为用户屏蔽了监听连接的底层实现, ez_client仅仅包裹了ez_conn, 暂时没有提供实际功能(必要性不大).
    
    4.封装了ez_thread线程池类，为ez_server提供直接的多线程API支持，同时可以为多线程client等其他场景服务，具有通用型。
    
    5.对线程间异步通讯提供了基本抽象，对于ez_poll线程来说，你可以发送对应不同实现的task对象，从而在ez_poll线程中异步执行。

# 使用方法 #
    详细代码见sample_server,sample_client,sample_multi_server,sample_multi_client 其中的使用方法为非常通用的网络程序设计方法, 有很高的参考价值.

    
# 一些说明 #
    
    1.很多人都会做自己的网络库封装, 但有一个问题我发现很多人忽略了, 想象这个场景:一次epoll_wait返回, 在处理fd=1的回调函数中, 删除了fd=3的epoll注册并close(fd=3)随后重建socket复用fd=3, 但本次epoll_wait同时也返回了fd=3的事件, 那么接下来的处理就会导致将之前fd的事件当作新的fd=3的事件. 这种BUG我见过不计其数, 如果你还不理解, 可以看一下ez_poll中是如何延迟删除解决这个问题的.

    2.在ez_conn实现中, 同样面临了1中的类似问题, 即ez_poll的回调函数设计是多个event或运算后一次性通知, 这导致在ez_conn的事件回调函数中, 依次将处理on_read/on_write/on_error, 每一个步骤中都有可能出现错误或者得到数据等原因回调用户的函数, 而用户在回调中关闭了连接, 这就面临被关闭的连接从用户回调函数中返回后不能继续执行后续代码, 否则会有问题, 这是通过一个uint64_t的递增id来判定的. 

    3.对于ez_server做accept得来的ez_conn, 其conn一旦被用户Close, 用户根本不在乎conn的内存释放问题, 而ez_client用来connect的conn的生命期则是长久存在的, 即便close也不应该释放, 因为用户会发起connect重连.这就涉及到一个ez_conn内存管理职责的问题, 这在ez_conn中通过一个del_myself_标记标示, 从而保证ez_conn的内存管理对用户透明且理解起来合情合理. 其中, ez_server创建的ez_conn一旦close就应该释放内存, 所以del_myself为true, 由于(2)中提到的close后内存有效性问题, 采取了定时器延迟删除ez_conn的方式实现.

    4.由于(3)中提到的延迟删除ez_conn的定时器问题, 所以在ez_poll的shutdown中调用了一次poll(0)执行最后一次事件循环, 目的是让所有用于删除ez_conn的定时器全部生效, 避免内存泄漏.

    5.这个库我之所以感觉不错, 主要是因为基本达到了我对接口一致性的要求, 对用户的记忆负担是极小的, 不过我建议感兴趣的同学主要了解设计中的一些关键理念, 自己开发实现自己的网络库, 那样会更有成就感.

    6.有些同学反馈, 问是否可以在同一个ez_poll中同时执行ez_server和ez_client, 以及多线程框架下是否依旧正常工作, 答案是完全可以, 你甚至可以自实现ez_fd并挂载到ez_poll中执行.

    7.多线程工作模式下, 你应该保证在init_threads之前不向线程池做任何操作，同时保证在free_threads后不再向线程池做任何操作，在此之间的任何时间，你的任何异步操作(run_task/add_timer/del_timer)都是可以被正确排队处理的，并且保证在线程池退出前，所有的操作都会兑现（ez_timer自身不到期不会执行，但add_timer/del_timer/run_task发起的异步动作一定会执行完成）

    8.框架自身以及代码事例都经过我较为严格的测试，包括内存valgrind以及性能callgrind检查, 保证代码产出质量.

    9.最新添加了ez_proto类, 这是一个简单文本协议的封装, 作为可以可选的功能提供. 使用ez_proto, 只需要传入ez_conn就可以自动完成字节流的包解析与流缓冲区的清理工作, 应用层只需要关注unpack_message接口的返回值即可.

# 性能测试 #    

    1. 测试机器为12核心, 64G内存物理机, 客户端与服务端均在同一机器, 测试时top截图如下, 此时QPS约为41万. 
    2. 其中服务端包含8个工作线程以及1个监听线程, 客户端为单线程并发10连接. 
    3. 理论计算， 40/7*12=60万, 与我之前的测试经验基本相符, 实际大约有50万+的QPS.
    
> top - 17:22:59 up 268 days,  2:29,  2 users,  load average: 6.39, 6.83, 7.16
> Tasks: 439 total,   8 running, 422 sleeping,   9 stopped,   0 zombie
> Cpu(s): 15.2% us, 35.2% sy,  0.1% ni,  2.2% id,  0.0% wa,  0.0% hi, 47.4% si
> Mem:  65878264k total, 32571748k used, 33306516k free,   627076k buffers
> Swap:  1020088k total,   138440k used,   881648k free, 24328920k cached
> 
> PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+  COMMAND                                                                                                 
> 13044 liangdon  16   0  188m  58m  836 S  593  0.1   4:35.69 sample_multi                                                                                            
> 15629 liangdon  16   0  9692 1152  808 R   96  0.0   0:10.47 sample_client                                                                                           
> 15619 liangdon  16   0  9692 1152  808 R   96  0.0   0:11.39 sample_client                                                                                           
> 15777 liangdon  16   0  9692 1152  808 R   95  0.0   0:05.32 sample_client                                                                                           
> 15620 liangdon  16   0  9692 1152  808 R   94  0.0   0:10.80 sample_client                                                                                           
> 15605 liangdon  16   0  9692 1152  808 R   93  0.0   0:12.78 sample_client    

