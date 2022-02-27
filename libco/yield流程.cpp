void co_yield( stCoRoutine_t *co )
{
	co_yield_env( co->env );
}

void co_yield_env( stCoRoutineEnv_t *env )
{
	// 这里并没有任何越界保护
	stCoRoutine_t *last = env->pCallStack[ env->iCallStackSize - 2 ];
	stCoRoutine_t *curr = env->pCallStack[ env->iCallStackSize - 1 ];

	env->iCallStackSize--;

	co_swap( curr, last);
}