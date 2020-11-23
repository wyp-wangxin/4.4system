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
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <dirent.h>

#define LOG_TAG "Netd"

#include "cutils/log.h"

#include "CommandListener.h"
#include "NetlinkManager.h"
#include "DnsProxyListener.h"
#include "MDnsSdListener.h"
#include "UidMarkMap.h"

static void coldboot(const char *path);
static void sigchld_handler(int sig);
static void blockSigpipe();

/*wwxx
Netd 的全称是Network Daemon，主要作用是管理各种网络设备。Netd是一个守护进程，通过init进程启动.
Netd模块的源代码位于目录system/netd中，我们先看看它的入口函数，代码如下:

看了Netd模块的 main()函数的代码，我们应该意识到它的架构和前一章介绍的vold的架构几乎是一样的，都是通过 NetlinkManager 来监听内核的 Netlink Socket，
然后读取内核发送的uevent，这些uevent最后会在 NetlinkHander对象的onEvent()函数中得到处理。
现在不再关注NetlinkManager和 SocketListener等框架代码，只分析与功能相关部分。


在init.rc文件中netd申明了3个socket，因此，main()函数中创建了3个 FrameworkListenter对象:CommandListener、DnsProxyListener和 MdnsSdListener，
分别监听这3个 socket。



处理net类型的uevent 
从内核发送过来的uevent最终会在 NetlinkHandler 的 onEvent()函数中处理，这些uevent 是内核监测到硬件或驱动发生了变化后发出的。下面是onEvent()函数的代码，在system/netd/NetlinkHandler.cpp中


处理 NMS的命令
CommandListener对象用来处理从 NetworkManagementService中发送的命令。CommandListener类中注册的命令如下:
见分析system/netd/CommandListener.cpp 中 CommandListener::CommandListener(UidMarkMap *map) 处。


DNS服务代理
DnsProxyListener对象实现了DNS服务的代理功能，它注册了很多命令:
见system/netd/DnsProxyListener.cpp 


MDnsSdListener的作用——与守护进程进行交互
见system/netd/MDnsSdListener.cpp
*/

int main() {

    CommandListener *cl;
    NetlinkManager *nm;
    DnsProxyListener *dpl;
    MDnsSdListener *mdnsl;

    ALOGI("Netd 1.0 starting");

//    signal(SIGCHLD, sigchld_handler);
    blockSigpipe();

    if (!(nm = NetlinkManager::Instance())) {
        ALOGE("Unable to create NetlinkManager");
        exit(1);
    };

    UidMarkMap *rangeMap = new UidMarkMap();

    cl = new CommandListener(rangeMap);
    nm->setBroadcaster((SocketListener *) cl);

    if (nm->start()) {
        ALOGE("Unable to start NetlinkManager (%s)", strerror(errno));
        exit(1);
    }

    // Set local DNS mode, to prevent bionic from proxying
    // back to this service, recursively.
    //注意,这里设置了一个环境变量
    setenv("ANDROID_DNS_MODE", "local", 1);
    dpl = new DnsProxyListener(rangeMap);
    if (dpl->startListener()) {
        ALOGE("Unable to start DnsProxyListener (%s)", strerror(errno));
        exit(1);
    }

    mdnsl = new MDnsSdListener();
    if (mdnsl->startListener()) {
        ALOGE("Unable to start MDnsSdListener (%s)", strerror(errno));
        exit(1);
    }
    /*
     * Now that we're up, we can respond to commands
     */
    if (cl->startListener()) {
        ALOGE("Unable to start CommandListener (%s)", strerror(errno));
        exit(1);
    }

    // Eventually we'll become the monitoring thread
    while(1) {
        sleep(1000);
    }

    ALOGI("Netd exiting");
    exit(0);
}

static void do_coldboot(DIR *d, int lvl)
{
    struct dirent *de;
    int dfd, fd;

    dfd = dirfd(d);

    fd = openat(dfd, "uevent", O_WRONLY);
    if(fd >= 0) {
        write(fd, "add\n", 4);
        close(fd);
    }

    while((de = readdir(d))) {
        DIR *d2;

        if (de->d_name[0] == '.')
            continue;

        if (de->d_type != DT_DIR && lvl > 0)
            continue;

        fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY);
        if(fd < 0)
            continue;

        d2 = fdopendir(fd);
        if(d2 == 0)
            close(fd);
        else {
            do_coldboot(d2, lvl + 1);
            closedir(d2);
        }
    }
}

static void coldboot(const char *path)
{
    DIR *d = opendir(path);
    if(d) {
        do_coldboot(d, 0);
        closedir(d);
    }
}

static void sigchld_handler(int sig) {
    pid_t pid = wait(NULL);
    ALOGD("Child process %d exited", pid);
}

static void blockSigpipe()
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0)
        ALOGW("WARNING: SIGPIPE not blocked\n");
}
