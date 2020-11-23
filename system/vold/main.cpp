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
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <dirent.h>
#include <fs_mgr.h>

#define LOG_TAG "Vold"

#include "cutils/klog.h"
#include "cutils/log.h"
#include "cutils/properties.h"

#include "VolumeManager.h"
#include "CommandListener.h"
#include "NetlinkManager.h"
#include "DirectVolume.h"
#include "cryptfs.h"

static int process_config(VolumeManager *vm);
static void coldboot(const char *path);

#define FSTAB_PREFIX "/fstab."
struct fstab *fstab;
/*
Android的存储系统主要由SystemSever进程中的MountService和Vold进程中的VolumeManager组成，它们管理着系统的存储设备，执行各种操作，包括mount、unmount、format等。

在Android的存储系统中,MountService 是为应用提供服务的Binder类,运行在SystemServer中，而 StorageManager 是MountServer的代理，在用户进程中使用的。
Vold是一个守护进程，负责和底层存储系统的驱动交互。
MountService 和 Vold之间通过socket进行通信，这个通信过程是双向的，有从 MountService到 Vold的操作命令，也有从Vold到 MountServcie的消息，用来通知底层硬件发生了变化。

Vold进程的主体是 VolumeManager 对象，它管理着系统底层所有 Volume 对象，实现各种存储相关的操作。
Vold 中 CommandListener 对象负责和 MountService 中的 NativeDemonConnector 进行 Socket 通信，
而 NetlinkHandler对象则负责接监听来自驱动的Netlink Socket消息。

NtelinkMananger 对象的主要作用是创建3个 NetlinkHandler 对象，除此外，NtelinkMananger对象在系统运行过程中几乎毫无作用。

管理存储设备----Vold守护进程

Vold 的全称是 Volume Daemon，用来管理各种存储设备，包括外置USB和 SD卡设备，MountService 通过 Vold进程查询和操作这些存储设备。
如果外部存储设备发生变化，例如，插入了USB设备，Vold将会接收到内核的UEvent消息并转发给MountService。

Vold的main函数

Vold也是通过init进程启动,它在 init.rc中的定义如下:

service vold /system/bin/vold
    class core
    socket vold stream 0660 root mount
    ioprio be 2

vold服务放到了core分组，这就意味着这系统启动时它就会被init进程启动。同时要注意这里定义一个 socket，这种方式定义socket前面已经讲过多次了。
它主要用于Vold和Java 层的MountService通信。
vold模块的源码位于目录system/vold，先看看它的入口函数main(),


main()函数比较简单，主要工作是创建了3个对象:VolumeManager、NetlinkManager和 CommandListener。
同时将CommandListener对象分别设置到了VolumeManager对象和NetlinkManager中。
CommandListener 对象用于和Java层的 NativeDaemonConnector 对象进行 socket 通信，因此，无论是 VolumeManager 对象还是 NetlinkManager 对象都需要拥有
CommandListener 对象的引用。下面先分析 NetlinkManager 对象。





监听驱动发出的消息———Vold的 NetlinkManager对象
NetlinkManager对象主要的工作是监听驱动发出的 uevent 消息。main()函数中调用 NetlinkManager 类的静态函数Instance()来创建NetlinkManager对象，
代码见(NetlinkManager *NetlinkManager::Instance)




处理block类型的uevent
main()函数中创建了 CommandListener 对象， NetlinkManager 的 start()函数中又创建了3个 NetlinkHandler 对象，
如果将 CommandListener 类和 NetlinkHandler 类的继承关系图画出来，会发现它们都是从 SocketListener类派生的.

处于最底层的 SocketListener 类的作用是监听socket的数据，接收到数据后分别交给 FrameworkListener 类和 NetlinkListener 类的函数，
并分别对来自Framework和驱动的数据进行分析，分析后根据命令再分别调用 CommandListener 和 NetlinkHandler 中的函数。
去看看 NetlinkHandler 类的构造函数




处理MountService的命令
CommandListener对象如何处理从 MountService 发送的命令数据呢?下面是 FrameworkListener的 onDataAvailable()函数的代码:
(system\core\libsysutils\src\FrameworkListener.cpp)




VolumeManager的作用———创建实例对象
vold的main()函数中调用了VolumeManager 的 Instance()函数来创建实例对象。
和 NetlinkManager 一样，VolumeManager 也只能创建一个对象。在 main()函数中，还调用了process_config() 函数,代码如下:(static int process_config(VolumeManager *vm))



*/
int main() {

    VolumeManager *vm;
    CommandListener *cl;
    NetlinkManager *nm;

    SLOGI("Vold 2.1 (the revenge) firing up");

    mkdir("/dev/block/vold", 0755);//创建void目录

    /* For when cryptfs checks and mounts an encrypted filesystem */
    klog_set_level(6);

    /* Create our singleton managers */
    if (!(vm = VolumeManager::Instance())) {//创建VolumeManager对象
        SLOGE("Unable to create VolumeManager");
        exit(1);
    };

    if (!(nm = NetlinkManager::Instance())) {//创建NetlinkManager对象
        SLOGE("Unable to create NetlinkManager");
        exit(1);
    };


    cl = new CommandListener();//创建commandListener对象
    vm->setBroadcaster((SocketListener *) cl);//建立vm和cl的联系
    nm->setBroadcaster((SocketListener *) cl);//建立nm和cl的联系

    if (vm->start()) {//启动VolumeManager
        SLOGE("Unable to start VolumeManager (%s)", strerror(errno));
        exit(1);
    }
    //创建文件/fstab.xxx中定义的Volume对象
    if (process_config(vm)) {
        SLOGE("Error reading configuration (%s)... continuing anyways", strerror(errno));
    }

    if (nm->start()) { //启动NetlinkManager
        SLOGE("Unable to start NetlinkManager (%s)", strerror(errno));
        exit(1);
    }

    coldboot("/sys/block");        //创建/sys/block下的节点文件
//    coldboot("/sys/class/switch");

    /*
     * Now that we're up, we can respond to commands
     */
    if (cl->startListener()) {//开始监听Framework的socket
        SLOGE("Unable to start CommandListener (%s)", strerror(errno));
        exit(1);
    }

    // Eventually we'll become the monitoring thread
    while(1) {//进入循环
        sleep(1000);
    }

    SLOGI("Vold exiting");
    exit(0);
}/*

*/

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
/*wwxx
process_config()函数首先读取/fstab.xxx文件的内容，然后根据文件中每行定义的存储器的属性来创建 DirectVolume 对象，所有对象都将加入到VolumeManager中。
/fstab.xxx文件的后缀是根据设备的属性值ro.hardware来确定的，例如 fstab.hammerhead。这个文件有很多行，每行都是设备中分区的5种属性;
第一个值是分区的原始位置，例如/dev/block/platform/msm_sdcc.1/by-name/system;
第二个值是挂载点，例如/system;
第三个值是分区的类型，通常的值为ext4、 vfat和 emmc;
第四个值是mount分区使用的标志和属性，例如ro,barrier=1;
第五个值是定义分区的属性，包括是否加密、是否能够移动等，不过大不部分情况下它的值为default。

process_config()函数会使用从文件中读到的内容，每行创建一个对应的 DirectVolume对象，然后把对象加入到VolumeManager对象中。
前面介绍了，NetlinkHandler 中接收到从驱动来的消息，会调用 VolumeManager 中各种 DirectVolume 对象的 handleBlockEvent() 函数来处理，代码如下:(system\vold\DirectVolume.cpp)


*/
static int process_config(VolumeManager *vm)
{
    char fstab_filename[PROPERTY_VALUE_MAX + sizeof(FSTAB_PREFIX)];
    char propbuf[PROPERTY_VALUE_MAX];
    int i;
    int ret = -1;
    int flags;

    property_get("ro.hardware", propbuf, "");
    snprintf(fstab_filename, sizeof(fstab_filename), FSTAB_PREFIX"%s", propbuf);//拼接文件名，形式为/fstab.xxx，例如:/fstab.hammerhead

    fstab = fs_mgr_read_fstab(fstab_filename);//把文件的内容读入内存
    if (!fstab) {
        SLOGE("failed to open %s\n", fstab_filename);
        return -1;
    }

    /* Loop through entries looking for ones that vold manages */
    for (i = 0; i < fstab->num_entries; i++) {//循环处理文件所有行
        if (fs_mgr_is_voldmanaged(&fstab->recs[i])) {
            DirectVolume *dv = NULL;
            flags = 0;

            /* Set any flags that might be set for this volume */
            if (fs_mgr_is_nonremovable(&fstab->recs[i])) {
                flags |= VOL_NONREMOVABLE;//如果是不能移动的存储器,加上对应的标志
            }
            if (fs_mgr_is_encryptable(&fstab->recs[i])) {
                flags |= VOL_ENCRYPTABLE;//如果是加密的存储器，加上对应的标志
            }
            /* Only set this flag if there is not an emulated sd card */
            if (fs_mgr_is_noemulatedsd(&fstab->recs[i]) &&
                !strcmp(fstab->recs[i].fs_type, "vfat")) {
                flags |= VOL_PROVIDES_ASEC;//外置存储卡，加上对象的标志
            }
            //创建Directvolume对象
            dv = new DirectVolume(vm, &(fstab->recs[i]), flags);

            if (dv->addPath(fstab->recs[i].blk_device)) {
                SLOGE("Failed to add devpath %s to volume %s",
                      fstab->recs[i].blk_device, fstab->recs[i].label);
                goto out_fail;
            }

            vm->addVolume(dv);//把Directvolume对象加入到volumeManager中
        }
    }

    ret = 0;

out_fail:
    return ret;
}
