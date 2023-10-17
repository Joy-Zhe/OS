# Lab 1: RV64 内核引导与时钟中断

<a name="ca927a96"></a>
## 1 实验目的

学习 RISC-V 相关知识，了解 OpenSBI 平台，实现 sbi 调用函数，封装打印函数，并利用 Makefile 来完成对整个工程的管理。

<a name="1bdb1875"></a>
## 2 实验内容及要求


- 阅读 RISC-V 中文手册，学习 RISC-V 相关知识
- 学习 Makefile 编写规则，补充 Makefile 文件使得项目成功运行
- 了解 OpenSBI 的运行原理，编写代码通过 sbi 调用实现字符串的打印
- 利用 OpenSBI 平台的 SBI 调用触发时钟中断，并通过代码设计实现定时触发时钟中断的效果

## 3 实验步骤

### 3.1 搭建实验环境

本实验提供的代码框架结构如图，你可以点击 [lab1.zip](https://yuque.zju.edu.cn/attachments/yuque/0/2023/zip/25434/1695295380121-56e9a949-2b1c-4b55-a6b1-a54ee76b7990.zip?_lake_card=%7B%22uid%22%3A%221695295379694-0%22%2C%22src%22%3A%22https%3A%2F%2Fyuque.zju.edu.cn%2Fattachments%2Fyuque%2F0%2F2023%2Fzip%2F25434%2F1695295380121-56e9a949-2b1c-4b55-a6b1-a54ee76b7990.zip%22%2C%22name%22%3A%22lab1.zip%22%2C%22size%22%3A9988%2C%22type%22%3A%22application%2Fzip%22%2C%22ext%22%3A%22zip%22%2C%22progress%22%3A%7B%22percent%22%3A99%7D%2C%22status%22%3A%22done%22%2C%22percent%22%3A0%2C%22id%22%3A%22Jl3AQ%22%2C%22card%22%3A%22file%22%7D) 进行下载。首先，请下载相关代码，并移动至你所建立的本地映射文件夹中（即`lab0`中创建的`os_experiment`文件夹）。

```shell
Lab1
├──  arch
│  └──  riscv
│     ├──  boot
│     ├──  kernel
│     │  ├──  clock.c
│     │  ├──  entry.S
│     │  ├──  head.S
│     │  ├──  init.c
│     │  ├──  main.c
│     │  ├──  Makefile
│     │  ├──  print.c
│     │  ├──  sbi.c
│     │  ├──  trap.c
│     │  └──  vmlinux.lds
│     └──  Makefile
├──  include
│  ├──  defs.h
│  ├──  riscv.h
│  └──  test.h
└──  Makefile
```


<a name="8fc9a2a2"></a>
### 3.2 了解项目框架，编写 MakeFile（10%）


<a name="314f2fbf"></a>
#### 3.2.1 编写 Makefile 文件


1. 请参考[【附录A.Makefile介绍】](#220c9420)学习 Makefile 的基本知识。
1. 阅读项目中的 Makefile 文件，确保你理解了 Makefile 文件中每一行的作用（一些参数配置等不做要求）。


<br />**注意：在 `Lab1/Makefile` 中已经帮助你预定义好了文件的 `include` 地址，编译参数等等，你再编写下面的 Makefile 的时候是需要用到的，如果不知道如何使用，可以阅读代码框架里面的其他 Makefile 文件（有参考），仔细做了解。**<br />**
> Makefile 部分是需要大家自己学习的，请务必阅读附录部分提供的教程，这里简单讲一下本项目的 Makefile 结构，最外层的 Makefile 定义了一些基础变量以供使用，包括本项目使用的编译器，编译的参数，头文件所在路径，另外定义了一些基本的编译目标，如 all，vmlinux，run，debug，其中 vmlinux 就是编译出 vmlinux 这个本项目用的程序， run 则是编译出的基础上再运行，debug 则是编译出的基础上以 debug 模式运行（即 Lab 0 中介绍的调试内核部分）在编译 vmlinux 的时候，最外层 Makefile 会跳转到内层的一些 Makefile 去执行，内层的 Makefile 文件干的事情基本就是把各文件夹的 .c 文件编译成 .o 文件。都编译好再跳转回根目录的 Makefile ，然后把所有 .o 文件编译成一个整体的可执行文件。编译链接是什么请搜索 C 编译链接详解。实验不做要求，但实验对 Makefile 有要求。 


<br />**请补充完整 **`./arch/riscv/kernel/Makefile`** 文件使得整个项目能够顺利编译，最后，将你的代码补充在下方的代码框中。**<br />
<br />**你需要确保 `make test` 指令和 `make clean` 指令均可正常运行（在**`**lab1**`**目录下），如果运行成功会显示绿色的 Success 字样提示。**<br />

```makefile
# TODO: 将本文件夹下的所有.S与.c文件编译成.o文件	
# 1. 使用wildcard获取所有.S与.c文件（TODO）
ASM_SRC= $(sort $(wildcard *.S))
C_SRC= $(sort $(wildcard *.c))

# 2. 使用patsubst将.S与.c文件转换成.o文件（TODO）
ASM_OBJ= $(patsubst %.S,%.o,$(ASM_SRC))
C_OBJ= $(patsubst %.c,%.o,$(C_SRC))

# 3. 定义目标文件all与依赖关系。执行make命令时可以指定编译目标，比如make all，make clean，然后make会寻找该编译目标，并根据make的运行机制进行编译。（TODO）
all: $(ASM_OBJ) $(C_OBJ)

# 4. 使用%.o:%.S与%.o:%.c定义依赖关系与命令（Done）
%.o:%.S
	${CC}  ${CFLAG}  -c $<
%.o:%.c
	${CC}  ${CFLAG}  -c $<

# 请自行查询makefile相关文档，包括makefile函数、变量、通配符，make的运行机制等



clean:
	$(shell rm *.o 2>/dev/null)
```

+ `make test` 与 `make clean` 均可成功执行
![1.png](./img/Lab1/1.png)
<a name="98a9a348"></a>
#### 3.2.2 解释 Makefile 命令

**请解释** `lab1/Makefile`中的下列命令：

```makefile
line 25: ${MAKE} -C arch/riscv all
```


> all 不是全部的意思，而是makefile文件里定义的目标

含义：
1. `${MAKE}`: 调用make命令
2. `-C arch/riscv`: 指定`arch/riscv`作为被指定的目录
3. `all`: 指定目标为对应Makefile中的`all`
+ 该命令的含义为对`arch/riscv`中`all`指定的依赖文件执行make

<br />**请解释** **`lab1/arch/riscv/kernel/Makefile`** 中的下列命令：<br />

```makefile
line 16: %.o:%.c
	${CC}  ${CFLAG}  -c $<
```

<br />含义：
1. `%.o:%.c`: 规则头部，所有依赖文件`.c`如何生成`.o`
2. `${CC}`: 选择编译器
3. `${CFLAG}`: 编译选项参数
4. `-c`: 参数-c表示只编译不链接
5. `$<`: 自动变量，表示规则中的依赖项，执行时会被替换为对应文件的文件名
+ 该命令是一个静态模式规则，表示了如何从`.c`文件生成`.o`文件，即将`.c`编译生成`.o`
<br />

<a name="3f804697"></a>
### 3.3 学习 RISC-V 相关知识及特权架构（5%）

<br />后续实验中将持续使用 RISC-V 指令集相关的内容，请参考[【附录B.RISC-V指令集】](#dfbc74cf)了解相关知识，**下载并阅读 [RISC-V 手册](https://yuque.zju.edu.cn/os/lab/hrw3u8#MVG2f)，掌握基础知识、基本命令及特权架构相关内容。**<br />

<a name="90be974c"></a>
#### 3.3.1 基础命令掌握情况

<br />请按照下面提供的例子，补充完整各指令含义。<br />

```makefile
# 加载立即数 0x40000 到 t0 寄存器中
li t0,0x40000

# 设置satp寄存器的值为t0寄存器的值
csrw satp, t0

# t0 = t0 - t1
sub t0, t0, t1

# 将x1寄存器中的值存储到地址为sp+8的内存
sd x1, 8(sp)

# 将sp设置为stack_top
la sp, stack_top
```


<a name="zBAgL"></a>
#### 3.3.2 对risc-v特权模式的理解

<br />**请解释risc-v特权模式存在的意义：** RISC-V特权模式的存在是为了实现不同的CPU执行级别和特权级别，以确保系统的安全性、稳定性和隔离

假设我们有一个实现了U态和M态的risc-v架构CPU，并在其上运行os与用户程序，**请解释CPU如何在U态与M态之间进行切换（言之有理即可）：** __________________________________________（提示：从U态到M态是“谁操作”的，是硬件还是软件，从M态到U态呢？）
+ 从U态到M态的切换是由软件触发，通常涉及异常处理，而从M态到U态的切换是由硬件触发，通常是在中断或异常处理完成后

<a name="004b7487"></a>
### 3.4 通过 OpenSBI 接口实现字符串打印函数（30%）


<a name="16c70b43"></a>
#### 3.4.1 程序执行流介绍

<br />对于本次实验，我们选择使用 OpenSBI 作为 bios，来进行机器启动时的硬件初始化与寄存器设置（此时机器处于M态），并使用 OpenSBI 所提供的接口完成诸如字符打印等操作。<br />
<br />请参考[【附录C.OpenSBI介绍】](#7iLzZ)了解 OpenSBI 平台的功能及启动方式，参考[【附录E. Linux Basic】](#0fa8908f)了解`vmlinux.lds`、`vmlinux` 的作用，理解执行 `make run` 命令时程序的执行过程。<br />

```shell
# make run 依赖 vmlinux
# 因此，他首先会编译目标 vmlinux 然后执行 lab1/Makefile 中的该行命令
@qemu-system-riscv64 -nographic --machine virt -bios default -device loader,file=vmlinux,addr=0x80200000 -D log
```

<br />QEMU 模拟器完成从 ZSBL 到 OpenSBI 阶段的工作，本行指令使用 -bios default 选项将 OpenSBI 代码加载到 0x80000000 起始处。QEMU完成一部分初始化工作后（例如Power-On Self-Test 和 Initialization and Boot ROM），将会跳转到 0x80000000 处开始执行。在 OpenSBI 初始化完成后，将跳转到 0x80200000 处继续执行。因此，我们还需要将自己编译出的 `vmlinux` 程序加载至地址 0x80200000 处。<br />
<br />`vmlinux.lds` 链接脚本就可以帮助我们完成这件事情。它指定了程序的内存布局，最先加载的 `.text.init` 段代码为 `head.S` 文件的内容，该部分代码会执行调用 `main()` 函数。`main()` 函数调用了打印函数，打印函数通过 `sbi_call()` 向 OpenSBI 发起调用，完成字符的打印。<br />

<a name="ee28d148"></a>
#### 3.4.2 编写 sbi_call() 函数（10%）

<br />当系统处于 m 模式时，对指定地址进行写操作便可实现字符的输出。但我们编写的内核运行在 s 模式**（因为我们使用了 OpenSBI 帮助我们初始化，OpenSBI会在执行内核代码之前先进入S态）**，需要使用OpenSBI 提供的接口，让运行在 m 模式的 OpenSBI 帮助我们实现输出。即运行在 s 模式的内核通过调用 `ecall`  指令（汇编级指令）发起 sbi 调用请求，触发中断，接下来 RISC-V CPU 会从 s 态跳转到 m 态的 OpenSBI 固件中。<br />
<br />执行 `ecall` 时需要指定 sbi 调用的编号，传递的参数。一般而言：

- `a6` 寄存器存放 SBI 调用 `Function ID` 编号
- `a7` 寄存器存放 SBI 调用 `Extension ID` 编号
- `a0`、`a1` 、`a2` 、`a3` 、`a4` 、`a5` 寄存器存放 SBI 的调用参数，不同的函数对于传递参数要求也不同。


<br />简单来讲，你可以认为我们需要填好 `a0` 到 `a7` 这些寄存器的值，调用 `ecall` 后，OpenSBI 会根据这些值做相应的处理。以下是一些常用的函数表。<br />


| Function Name | Function ID | Extension ID |
| :--- | :--- | :--- |
| sbi_set_timer （设置时钟相关寄存器） | 0 | 0x00 |
| sbi_console_putchar （打印字符） | 0 | 0x01 |
| sbi_console_getchar （接收字符） | 0 | 0x02 |
| sbi_shutdown （关机） | 0 | 0x08 |


<br />你需要编写内联汇编语句以使用 OpenSBI 接口，本实验给出的函数定义如下：（注意：本实验是 64 位 riscv 程序，这意味着我们使用的寄存器都是 64 位寄存器）<br />

```c
typedef unsigned long long uint64_t;
struct sbiret {
  uint64_t error;
  uint64_t value;
};

struct sbiret sbi_call(uint64_t ext, uint64_t fid, uint64_t arg0, uint64_t arg1,
                       uint64_t arg2, uint64_t arg3, uint64_t arg4,
                       uint64_t arg5);
```

<br />在该函数中，你需要完成以下内容：<br />

- 将 ext (Extension ID) 放入寄存器 a7 中，fid (Function ID) 放入寄存器 a6 中，将 arg0 ~ arg5 放入寄存器 a0 ~ a5 中。
- 使用 ecall 指令。ecall 之后系统会进入 M 模式，之后 OpenSBI 会完成相关操作。
- OpenSBI 的返回结果会存放在寄存器 a0 ， a1 中，其中 a0 为 error code， a1 为返回值， 我们用 sbiret  结构来接受这两个返回值。


<br />请参考[【附录C.内联汇编】](#7iLzZ)相关知识，以内联汇编形式实现 `lab1/arch/riscv/kernel/sbi.c` 中的 `sbi_call()` 函数。<br />**<br />**注意：如果你在内联汇编中直接用到了某寄存器（比如本函数必然要直接使用 a0~a7 寄存器），那么你需要在内联汇编中指出，本段代码会影响该寄存器，如何指出请参考**[**【附录D】**](#8dc79ae2)**，如果不加声明，编译器可能会将你声明的要放到寄存器里的变量，放到你直接使用的寄存器内，可能引发意想不到的错误**。<br />
<br />最后，请将你编写好的 `sbi_call` 函数复制到下面代码框内。<br />

```c
// lab1/arch/riscv/kernel/sbi.c

#include "defs.h"

struct sbiret sbi_call(uint64_t ext, uint64_t fid, uint64_t arg0, uint64_t arg1,
                       uint64_t arg2, uint64_t arg3, uint64_t arg4,
                       uint64_t arg5) {
  struct sbiret ret;
  __asm__ volatile(

    "addi a7, %[ext], 0\n"

    "addi a6, %[fid], 0\n"

    "addi a0, %[arg0], 0\n"

    "addi a1, %[arg1], 0\n"

    "addi a2, %[arg2], 0\n"

    "addi a3, %[arg3], 0\n"

    "addi a4, %[arg4], 0\n"

    "addi a5, %[arg5], 0\n"

    "ecall\n"

    "addi %[error], a0, 0\n"

    "addi %[ret_val], a1, 0\n"

    : [error] "=r"(ret.error) , [ret_val] "=r"(ret.value)

    : [ext] "r"(ext), [fid] "r"(fid), [arg0] "r"(arg0), [arg1] "r"(arg1), [arg2] "r"(arg2), [arg3] "r"(arg3), [arg4] "r"(arg4), [arg5] "r"(arg5)

    : "memory", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"

  );
  return ret;
}

```


> 可以自己在makefile中添加命令把sbi.c编译成汇编代码（使用gcc的-S选项），看一下sbi_call在汇编层面是如何运行的，有没有优化空间。

<a name="wzBHx"></a>
#### 
<a name="tiezL"></a>
#### 3.4.3 编写字符串打印函数（20%）

<br />现在你已经有了一个 C 语言层面的 `sbi_call` 接口函数，因此，后面的代码中，你只需要调用这个接口函数即可，并不需要再写汇编代码。<br />
<br />本节，你需要在 `./arch/riscv/kernel/print.c` 文件中通过调用 `sbi_call()` 实现字符串打印函数 `int puts(char* str)` 及数字打印函数 `int put_num(uint64_t n)`，后者可将数字转换为字符串后调用前者执行。（注意处理边界 n = 0 的情况）<br />
<br />提示：上节已经给出了你一个 OpenSBI 调用函数表，具体使用方法可参考[OpenSBI 文档](https://github.com/riscv/riscv-sbi-doc/blob/master/riscv-sbi.adoc#function-listing-1)。为了利用 OpenSBI 接口打印字符，我们需要向 `sbi_call()` 函数传入 `ext=1, fid=0` 以调用 `sbi_console_putchar(int ch)` 函数，之后，第一个参数 `arg0` 需要传入待打印字符的 ASCII 码，其余没有用到的参数可直接设为0。<br />
<br />最后，请将你编写好的函数复制到下面代码框内。<br />

```c
// ./arch/riscv/libs/print.c

#include "defs.h"
extern struct sbiret sbi_call(uint64_t ext, uint64_t fid, uint64_t arg0,
                              uint64_t arg1, uint64_t arg2, uint64_t arg3,
                              uint64_t arg4, uint64_t arg5);

int puts(char* str){
	while (*str != '\0')
	{
		sbi_call(1, 0, *str, 0, 0, 0, 0, 0);
		str++;
	}
    return 0;
}

int put_num(uint64_t n){
	char str[20];
	int i = 0;
	while (n != 0) 
	{
		str[i] = n % 10 + '0';
		n /= 10;
		i++;
	}
	str[i] = '\0';
	for (int j = i - 1; j >= 0; j--)
	{
		sbi_call(1, 0, str[j], 0, 0, 0, 0, 0);
	}
	return 0;
}
```


<a name="KsHlh"></a>
#### 3.4.4 修改链接脚本文件

<br />裸机程序从 `.text` 段起始位置执行，所以需要利用 `vmlinux.lds` 中 `.text` 段的定义来确保 `head.S` 中的 `.text` 段被放置在其他 `.text` 段之前。这可以通过重命名来解决。<br />
<br />首先将 `head.S` 中的 `.text` 命名为 `.text.init`：<br />

```asm
<<<<< before
.section .text
============
.section .text.init
>>>>> after
```

<br />接下来将 `entry.S` 中的 `.text` 命名为 `.text.entry`：<br />

```asm
<<<<< before
.section .text
============
.section .text.entry
>>>>> after
```

<br />然后修改 `vmlinux.lds` 文件中的 `.text` 展开方式：<br />

```asm
<<<<< before 
.text : {
		*(.text)
		*(.text.*)
	 }
============
.text : {
		*(.text.init)
		*(.text.entry)
		*(.text)
		*(.text.*)
	 }
>>>>> after
```

<br />**如果你没有理解这段代码为什么这样修改，请阅读**[**【附录 E: Linux Basic】**](#0fa8908f)**部分的说明。**<br />

<a name="1LvGA"></a>
#### 3.4.5 编译运行

<br />在`lab1`目录下执行`make print_only && make run`，如果输出`"2023 Hello Oslab"`则实验成功。<br />
+ 可以看到正确输出了结果
![](./img/Lab1/2.png)

<a name="meo5E"></a>
### 3.5 实现时钟中断（55%）


<a name="nZk9B"></a>
#### 3.5.1 实现逻辑

<br />本实验的目标是**定时触发时钟中断并在相应的中断处理函数中输出相关信息**。<br />
<br />代码实现逻辑如下：<br />

- 在初始化阶段，设置 CSR 寄存器以允许 S 模式的时钟中断发生，利用 SBI 调用触发第一次时钟中断。
- SBI 调用触发时钟中断后，OpenSBI 平台自动完成 M 模式时钟中断处理，并触发 S 模式下的时钟中断，接下来会进入程序设置好的（汇编级）中断函数中。中断函数保存寄存器现场后会调用（C 语言级）中断处理函数。
- 在中断处理函数中打印相关信息，并设置下一次时钟中断，从而实现定时（每隔一秒执行一次）触发时钟中断并打印相关信息的效果。函数返回后恢复寄存器现场，调用 S 模式异常返回指令 sret 回到发生中断的指令。


<br />对应到程序流程中：

1. 各类 init 函数：允许中断，开启时钟中断，设置第一次时钟中断，设置中断处理函数的地址。
1. trap_s 函数：保存寄存器，进入 handler_s 函数处理中断
1. handler_s 函数：判断是否是时钟中断，是就设置下一次时钟中断并输出信息。
1. 下一次时钟中断触发，再次回到 2


<br />为了完成实验，需要同学们在 `init.c` 中设置 CSR 寄存器允许时钟中断发生，在 `clock.c` 中写设置时钟中断开启和下一次时钟中断发生时间的函数，最后在 `entry.S` 及 `trap.c` 中编写中断处理函数。<br />**
<a name="kPrdc"></a>
#### 3.5.2 编写 init.c 中的相关函数（15%）

<br />在qemu完成初始化并进入os时，默认处于S态，接下来我们将在S态实现时钟中断。<br />
<br />首先，我们需要开启 S 模式下的中断总开关，需要对以下寄存器进行设置：<br />

1. 设置 `stvec` 寄存器。`stvec` 寄存器中存储着 S 模式下发生中断时跳转的地址，我们需要编写相关的中断处理函数，并将地址存入 `stvec` 中。
1. 将 `sstatus` 寄存器中的 `sie` 位打开。`sstatus[sie]` 位为 S 模式下的中断总开关，这一位为 1 时，才能响应中断。


<br />**编写 `intr_enable()`/`intr_disable()`：**<br />**<br />这两个函数的作用如下注释。你需要使用 CSR 命令设置 `sstatus[sie]` 的值。本实验中在 `riscv.h` 文件中为你提供了一些宏定义，可以方便的使用 CSR 指令。当然，你也可以自行通过内联汇编实现。<br />

> 提示：你需要根据 [RISC-V中文手册](https://yuque.zju.edu.cn/os/lab/hrw3u8#omrh2)** 中的【第十章 RV32/64 特权架构】**中的内容确定 sstatus 寄存器的 sie 位是第几位，从而为该位赋 1。

![[Pasted image 20231005145719.png]]

> **另一个教程：CSR 寄存器各位含义见 **[特权架构](https://www.cnblogs.com/dream397/p/15687184.html)。



> `write_csr(a, 2)` 相当于 a = 2
> `set_csr(a, 2)` 相当于 a |= 2
> `clear_csr(a, 2)` 相当于 a &= (~2)


<br />**请在下方代码框中补充完整你的代码：**<br />

```c
void intr_enable(void) {
  // 设置 sstatus[sie] = 1, 打开 s 模式的中断开关
  // your code
    set_csr(sstatus, 0x0002);
}

void intr_disable(void) {
  // 设置 sstatus[sie] = 0, 关闭 s 模式的中断开关
  // your code
    clear_csr(sstatus, 0xfffd);
}
```

<br />**请对你的代码做简要解释：**<br />答：_`set_csr`通过按位或实现，将sie位置1即可，其他位保持0，不影响其他位的原有值；`clear_csr`通过按位与实现，其他位保持1，sie位与0取或_<br />
<br />**编写 `idt_init()`：**<br />**<br />该函数需要你向 `stvec` 寄存器中写入中断处理后跳转函数的地址，在本实验中，我们的中断处理函数是 `trap_s` 这个函数。<br />

> 提示：C 语言中，可以使用取地址符和函数名，获取函数的地址。


<br />**请在下方代码框中补充完整你的代码：<br />**<br />

```c
void idt_init(void) {
  extern void trap_s(void);
  // 向 stvec 寄存器中写入中断处理后跳转函数的地址
  // your code
    set_csr(stvec, &trap_s);
}
```


<a name="WURfq"></a>
#### 3.5.3 编写 `clock.c` 中的相关函数（20%）

<br />我们的时钟中断需要利用 OpenSBI 提供的 `sbi_set_timer()` 接口触发，**向该函数传入一个时刻，OpenSBI 在那个时刻将会触发一次时钟中断**。<br />
<br />我们需要“**每隔若干时间就发生一次时钟中断**”，但是 OpenSBI 提供的接口一次只能设置一个时钟中断事件。本实验采用的方式是：一开始设置一个时钟中断，之后每次发生时钟中断的时候，在相应的中断处理函数中设置下一次的时钟中断。这样就达到了每隔一段时间发生一次时钟中断的目的。<br />
<br />对于代码而言，在文件 `clock.c` 中：

- `clock_init()` 函数将会启用时钟中断并设置第一个时钟中断
- `clock_set_next_event()` 用于调用 OpenSBI 函数 `set_sbi_timer()` 设置下一次的时钟中断时间。
- `get_cycles()` 函数是已经为你提供好的函数。其通过 `rdtime` 伪指令读取一个叫做 `mtime` 的 CSR 寄存器数值，表示 CPU 启动之后经过的真实时间。


<br />**修改时钟中断间隔：**<br />
<br />QEMU 中外设晶振的频率为 10mhz，即每秒钟 `time` 的值将会增大 ![](https://yuque.zju.edu.cn/images/yuque/__latex/8ec8f1234e57f139a068e89eb3b2e5fa.svg#card=math&code=10%5E7%0A&height=19&width=24)。我们可以据此来计算每次 `time` 的增加量，以控制时钟中断的间隔。**<br />
<br />为了使得每次时钟中断的间隔为 1 秒，`timebase`（即`time`的增加量）需要设置为？<br />答：________________$10^7$_____________________<br />
<br />**编写 `clock_init()`：（10%）**<br />**<br />**请根据注释在下方代码框中补充完整你的代码：**<br />

```c
void clock_init(void) {
  puts("ZJU OS LAB 2       Student_ID:123456\n");

  // 对 sie 寄存器中的时钟中断位设置（ sie[stie] = 1 ）以启用时钟中断。
  // 设置第一个时钟中断。
  // your code
    set_csr(sie, 1 << 5);
}
```

<br />**编写 `clock_set_next_event()`：（10%）**<br />**
> 提示：你需要调用 OpenSBI 提供的接口 `sbi_set_timer()` ，你需要通过 Lab 1 中编写好的 sbi_call 函数调用他。该函数对应的 Function ID 为 0，Extension ID 也为 0，接收一个参数 (arg0)，表示触发时钟中断的时间点。


<br />**请根据注释在下方代码框中补充完整你的代码：**<br />

```c
void clock_set_next_event(void) {
  // 获取当前 cpu cycles 数并计算下一个时钟中断的发生时刻
  // 通过调用 OpenSBI 提供的函数设置下一次的时钟中断时间
  // your code
  int numCycles = rdtime();

  int nextTime = numCycles + timebase;

  sbi_call(0, 0, nextTime);
}

```


<a name="xE0ni"></a>
#### 3.5.5 编写并调用中断处理函数（20%）

<br />**在 `entry.S` 中编写中断处理函数：（10%）**<br />
<br />在【3.5.3】中，我们向 `stvec` 寄存器存入了中断处理函数的地址，中断发生后将自动进行硬件状态转换，程序将读取 `stvec` 的地址并进行跳转，运行 `trap_s` 函数。该函数该函数需要在栈中保存 `caller saved register` 及 `sepc` 寄存器，读取 `scause` 这个 CSR 寄存器并作为参数传递给 `handle_s` 函数进行中断处理，调用返回后需要恢复寄存器并使用 `sret` 命令回到发生中断的指令。<br />

> **提示**：你可以参考 [RISC-V中文手册](https://yuque.zju.edu.cn/os/lab/hrw3u8#omrh2) 3.2 节相关内容完成实验；本实验中寄存器大小为 8 字节；需要使用 CSR 命令操作 CSR 寄存器；若不清楚 `caller saved register` ，也可将寄存器全都保存；对汇编语言不是特别了解的建议把中文手册读一遍，或在网上自行学习汇编语言基本的函数调用约定知识。



> 调用 handler_s 函数，如何传参数？ 给 `a0` 寄存器存入 scause 的值即可。如果你不知道为什么 `a0` 寄存器存储的是这个参数的话，请参考 [RISC-V中文手册](https://yuque.zju.edu.cn/os/lab/hrw3u8#omrh2) 第 3.2 节。（简单来说，一般规定 a 开头寄存器用来做参数传递，编译的时候也遵守了这个规则）


<br />**请根据注释在下方代码框中补充完整你的代码：**<br />

```c
trap_s:
	# save caller-saved registers and sepc
	addi sp, sp, -256
    sd x1, 0(sp)
    sd x2, 8(sp)
    sd x3, 16(sp)
    sd x4, 24(sp)
    sd x5, 32(sp)
    sd x6, 40(sp)
    sd x7, 48(sp)
    sd x8, 56(sp)
    sd x9, 64(sp)
    sd x10, 72(sp)
    sd x11, 80(sp)
    sd x12, 88(sp)
    sd x13, 96(sp)
    sd x14, 104(sp)
    sd x15, 112(sp)
    sd x16, 120(sp)
    sd x17, 128(sp)
    sd x18, 136(sp)
    sd x19, 144(sp)
    sd x20, 152(sp)
    sd x21, 160(sp)
    sd x22, 168(sp)
    sd x23, 176(sp)
    sd x24, 184(sp)
    sd x25, 192(sp)
    sd x26, 200(sp)
    sd x27, 208(sp)
    sd x28, 216(sp)
    sd x29, 224(sp)
    sd x30, 232(sp)
    sd x31, 240(sp)
    csrr t0, sepc
    sd t0, 248(sp)

	# call handler_s(scause)
	csrr a0, scause
	call handler_s
	
	# load sepc and caller-saved registers
	ld t0, 248(sp)
    csrw sepc, t0
    ld x1, 0(sp)
    ld x3, 16(sp)
    ld x4, 24(sp)
    ld x5, 32(sp)
    ld x6, 40(sp)
    ld x7, 48(sp)
    ld x8, 56(sp)
    ld x9, 64(sp)
    ld x10, 72(sp)
    ld x11, 80(sp)
    ld x12, 88(sp)
    ld x13, 96(sp)
    ld x14, 104(sp)
    ld x15, 112(sp)
    ld x16, 120(sp)
    ld x17, 128(sp)
    ld x18, 136(sp)
    ld x19, 144(sp)
    ld x20, 152(sp)
    ld x21, 160(sp)
    ld x22, 168(sp)
    ld x23, 176(sp)
    ld x24, 184(sp)
    ld x25, 192(sp)
    ld x26, 200(sp)
    ld x27, 208(sp)
    ld x28, 216(sp)
    ld x29, 224(sp)
    ld x30, 232(sp)
    ld x31, 240(sp)
    ld x2, 8(sp)
    addi sp, sp, 256
	sret
```

<br />**为什么需要保存 sepc 寄存器：**<br />答：____执行完中断程序还要回到原来程序被中断的位置继续执行_______<br />

<a name="81f49a88"></a>
#### 3.4.2 在 `trap.c` 中编写中断处理函数（10%）

<br />正常情况下，异常处理函数需要根据 `[m|s]cause` 寄存器的值判断异常的种类后分别处理不同类型的异常，但在本次实验中简化为只判断并处理时钟中断。<br />
<br />【3.5.4】中提到，为了实现"定时触发中断"，我们需要在该函数中继续设置下一次的时钟中断。此外，为了进行测试，中断处理函数中还需要打印时钟中断发生的次数，你需要在 `clock.c` 中利用 `ticks` 变量进行统计，请更新【3.5.4】中相关代码，每设置一次时钟中断，便给 `ticks` 变量加一。<br />
<br />本函数的流程如下：

- 判断是否是中断（可能是中断，可能是异常）
- 判断是否是时钟中断 **(注意 C 语言中的运算符优先级，若使用位运算请加括号)**
   - 如果是
      - 设置下一次时钟中断的时间点
      - 打印已经触发过的中断次数


<br />**触发时钟中断时，****`scause`****寄存器的值是？据此填写代码中的条件判断语句：**<br />答：____首位为1，后四位为1101___________<br />
<br />**请根据注释在下方代码框中补充完整你的代码：**<br />

```c
void handler_s(uint64_t cause) {
  // interrupt
  if (cause & (0x8000000000000000) == 0x8000000000000000) {
    // supervisor timer interrupt
    if ((cause & (0b1101)) == 5) {
      // 设置下一个时钟中断，打印当前的中断数目。
      // your code
      clock_set_next_event();
      put_num(ticks);
      puts("\n");
    }
  }
}
```

<br />**【同步异常与中断的区别】当处理同步异常时应该在退出前给 `epc` ****寄****存器+4（一条指令的长度），当处理中断时则不需要，请解释为什么要这样做。（请阅读附录内容）**<br />答：___同步异常一般是由于出现了未定义指令，该指令需要在处理后跳过，而中断是主动触发的，触发中断时的指令仍然需要执行，故m/sepc寄存器无需+4______<br />

<a name="YLZ3g"></a>
### 3.5 编译及测试

<br />请修改 `clock.c` 中的 `ID:123456`为自己的学号，在项目最外层输入 `make run` 命令调用 Makefile 文件完成整个工程的编译及执行。<br />
<br />**如果编译失败，及时使用`make clean`命令清理文件后再重新编译。**<br />****默认的 Makefile 为你提供了 ****`****make debug****` **命令，你可以用此命令以 debug 模式启动程序，此时程序会在入口停下，你可以参照 Lab 0 的方式使用 gdb + target remote :1234 的方式连接并进行调试。**<br />
<br />预期的实验结果如下：<br />**开始时会打印 OSLAB 和 学号，之后每隔一秒触发一次时钟中断，打印一次时钟中断发生的次数**。<br />
<br />【注意：由于代码最后有死循环，所以输出完成后整个进程不会自动退出，你需要手动 Ctrl+a，x 来退出 QEMU 模拟器】<br />
<br />请在此附上你的实验结果截图。![](./img/Lab1/4.png)<br />

<a name="357da1a6"></a>
## 4 讨论和心得

<br />请在此处填写实验过程中遇到的问题及相应的解决方式。<br />
1. 在打印字符时遇到了输出速度不受timebase控制的情况，排查后发现是函数入口`entry.S`对于寄存器的保存问题，没有保存全部的`caller saved register`，导致函数调用时寄存器值出错。
<br />实验可能存在不足之处，欢迎同学们对本实验提出建议。<br />


<a name="2e307a62"></a>
