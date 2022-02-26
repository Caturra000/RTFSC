void co_resume( stCoRoutine_t *co )
{
	stCoRoutineEnv_t *env = co->env;
	stCoRoutine_t *lpCurrRoutine = env->pCallStack[ env->iCallStackSize - 1 ];
	if( !co->cStart )
	{
		coctx_make( &co->ctx,(coctx_pfn_t)CoRoutineFunc,co,0 );
		co->cStart = 1;
	}
	env->pCallStack[ env->iCallStackSize++ ] = co;
	co_swap( lpCurrRoutine, co );


}

int coctx_make(coctx_t* ctx, coctx_pfn_t pfn, const void* s, const void* s1) {
  char* sp = ctx->ss_sp + ctx->ss_size - sizeof(void*);
  sp = (char*)((unsigned long)sp & -16LL);

  memset(ctx->regs, 0, sizeof(ctx->regs));
  void** ret_addr = (void**)(sp);
  *ret_addr = (void*)pfn;

  ctx->regs[kRSP] = sp;

  ctx->regs[kRETAddr] = (char*)pfn;

  ctx->regs[kRDI] = (char*)s;
  ctx->regs[kRSI] = (char*)s1;
  return 0;
}