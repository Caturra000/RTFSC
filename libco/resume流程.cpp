void co_resume( stCoRoutine_t *co )
{
	stCoRoutineEnv_t *env = co->env;
	stCoRoutine_t *lpCurrRoutine = env->pCallStack[ env->iCallStackSize - 1 ];
  // 如果没有执行过，就没有上下文
	if( !co->cStart )
	{
    // 注册CoRoutineFunc回调
    // 可以认为是pfn的封装，额外处理yield
		coctx_make( &co->ctx,(coctx_pfn_t)CoRoutineFunc,co,0 );
		co->cStart = 1;
	}
	env->pCallStack[ env->iCallStackSize++ ] = co;
  // rsi == co
	co_swap( lpCurrRoutine, co );


}

int coctx_make(coctx_t* ctx, coctx_pfn_t pfn, const void* s, const void* s1) {
  // 找到自定义stack最后面的一个存放sp的地方
  // 首次使用时*sp为全0（stack经过memset处理）
  char* sp = ctx->ss_sp + ctx->ss_size - sizeof(void*);
  // 处理对齐，x64 system v abi要求函数调用时rsp能整除16
  // 更多参考：https://stackoverflow.com/questions/38335212/calling-printf-in-x86-64-using-gnu-assembler
  sp = (char*)((unsigned long)sp & -16LL);

  memset(ctx->regs, 0, sizeof(ctx->regs));
  void** ret_addr = (void**)(sp);
  // *sp存放pfn回调
  *ret_addr = (void*)pfn;

  // 用于替换sp
  ctx->regs[kRSP] = sp;

  ctx->regs[kRETAddr] = (char*)pfn;

  // 只考虑2个传参
  ctx->regs[kRDI] = (char*)s;
  ctx->regs[kRSI] = (char*)s1;
  return 0;
}

typedef void* (*coctx_pfn_t)( void* s, void* s2 );

static int CoRoutineFunc( stCoRoutine_t *co,void * )
{
	if( co->pfn )
	{
		co->pfn( co->arg );
	}
	co->cEnd = 1;

	stCoRoutineEnv_t *env = co->env;

	co_yield_env( env );

	return 0;
}
