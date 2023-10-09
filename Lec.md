# Process Concept
+ job / process
+ task (比process更小)
1. A process includes:
	1. text section
	2. program
	3. stack(function parameter, local variables, return address)
	4. datasection
	5. heap(dynamic)
2. Process in memory
	1. 从上至下(max to 0)： stack(向下)， heap(向上)，data， text
3. Process state
	1. new 
	2. running 
	3. waiting:等待被执行
	4. ready
	5. terminated
4. Process Control Block(PCB)


## Thread v.s. Process
+ 线程用户态，进程需要系统调度

### Kernel Thread
+ supported by kernel

### Multithreading models
+ 挂钩模式
	+ many-to-one
		+ 一个kernel thread对应很多user thread的调度
	+ one-to-one
		+ 一个kernel thread对应一个user thread
	+ many-to-many
		+ vice versa


### Thread cancellation
+ Asynchronous
+ defered

### Signal Handling



### Thread Pool


### Light-Weight Process
+ 不是进程，只是一种机制


# CPU Scheduler

1. switches from running to waiting state
2. switches from running to ready state
2. 


## long term


## short term
