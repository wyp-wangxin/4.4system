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
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>

#define LOG_TAG "NetlinkListener"
#include <cutils/log.h>
#include <cutils/uevent.h>

#include <sysutils/NetlinkEvent.h>

#if 1
/* temporary version until we can get Motorola to update their
 * ril.so.  Their prebuilt ril.so is using this private class
 * so changing the NetlinkListener() constructor breaks their ril.
 */
NetlinkListener::NetlinkListener(int socket) :
                            SocketListener(socket, false) {
    mFormat = NETLINK_FORMAT_ASCII;
}
#endif

NetlinkListener::NetlinkListener(int socket, int format) :
                            SocketListener(socket, false), mFormat(format) {
}

bool NetlinkListener::onDataAvailable(SocketClient *cli)
{
    int socket = cli->getSocket();
    ssize_t count;
    uid_t uid = -1;

    count = TEMP_FAILURE_RETRY(uevent_kernel_multicast_uid_recv(
                                       socket, mBuffer, sizeof(mBuffer), &uid));
    if (count < 0) {//如果count <0,进行错误处理
        if (uid > 0)
            LOG_EVENT_INT(65537, uid);
        SLOGE("recvmsg failed (%s)", strerror(errno));
        return false;
    }

    NetlinkEvent *evt = new NetlinkEvent();
    if (!evt->decode(mBuffer, count, mFormat)) {
        SLOGE("Error decoding NetlinkEvent");
    } else {
        onEvent(evt);
    }

    delete evt;
    return true;
}/*wwxx
NetlinkListener类的onDataAvailable()函数首先调用uevent_kernel_multicast_uid_recv()函数来接收uvent消息。
接收到消息后，创建 NetlinkEvent 对象，然后调用它的 decode()函数对消息进行解码，然后用得到的消息数据给NetlinkEvent对象的成员变量赋值。
最后 onDataAvailable()函数调用了 onEvent() 函数继续处理消息,onEvent()函数的代码如下:(system\vold\NetlinkHandler.cpp)
*/
