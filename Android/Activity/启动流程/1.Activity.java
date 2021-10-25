// 文件：frameworks/base/core/java/android/app/Activity.java

    /**
     * Same as {@link #startActivity(Intent, Bundle)} with no options
     * specified.
     *
     * @param intent The intent to start.
     *
     * @throws android.content.ActivityNotFoundException
     *
     * @see #startActivity(Intent, Bundle)
     * @see #startActivityForResult
     */
    @Override
    public void startActivity(Intent intent) {
        this.startActivity(intent, null);
    }

    /**
     * Launch a new activity.  You will not receive any information about when
     * the activity exits.  This implementation overrides the base version,
     * providing information about
     * the activity performing the launch.  Because of this additional
     * information, the {@link Intent#FLAG_ACTIVITY_NEW_TASK} launch flag is not
     * required; if not specified, the new activity will be added to the
     * task of the caller.
     *
     * <p>This method throws {@link android.content.ActivityNotFoundException}
     * if there was no Activity found to run the given Intent.
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
    ///////////////////////////////
    // 把Context.startActivity(Intent, Bundle)的注释搬过来了
    // /**
    //  * Launch a new activity.  You will not receive any information about when
    //  * the activity exits.
    //  *
    //  * <p>Note that if this method is being called from outside of an
    //  * {@link android.app.Activity} Context, then the Intent must include
    //  * the {@link Intent#FLAG_ACTIVITY_NEW_TASK} launch flag.  This is because,
    //  * without being started from an existing Activity, there is no existing
    //  * task in which to place the new activity and thus it needs to be placed
    //  * in its own separate task.
    //  *
    //  * <p>This method throws {@link ActivityNotFoundException}
    //  * if there was no Activity found to run the given Intent.
    //  *
    //  * @param intent The description of the activity to start.
    //  * @param options Additional options for how the Activity should be started.
    //  * May be null if there are no options.  See {@link android.app.ActivityOptions}
    //  * for how to build the Bundle supplied here; there are no supported definitions
    //  * for building it manually.
    //  *
    //  * @throws ActivityNotFoundException &nbsp;
    //  *
    //  * @see #startActivity(Intent)
    //  * @see PackageManager#resolveActivity
    //  */
    // public abstract void startActivity(@RequiresPermission Intent intent,
    //         @Nullable Bundle options);
    // Context.startActivity需要携带Intent#FLAG_ACTIVITY_NEW_TASK，因为可以在原先没有Activity的基础上调用，所以要安排NEW_TASK，如果已有实例，则会onNewIntent()
    // - 具体见https://developer.android.google.cn/guide/components/activities/tasks-and-back-stack#IntentFlagsForTasks
    // options需要看ActivityOptions.java，一般当它null吧
    @Override
    public void startActivity(Intent intent, @Nullable Bundle options) {
        // auto fill server相关，略
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
        if (options != null) {
            startActivityForResult(intent, -1, options);
        } else {
            // Note we want to go through this call for compatibility with
            // applications that may have overridden the method.
            // 内部调用 startActivityForResult(intent, -1, null);
            // 但可能存在override的情况，因此不直接调用带null传参的方法
            startActivityForResult(intent, -1);
        }
    }

    /**
     * Same as calling {@link #startActivityForResult(Intent, int, Bundle)}
     * with no options.
     *
     * @param intent The intent to start.
     * @param requestCode If >= 0, this code will be returned in
     *                    onActivityResult() when the activity exits.
     *
     * @throws android.content.ActivityNotFoundException
     *
     * @see #startActivity
     */
    // requestCode决定是否回调onActivityResult()
    public void startActivityForResult(@RequiresPermission Intent intent, int requestCode) {
        startActivityForResult(intent, requestCode, null);
    }

    /**
     * Launch an activity for which you would like a result when it finished.
     * When this activity exits, your
     * onActivityResult() method will be called with the given requestCode.
     * Using a negative requestCode is the same as calling
     * {@link #startActivity} (the activity is not launched as a sub-activity).
     *
     * <p>Note that this method should only be used with Intent protocols
     * that are defined to return a result.  In other protocols (such as
     * {@link Intent#ACTION_MAIN} or {@link Intent#ACTION_VIEW}), you may
     * not get the result when you expect.  For example, if the activity you
     * are launching uses {@link Intent#FLAG_ACTIVITY_NEW_TASK}, it will not
     * run in your task and thus you will immediately receive a cancel result.
     *
     * <p>As a special case, if you call startActivityForResult() with a requestCode
     * >= 0 during the initial onCreate(Bundle savedInstanceState)/onResume() of your
     * activity, then your window will not be displayed until a result is
     * returned back from the started activity.  This is to avoid visible
     * flickering when redirecting to another activity.
     *
     * <p>This method throws {@link android.content.ActivityNotFoundException}
     * if there was no Activity found to run the given Intent.
     *
     * @param intent The intent to start.
     * @param requestCode If >= 0, this code will be returned in
     *                    onActivityResult() when the activity exits.
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
        // 一般来说，关注这个
        if (mParent == null) {
            options = transferSpringboardActivityOptions(options);
            Instrumentation.ActivityResult ar =
                mInstrumentation.execStartActivity(
                    this, mMainThread.getApplicationThread(), mToken, this,
                    intent, requestCode, options);
            if (ar != null) {
                // 执行onActivityResult()
                mMainThread.sendActivityResult(
                    mToken, mEmbeddedID, requestCode, ar.getResultCode(),
                    ar.getResultData());
            }
            if (requestCode >= 0) {
                // If this start is requesting a result, we can avoid making
                // the activity visible until the result is received.  Setting
                // this code during onCreate(Bundle savedInstanceState) or onResume() will keep the
                // activity hidden during this time, to avoid flickering.
                // This can only be done when a result is requested because
                // that guarantees we will get information back when the
                // activity is finished, no matter what happens to it.
                // 注意avoid making the activity visible
                mStartedActivity = true;
            }

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
