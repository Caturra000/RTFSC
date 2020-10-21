
    // 文件分布：frameworks/base/core/java/android/app/Activity.java

    /**
     * Launch a new activity.  You will not receive any information about when
     * the activity exits.  ...省略
     *
     * @param intent The intent to start.
     * @param options Additional options for how the Activity should be started.
     * See {@link android.content.Context#startActivity(Intent, Bundle)}
     * Context.startActivity(Intent, Bundle)} for more details.
     *
     * @throws android.content.ActivityNotFoundException
     *
     * @see #startActivity(Intent)
     * @see #startActivityForResult
     */
    @Override
    public void startActivity(Intent intent, @Nullable Bundle options) {
// SKIP START
        if (mIntent != null && mIntent.hasExtra(AutofillManager.EXTRA_RESTORE_SESSION_TOKEN)
                && mIntent.hasExtra(AutofillManager.EXTRA_RESTORE_CROSS_ACTIVITY)) {
            if (TextUtils.equals(getPackageName(),
                    intent.resolveActivity(getPackageManager()).getPackageName())) {
                // Apply Autofill restore mechanism on the started activity by startActivity()
                final IBinder token =
                        mIntent.getIBinderExtra(AutofillManager.EXTRA_RESTORE_SESSION_TOKEN);
                // Remove restore ability from current activity
                mIntent.removeExtra(AutofillManager.EXTRA_RESTORE_SESSION_TOKEN);
                mIntent.removeExtra(AutofillManager.EXTRA_RESTORE_CROSS_ACTIVITY);
                // Put restore token
                intent.putExtra(AutofillManager.EXTRA_RESTORE_SESSION_TOKEN, token);
                intent.putExtra(AutofillManager.EXTRA_RESTORE_CROSS_ACTIVITY, true);
            }
        }
// SKIP END

        if (options != null) {
            startActivityForResult(intent, -1, options);
        } else {
            // Note we want to go through this call for compatibility with
            // applications that may have overridden the method.
            startActivityForResult(intent, -1); // 内部调用 startActivityForResult(intent, -1, null);
        }
    }




       /**
        * @param intent The intent to start.
        * @param requestCode If >= 0, this code will be returned in
        *                    onActivityResult() when the activity exits.  // 前面的调用流程中requestCode < 0
        * @param options Additional options for how the Activity should be started.
        * See {@link android.content.Context#startActivity(Intent, Bundle)}
        * Context.startActivity(Intent, Bundle)} for more details.
        *
        * @throws android.content.ActivityNotFoundException
        *
        * @see #startActivity
        */
    public void startActivityForResult(@RequiresPermission Intent intent, int requestCode, 
            @Nullable Bundle options) {

            // mParent是否存在决定了activity不同的启动流程，
        if (mParent == null) {                                                               // TODO mParent
            options = transferSpringboardActivityOptions(options);
            Instrumentation.ActivityResult ar =
// TRACE START
                mInstrumentation.execStartActivity(
                    this, mMainThread.getApplicationThread(), mToken, this,
                    intent, requestCode, options);
// TRACE END
            if (ar != null) {
                mMainThread.sendActivityResult(
                    mToken, mEmbeddedID, requestCode, ar.getResultCode(),
                    ar.getResultData());
            }
// SKIP START
            if (requestCode >= 0) {
                // If this start is requesting a result, we can avoid making
                // the activity visible until the result is received.  Setting
                // this code during onCreate(Bundle savedInstanceState) or onResume() will keep the
                // activity hidden during this time, to avoid flickering.
                // This can only be done when a result is requested because
                // that guarantees we will get information back when the
                // activity is finished, no matter what happens to it.
                mStartedActivity = true;
            }
// SKIP END
            cancelInputsAndStartExitTransition(options);
            // TODO Consider clearing/flushing other event sources and events for child windows.


        } else {
            if (options != null) {
                mParent.startActivityFromChild(this, intent, requestCode, options);
            } else {
                // Note we want to go through this method for compatibility with
                // existing applications that may have overridden it.
                mParent.startActivityFromChild(this, intent, requestCode);
            }
        }
    }




////////////////////////////////////////////////////////////////////////////////////



   // 文件分布：frameworks/base/core/java/android/app/Instrumentation.java
   // From wiki: 具有启动能力用于监控其他类的工具类


    /**
     * Execute a startActivity call made by the application.  The default 
     * implementation takes care of updating any active {@link ActivityMonitor}
     * objects and dispatches this call to the system activity manager; you can
     * override this to watch for the application to start an activity, and 
     * modify what happens when it does. 
     *
     * <p>This method returns an {@link ActivityResult} object, which you can 
     * use when intercepting application calls to avoid performing the start 
     * activity action but still return the result the application is 
     * expecting.  To do this, override this method to catch the call to start 
     * activity so that it returns a new ActivityResult containing the results 
     * you would like the application to see, and don't call up to the super 
     * class.  Note that an application is only expecting a result if 
     * <var>requestCode</var> is &gt;= 0.
     *
     * <p>This method throws {@link android.content.ActivityNotFoundException}
     * if there was no Activity found to run the given Intent.
     *
     * @param who The Context from which the activity is being started.
     * @param contextThread The main thread of the Context from which the activity
     *                      is being started.
     * @param token Internal token identifying to the system who is starting 
     *              the activity; may be null.
     * @param target Which activity is performing the start (and thus receiving 
     *               any result); may be null if this call is not being made
     *               from an activity.
     * @param intent The actual Intent to start.
     * @param requestCode Identifier for this request's result; less than zero 
     *                    if the caller is not expecting a result.
     * @param options Addition options.
     *
     * @return To force the return of a particular result, return an 
     *         ActivityResult object containing the desired data; otherwise
     *         return null.  The default implementation always returns null.
     *
     * @throws android.content.ActivityNotFoundException
     *
     * @see Activity#startActivity(Intent)
     * @see Activity#startActivityForResult(Intent, int)
     *
     * {@hide}
     */
    @UnsupportedAppUsage
    public ActivityResult execStartActivity(
            Context who, IBinder contextThread, IBinder token, Activity target,  
            Intent intent, int requestCode, Bundle options) {


        /*
                Activity中的调用为 mInstrumentation.execStartActivity(
                    this, mMainThread.getApplicationThread(), mToken, this,
                    intent, requestCode, options);

                // who 和 target 均为 Activity调用对象
                // 从注释上来看应该是发起方和执行方其实是可以不同的
        */
        
        IApplicationThread whoThread = (IApplicationThread) contextThread;
// SKIP START
        Uri referrer = target != null ? target.onProvideReferrer() : null;
        if (referrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, referrer);
        }
// SKIP END
        if (mActivityMonitors != null) {    
            synchronized (mSync) {
                final int N = mActivityMonitors.size();

                // Monitor拦截，如果截获成功，返回monitor内置的ActivityResult
                // 1. boolean无条件拦截
                // 2. subclass重写onStartActivity拦截
                // 3. filter-match拦截
                for (int i=0; i<N; i++) {
                    final ActivityMonitor am = mActivityMonitors.get(i);
                    ActivityResult result = null;
                    if (am.ignoreMatchingSpecificIntents()) { // 
                        result = am.onStartActivity(intent);
                    }
                    if (result != null) {     // 一般am.onStartActivity()是返回null的(见Google注释)
                        am.mHits++;           // 一个拦截的统计
                        return result;
                    } else if (am.match(who, null, intent)) {
                        am.mHits++;
                        if (am.isBlocking()) {
                            return requestCode >= 0 ? am.getResult() : null;
                        }
                        break;
                    }
                }
            }
        }
        try {
            intent.migrateExtraStreamToClipData(who);
            intent.prepareToLeaveProcess(who);
// TRACK START
            int result = ActivityTaskManager.getService().startActivity(whoThread,
                    who.getBasePackageName(), who.getAttributionTag(), intent,
                    intent.resolveTypeIfNeeded(who.getContentResolver()), token,
                    target != null ? target.mEmbeddedID : null, requestCode, 0, null, options);
// TRACK END
            checkStartActivityResult(result, intent);
        } catch (RemoteException e) {
            throw new RuntimeException("Failure from system", e);
        }
        return null;
    }


////////////////////////////////////////////////////////////////////////////////////


    // 文件分布：frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java

    // 代码变动，新的调用流程在WithFeature


    @Override
    public int startActivityWithFeature(IApplicationThread caller, String callingPackage,
            String callingFeatureId, Intent intent, String resolvedType, IBinder resultTo,
            String resultWho, int requestCode, int startFlags, ProfilerInfo profilerInfo,
            Bundle bOptions) {
        // 无论是AsUserWithFeature，还是AndWait都会委托到mActivityTaskManager
        return mActivityTaskManager.startActivity(caller, callingPackage, callingFeatureId, intent,
                resolvedType, resultTo, resultWho, requestCode, startFlags, profilerInfo, bOptions);
    }

////////////////////////////////////////////////////////////////////////////////////

    // 文件分布：frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java


    @Override
    public final int startActivity(IApplicationThread caller, String callingPackage,
            String callingFeatureId, Intent intent, String resolvedType, IBinder resultTo,
            String resultWho, int requestCode, int startFlags, ProfilerInfo profilerInfo,
            Bundle bOptions) {
        return startActivityAsUser(caller, callingPackage, callingFeatureId, intent, resolvedType,
                resultTo, resultWho, requestCode, startFlags, profilerInfo, bOptions,
                UserHandle.getCallingUserId());
    }

    @Override
    public int startActivityAsUser(IApplicationThread caller, String callingPackage,
            String callingFeatureId, Intent intent, String resolvedType, IBinder resultTo,
            String resultWho, int requestCode, int startFlags, ProfilerInfo profilerInfo,
            Bundle bOptions, int userId) {
        return startActivityAsUser(caller, callingPackage, callingFeatureId, intent, resolvedType,
                resultTo, resultWho, requestCode, startFlags, profilerInfo, bOptions, userId,
                true /*validateIncomingUser*/);
    }

    private int startActivityAsUser(IApplicationThread caller, String callingPackage,
            @Nullable String callingFeatureId, Intent intent, String resolvedType,
            IBinder resultTo, String resultWho, int requestCode, int startFlags,
            ProfilerInfo profilerInfo, Bundle bOptions, int userId, boolean validateIncomingUser) {
        assertPackageMatchesCallingUid(callingPackage);
        enforceNotIsolatedCaller("startActivityAsUser");

        userId = getActivityStartController().checkTargetUser(userId, validateIncomingUser,
                Binder.getCallingPid(), Binder.getCallingUid(), "startActivityAsUser");

        // TODO: Switch to user app stacks here.
        return getActivityStartController().obtainStarter(intent, "startActivityAsUser")
                .setCaller(caller)
                .setCallingPackage(callingPackage)
                .setCallingFeatureId(callingFeatureId)
                .setResolvedType(resolvedType)
                .setResultTo(resultTo)
                .setResultWho(resultWho)
                .setRequestCode(requestCode)
                .setStartFlags(startFlags)
                .setProfilerInfo(profilerInfo)
                .setActivityOptions(bOptions)
                .setUserId(userId)
                .execute();

    }

////////////////////////////////////////////////////////////////////////////////////

    // 文件分布：frameworks/base/services/core/java/com/android/server/wm/ActivityStarter.java

    /**
     * Resolve necessary information according the request parameters provided earlier, and execute
     * the request which begin the journey of starting an activity.
     * @return The starter result.
     */
    int execute() {
// SKIP START
        try {
            // Refuse possible leaked file descriptors
            if (mRequest.intent != null && mRequest.intent.hasFileDescriptors()) {                  // TODO mRequest
                throw new IllegalArgumentException("File descriptors passed in Intent");
            }

            final LaunchingState launchingState;
            synchronized (mService.mGlobalLock) {
                final ActivityRecord caller = ActivityRecord.forTokenLocked(mRequest.resultTo);      // TODO caller
                launchingState = mSupervisor.getActivityMetricsLogger().notifyActivityLaunching(
                        mRequest.intent, caller);
            }

            // If the caller hasn't already resolved the activity, we're willing
            // to do so here. If the caller is already holding the WM lock here,
            // and we need to check dynamic Uri permissions, then we're forced
            // to assume those permissions are denied to avoid deadlocking.
            if (mRequest.activityInfo == null) {
                mRequest.resolveActivity(mSupervisor);
            }
// SKIP END
            int res;
            synchronized (mService.mGlobalLock) {
                final boolean globalConfigWillChange = mRequest.globalConfig != null
                        && mService.getGlobalConfiguration().diff(mRequest.globalConfig) != 0;
                final ActivityStack stack = mRootWindowContainer.getTopDisplayFocusedStack();     // TODO stack
                if (stack != null) {
                    stack.mConfigWillChange = globalConfigWillChange;
                }
                if (DEBUG_CONFIGURATION) {
                    Slog.v(TAG_CONFIGURATION, "Starting activity when config will change = "
                            + globalConfigWillChange);
                }

                final long origId = Binder.clearCallingIdentity();

                res = resolveToHeavyWeightSwitcherIfNeeded();
                if (res != START_SUCCESS) {
                    return res;
                }
                res = executeRequest(mRequest);

                Binder.restoreCallingIdentity(origId);

                if (globalConfigWillChange) {
                    // If the caller also wants to switch to a new configuration, do so now.
                    // This allows a clean switch, as we are waiting for the current activity
                    // to pause (so we will not destroy it), and have not yet started the
                    // next activity.
                    mService.mAmInternal.enforceCallingPermission(
                            android.Manifest.permission.CHANGE_CONFIGURATION,
                            "updateConfiguration()");
                    if (stack != null) {
                        stack.mConfigWillChange = false;
                    }
                    if (DEBUG_CONFIGURATION) {
                        Slog.v(TAG_CONFIGURATION,
                                "Updating to new configuration after starting activity.");
                    }
                    mService.updateConfigurationLocked(mRequest.globalConfig, null, false);
                }

                // Notify ActivityMetricsLogger that the activity has launched.
                // ActivityMetricsLogger will then wait for the windows to be drawn and populate
                // WaitResult.
                mSupervisor.getActivityMetricsLogger().notifyActivityLaunched(launchingState, res,
                        mLastStartActivityRecord);
                return getExternalResult(mRequest.waitResult == null ? res
                        : waitForResult(res, mLastStartActivityRecord));
            }
        } finally {
            onExecutionComplete();
        }
    }

    
// See also: wiki 66664a705