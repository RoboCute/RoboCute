# Standard Lib used in Robocute / Robocute使用的标准库

Robocute秉承“最小重复造轮子”的原则，会尽可能吸收采纳现成的解决方案作为标准库，主要开发中

- 继承使用LuisaCompute对eastl的封装和部分拓展，eastl采用BSD 3-Clause License，
- 对于一些专用容器，采用了一些业界常见的标杆实现
  - ConcurrentQueue: 对于一个多生产者，多消费者的无锁队列，我们封装了[moodycamel ConcurrentQueue](http://moodycamel.com/blog/2014/detailed-design-of-a-lock-free-queue) 其采用Simplified BSD license

## 注意事项

### HashMap

HashMap采用LuisaCompute中的一个内部实现，支持一些特殊用法

`auto is_new_elem = resource_factories.try_emplace(type, factory).second;`

如果你不希望在已经有了元素的情况下构建新的元素，那还可以
`resource_factories.try_emplace(type, vstd::lazy_eval([&] { return make_factory(); }))`;
这样 make_factory 只有在需要添加的时候才会调用

