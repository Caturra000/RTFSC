#include <stdint.h>

#define N	16
// N位以下全1
#define MASK	((1 << (N - 1)) + (1 << (N - 1)) - 1) 
#define LOW(x)	((unsigned)(x) & MASK)
#define HIGH(x)	LOW((x) >> N)
// z[2] = x*y，分别存储高低16位
#define MUL(x, y, z)	{ int32_t l = (long)(x) * (long)(y); \
		(z)[0] = LOW(l); (z)[1] = HIGH(l); }
// bool 是否可借位
#define CARRY(x, y)	((int32_t)(x) + (long)(y) > MASK)
#define ADDEQU(x, y, z)	(z = CARRY(x, (y)), x = LOW(x + (y)))

#define X0	0x330E
#define X1	0xABCD
#define X2	0x1234
#define A0	0xE66D
#define A1	0xDEEC
#define A2	0x5
#define C	0xB

#define SET3(x, x0, x1, x2)	((x)[0] = (x0), (x)[1] = (x1), (x)[2] = (x2))
#define SETLOW(x, y, n) SET3(x, LOW((y)[n]), LOW((y)[(n)+1]), LOW((y)[(n)+2]))
// x a c初始化
#define SEED(x0, x1, x2) (SET3(x, x0, x1, x2), SET3(a, A0, A1, A2), c = C)
#define REST(v)	for (i = 0; i < 3; i++) { xsubi[i] = x[i]; x[i] = temp[i]; } \
		return (v);
#define HI_BIT	(1L << (2 * N - 1))

static uint32_t x[3] = { X0, X1, X2 }, a[3] = { A0, A1, A2 }, c = C;
static void next(void);

int32_t redisLrand48() {
    next();
    return (((int32_t)x[2] << (N - 1)) + (x[1] >> 1));
}

void redisSrand48(int32_t seedval) {
    SEED(X0, LOW(seedval), HIGH(seedval));
}

static void next(void) {
    uint32_t p[2], q[2], r[2], carry0, carry1;

    MUL(a[0], x[0], p);
    ADDEQU(p[0], c, carry0);
    ADDEQU(p[1], carry0, carry1);
    MUL(a[0], x[1], q);
    ADDEQU(p[1], q[0], carry0);
    MUL(a[1], x[0], r);
    x[2] = LOW(carry0 + carry1 + CARRY(p[1], r[0]) + q[1] + r[1] +
            a[0] * x[2] + a[1] * x[1] + a[2] * x[0]);
    x[1] = LOW(p[1] + r[0]);
    x[0] = LOW(p[0]);
}