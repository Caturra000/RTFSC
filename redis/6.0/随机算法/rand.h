#ifndef REDIS_RANDOM_H
#define REDIS_RANDOM_H

// 返回一个随机数
int32_t redisLrand48();

// 初始化
void redisSrand48(int32_t seedval);

// 内部实现参考next()

#define REDIS_LRAND48_MAX INT32_MAX

#endif