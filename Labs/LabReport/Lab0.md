# Lab 0: RV64 内核调试 
---
## 1 实验目的

按照实验流程搭建实验环境，掌握基本的 Linux 概念与用法，熟悉如何从 Linux 源代码开始将内核运行在 QEMU 模拟器上，学习使用 GDB 跟 QEMU 对代码进行调试，为后续实验打下基础。

## 2 实验内容及要求

- 学习 Linux 基本知识
- 安装 Docker，下载并导入 Docker 镜像，熟悉docker相关指令
- 编译内核并用 GDB + QEMU 调试，在内核初始化过程中设置断点，对内核的启动过程进行跟踪，并尝试使用 GDB 的各项命令

## 3 操作方法和实验步骤  

### 3.1 安装 Docker 环境并创建容器

+ 本人使用win11+WSL2，通过VSCode的WSL插件连接WSL-Ubuntu22.04进行实验
#### 3.1.1 安装 Docker 并启动

+ 查看版本，正确安装

![[0_1.png]]

+ 开机自启

![[0_2.png]]

#### 3.1.2 下载并导入 Docker 镜像

``` shell
# 进入 oslab.tar 所在的文件夹
$ cd ~/code/OS
# 导入docker镜像
$ docker import oslab.tar oslab:2023
# 查看docker镜像
$ docker image ls
```

+ 运行结果如下，后续将用户加入docker组中解决了权限问题，这里仍然使用sudo提升权限
![[0_3.png]]

#### 3.1.3 从镜像创建一个容器并进入该容器

+ 本人使用vscode直接进行对容器的操作

+ 首先创建容器：![[0_4.png]]
+ 命令各参数含义：
``` shell
docker run -it -v ~/code/OS/OSexp:/home/oslab/os_experiment oslab:2023 /bin/bash
```
 1. `-it` : `-i`(交互模式运行容器) 与 `-t`(为容器分配伪输入终端) 的组合
 2. `-v` : 绑定卷，这里将容器中的`/home/oslab/os_experiment`绑定至本地的`~/code/OS/OSexp`
 3. `/bin/bash` : 运行bash
#### 3.1.4 测试映射关系

```Shell
$ cd ~/code/OS/OSexp
$ touch a.txt
$ ls
```
+ 通过docker插件提供的文件目录可以看到在建立映射的`/home/oslab/os_experiment`目录下出现了测试时创建的`a.txt`文件
![[0_5.png]]

### 3.2 编译 Linux 内核 

+ 编译命令
```Shell
$ make -C linux \
	O=/home/oslab/lab0/build/linux \ 
	CROSS_COMPILE=riscv64-unknown-linux-gnu- \       
	ARCH=riscv \       
	CONFIG_DEBUG_INFO=y \ 
	defconfig \       
	all \       
	-j$(nproc)
```
+ 编译完成：![[0_6.png]]

### 3.3 使用 QEMU 运行内核
```Shell
qemu-system-riscv64 \
	-nographic \    
	-machine virt \    
	-kernel build/linux/arch/riscv/boot/Image  \    
	-device virtio-blk-device,drive=hd0 \    
	-append "root=/dev/vda ro console=ttyS0"   \    
	-bios default -drive file=rootfs.ext4,format=raw,id=hd0 \    
	-netdev user,id=net0 -device virtio-net-device,netdev=net0
```

+ 成功登录：![[0_7.png]]
+ 查看架构：
``` Shell
# uname -a
# Linux buildroot 5.8.11 #1 SMP Thu Sep 21 03:15:25 UTC 2023 riscv64 GNU/Linux
```
![[0_8.png]]

### 3.4 使用GDB调试内核

首先请你退出上一步使用 QEMU 运行的内核，并重新使用 QEMU 按照下述参数模拟运行内核（**不是指在上一步运行好的 QEMU 运行的内核中再次运行下述命令！**）

``` Shell
$ qemu-system-riscv64 \
	-nographic \    
	-machine virt \    
	-kernel build/linux/arch/riscv/boot/Image  \      
	-device virtio-blk-device,drive=hd0 \    
	-append "root=/dev/vda ro console=ttyS0"   \      
	-bios default -drive file=rootfs.ext4,format=raw,id=hd0 \        
	-netdev user,id=net0 -device virtio-net-device,netdev=net0 \    
	-S \    
	-s 
# -S: 表示启动时暂停执行，这样我们可以在 GDB 连接后再开始执行 
# -s: -gdb tcp::1234 的缩写，会开启一个 tcp 服务，端口为 1234，可以使用 GDB 连接并进行调试
``` 

上述命令由于 `-S` 的原因，执行后会直接停止，表现为没有任何反应。接下来**再打开一个终端，进入同一个 Docker 容器**，并切换到 `lab0` 目录，使用 GDB 进行调试。

``` Shell
# 使用 GDB 进行调试
$ riscv64-unknown-linux-gnu-gdb build/linux/vmlinux
```

+ GDB调试：
1. 命令及其含义：
``` Shell
# 远程调试,1234 是上述 QEMU 执行时指定的用于调试连接的端口号
(gdb) target remote localhost:1234 
# 在 start_kernel 处打断点
(gdb) b start_kernel 
# 在 地址0x80000000处设置断点
(gdb) b *0x80000000
# 在 地址0x80200000处设置断点
(gdb) b *0x80200000
# 查看全部断点
(gdb) info breakpoints
# 删除断点2
(gdb) delete 2
# 查看全部断点
(gdb) info breakpoints
```
+ 执行结果![[0_10.png]]

  2. 命令及其含义：
``` Shell
# 继续执行直至下一断点
(gdb) continue
# 删除断点3
(gdb) delete 3
# 继续执行至下一断点
(gdb) continue
# 单步执行
(gdb) step
# 单步执行，与step相同
(gdb) s
# 执行上一条命令，这里的上一条为s，故单步执行
(gdb) (不做输入，直接回车)
# 单步执行，但是跳过函数内部的详细执行
(gdb) next
# 与next相同
(gdb) n
# 执行上一条，即n
(gdb) (不做输入，直接回车)
```
+ 执行结果：![[0_11.png]]

3. 命令及其含义
```Shell
# 反汇编正在执行的代码块。展示汇编指令
(gdb) disassemble
# 单步执行一个机器指令
(gdb) nexti
# next
(gdb) n
# 单步执行机器指令，执行函数内部的详细内容
(gdb) stepi
# step
(gdb) s
```

- 执行结果：![[0_14.png]]![[0_12.png]]
- **请回答：nexti 和 next 的区别在哪里？stepi 和 step 的区别在哪里？next 和 step 的区别是什么？**

	答：
	1. `nexti`与`next`的区别：前者单步执行机器指令，后者单步执行源码
	2. `stepi`与`step`的区别：前者单步执行机器指令，后者单步执行源码
	3. `next`与`step`的区别：前者不进入函数内部详细执行，后者会进入函数详细执行

4. 命令及其含义
```Shell
(gdb) continue
# 这个地方会卡住，可以用 ctrl+c 强行中断
# 退出gdb调试
(gdb) quit
```
+ 执行结果：![[0_13.png]]

`vmlinux` 和 `Image` 的关系和区别是什么？为什么 QEMU 运行时使用的是 `Image` 而不是`vmlinux`？

提示：一个可执行文件包括哪几部分？从vmlinux到Image发生了什么？

+ 答：
	+ 一个可执行文件包含**File Header**, **Program Header Table**, **Segment**, **Symbol Table**. **Relocation Table**
	+ 在 Linux 内核编译过程中，生成的 `vmlinux` 文件包含了完整的内核代码和符号信息，是Linux内核编译产生的原始内核elf文件，由于不含boot code，不可引导Linux启动，可用于内核debug。然后通过对 `vmlinux` 进行处理(通过log可以看到`vmlinux`通过OBJCOPY转化为binary文件`image`![[0_15.png]])，去除**Symbol Table**, **Relocation Table**，添加**boot code**，生成经过裁剪并可引导Linux并启动内核的 `Image` 文件(但仍然是未压缩的)。
	+ 因此，QEMU运行时无法选用`vmlinux`，自然就选用了`Image`。
## 4 讨论和心得

+ 遇到的问题：在WSL2-Ubuntu22.04中安装docker-desktop时报错，缺少依赖且无法安装，找到社区中相似的情况并解决，以下是[链接]([apt - Cannot install docker-desktop on ubuntu 22.04 - Ask Ubuntu](https://askubuntu.com/questions/1409192/cannot-install-docker-desktop-on-ubuntu-22-04))

+ 通过此次实验，我学习了docker的基本使用，进行了docker-desktop的部署，了解了镜像的导入，容器的创建与启用。此外，我学习了如何对Linux内核进行编译以及如何使用gdb调试Linux内核。