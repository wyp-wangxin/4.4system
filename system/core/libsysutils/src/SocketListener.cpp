/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#define LOG_TAG "SocketListener"
#include <cutils/log.h>
#include <cutils/sockets.h>

#include <sysutils/SocketListener.h>
#include <sysutils/SocketClient.h>

#define LOG_NDEBUG 0

SocketListener::SocketListener(const char *socketName, bool listen) {
    init(socketName, -1, listen, false);
}

SocketListener::SocketListener(int socketFd, bool listen) {
    init(NULL, socketFd, listen, false);
}

SocketListener::SocketListener(const char *socketName, bool listen, bool useCmdNum) {
    init(socketName, -1, listen, useCmdNum);
}

void SocketListener::init(const char *socketName, int socketFd, bool listen, bool useCmdNum) {
    mListen = listen;
    mSocketName = socketName;
    mSock = socketFd;
    mUseCmdNum = useCmdNum;
    pthread_mutex_init(&mClientsLock, NULL);
    mClients = new SocketClientCollection();
}

SocketListener::~SocketListener() {
    if (mSocketName && mSock > -1)
        close(mSock);

    if (mCtrlPipe[0] != -1) {
        close(mCtrlPipe[0]);
        close(mCtrlPipe[1]);
    }
    SocketClientCollection::iterator it;
    for (it = mClients->begin(); it != mClients->end();) {
        (*it)->decRef();
        it = mClients->erase(it);
    }
    delete mClients;
}
/*wwxx
startListener()函数将开始监听socket，这个函数在 NetlinkHandler 中会被调用，在 CommandListener 中也会被调用。
startListener()函数首先判断变量 mSocketName 是否有值，只有CommandListener对象会对这个变量赋值，它的值就是在 init.rc 中定义的 socket 字符串。
调用函数 android_get_control_socket()的目的是为了从环境变量中取到socket的值(这种创建socket的方法前面已经介绍过)。
这样 CommandListener对象得到了它需要监听的 socket。而对于NetlinkHandler对象而言，它的mSocket不为NULL，前面已经创建出了socket。

startListener()函数接下来会根据成员变量 mListen 的值来判断是否要调用listen()函数来监听socket。这个mListen 的值在对象构造时根据参数来初始化。
对于CommandListener对象，mListen的值为true;
对于NetlinkHandler对象，mListen 的值为 false。这是因为CommandListener对象和SystemServer通信，它需要监听socket连接，而NetlinkHandler对象则不用。

再接下来startListener()函数会创建一个管道，这个管道的作用是通知线程停止监听。这个线程就是 startListener() 函数最后创建的监听线程，它的运行函数是 threadStart()，我们去看看。
*/
int SocketListener::startListener() {

    if (!mSocketName && mSock == -1) {
        SLOGE("Failed to start unbound listener");
        errno = EINVAL;
        return -1;
    } else if (mSocketName) {//只有CommandListener中会设置mSocketName
        if ((mSock = android_get_control_socket(mSocketName)) < 0) {
            SLOGE("Obtaining file descriptor socket '%s' failed: %s",
                 mSocketName, strerror(errno));
            return -1;
        }
        SLOGV("got mSock = %d for %s", mSock, mSocketName);
    }

    if (mListen && listen(mSock, 4) < 0) {//CommandListener中mListen为true
        SLOGE("Unable to listen on socket (%s)", strerror(errno));
        return -1;
    } else if (!mListen)
        mClients->push_back(new SocketClient(mSock, false, mUseCmdNum));

    if (pipe(mCtrlPipe)) {// 创建管道,用于退出监听线程
        SLOGE("pipe failed (%s)", strerror(errno));
        return -1;
    }
    //创建一个监听线程
    if (pthread_create(&mThread, NULL, SocketListener::threadStart, this)) {
        SLOGE("pthread_create (%s)", strerror(errno));
        return -1;
    }

    return 0;
}

int SocketListener::stopListener() {
    char c = 0;
    int  rc;

    rc = TEMP_FAILURE_RETRY(write(mCtrlPipe[1], &c, 1));
    if (rc != 1) {
        SLOGE("Error writing to control pipe (%s)", strerror(errno));
        return -1;
    }

    void *ret;
    if (pthread_join(mThread, &ret)) {
        SLOGE("Error joining to listener thread (%s)", strerror(errno));
        return -1;
    }
    close(mCtrlPipe[0]);
    close(mCtrlPipe[1]);
    mCtrlPipe[0] = -1;
    mCtrlPipe[1] = -1;

    if (mSocketName && mSock > -1) {
        close(mSock);
        mSock = -1;
    }

    SocketClientCollection::iterator it;
    for (it = mClients->begin(); it != mClients->end();) {
        delete (*it);
        it = mClients->erase(it);
    }
    return 0;
}

void *SocketListener::threadStart(void *obj) {
    SocketListener *me = reinterpret_cast<SocketListener *>(obj);

    me->runListener();//直接却看这个函数
    pthread_exit(NULL);
    return NULL;
}

void SocketListener::runListener() {

    SocketClientCollection *pendingList = new SocketClientCollection();

    while(1) {//无限循环
        SocketClientCollection::iterator it;
        fd_set read_fds;
        int rc = 0;
        int max = -1;

        FD_ZERO(&read_fds);

        if (mListen) {//如果需要监听
            max = mSock;
            FD_SET(mSock, &read_fds);//把mSock加入 read_fds
        }

        FD_SET(mCtrlPipe[0], &read_fds);//把管道mctrlPipe [0]也加入到read_fds
        if (mCtrlPipe[0] > max)
            max = mCtrlPipe[0];

        pthread_mutex_lock(&mClientsLock);
        // mclient 中保存的是 NetlinkHandler 对象的socket，或者CommandList接入的socket//把它也加入到read_ fds
        for (it = mClients->begin(); it != mClients->end(); ++it) {
            int fd = (*it)->getSocket();
            FD_SET(fd, &read_fds);
            if (fd > max)
                max = fd;
        }
        pthread_mutex_unlock(&mClientsLock);
        SLOGV("mListen=%d, max=%d, mSocketName=%s", mListen, max, mSocketName);
        //执行select调用,开始等待socket上的数据到来
        if ((rc = select(max + 1, &read_fds, NULL, NULL, NULL)) < 0) {
            if (errno == EINTR)
                continue;   //因为中断退出select，继续
            SLOGE("select failed (%s) mListen=%d, max=%d", strerror(errno), mListen, max);
            sleep(1);
            continue;        //select出错了,睡眠—秒后继续
        } else if (!rc)
            continue;//如果fd上没有数据到达,继续

        if (FD_ISSET(mCtrlPipe[0], &read_fds))
            break;                          //如果是管道上有数据到达,退出循环,结束监听
        if (mListen && FD_ISSET(mSock, &read_fds)) {//如果是CommandListener对象上有连接请求
            struct sockaddr addr;
            socklen_t alen;
            int c;

            do {
                alen = sizeof(addr);
                c = accept(mSock, &addr, &alen);//接入连接请求
                SLOGV("%s got %d from accept", mSocketName, c);
            } while (c < 0 && errno == EINTR);//如果是中断导致失败,重新接入
            if (c < 0) {
                SLOGE("accept failed (%s)", strerror(errno));
                sleep(1);
                continue;//接入发生错误,继续循环
            }
            pthread_mutex_lock(&mClientsLock);
            //把接入的socket 连接加入到mclients，这样再循环时就会监听它的数据到达
            mClients->push_back(new SocketClient(c, true, mUseCmdNum));
            pthread_mutex_unlock(&mClientsLock);
        }

        /* Add all active clients to the pending list first */
        pendingList->clear();
        pthread_mutex_lock(&mClientsLock);
        for (it = mClients->begin(); it != mClients->end(); ++it) {
            int fd = (*it)->getSocket();
            if (FD_ISSET(fd, &read_fds)) {
                //如果mclients中的某个socket上有数据了，把它加入到pendingList列表中
                pendingList->push_back(*it);
            }
        }
        pthread_mutex_unlock(&mClientsLock);

        /* Process the pending list, since it is owned by the thread,
         * there is no need to lock it */
        while (!pendingList->empty()) {//处理pendingList列表
            /* Pop the first item from the list */
            it = pendingList->begin();
            SocketClient* c = *it;
            pendingList->erase(it);//把处理了的socket从pendingList列表中移除
            /* Process it, if false is returned and our sockets are
             * connection-based, remove and destroy it */
            if (!onDataAvailable(c) && mListen) {//调用onDataAvailable函数处理数据
                /* Remove the client from our array */
                SLOGV("going to zap %d for %s", c->getSocket(), mSocketName);
                pthread_mutex_lock(&mClientsLock);
                // 如果socket是listen接入的，而且 onDataAvailable返回false
                //表示不需要这个连接了,把这个socket 从 mClients中移除。
                for (it = mClients->begin(); it != mClients->end(); ++it) {
                    if (*it == c) {
                        mClients->erase(it);
                        break;
                    }
                }
                pthread_mutex_unlock(&mClientsLock);
                /* Remove our reference to the client */
                c->decRef();
            }
        }
    }
    delete pendingList;
}/*
runListener()函数稍微有点长，这是一段标准的处理混合socket连接的代码，笔者在函数中已经加入详尽的注释，读者可以仔细揣摩它的写法，对于我们写socket的程序大有帮助。
runListener()函数接收到从驱动传递的数据或者 MountService 传递的数据后，调用 onDataAvailable() 函数来处理，FrameworkListener 类和 NetlinkListener类都会重载这个函数。
我们先看看 NetlinkListener 类的 onDataAvailable()函数是如何实现的.(system\core\libsysutils\src\NetlinkListener.cpp)
*/

void SocketListener::sendBroadcast(int code, const char *msg, bool addErrno) {
    pthread_mutex_lock(&mClientsLock);
    SocketClientCollection::iterator i;

    for (i = mClients->begin(); i != mClients->end(); ++i) {
        // broadcasts are unsolicited and should not include a cmd number
        if ((*i)->sendMsg(code, msg, addErrno, false)) {
            SLOGW("Error sending broadcast (%s)", strerror(errno));
        }
    }
    pthread_mutex_unlock(&mClientsLock);
}
