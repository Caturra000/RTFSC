// 文件：frameworks/base/core/java/android/app/Instrumentation.java

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
        IApplicationThread whoThread = (IApplicationThread) contextThread;
        Uri referrer = target != null ? target.onProvideReferrer() : null;
        if (referrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, referrer);
        }
        if (mActivityMonitors != null) {
            synchronized (mSync) {
                final int N = mActivityMonitors.size();
                for (int i=0; i<N; i++) {
                    final ActivityMonitor am = mActivityMonitors.get(i);
                    ActivityResult result = null;
                    if (am.ignoreMatchingSpecificIntents()) {
                        result = am.onStartActivity(intent);
                    }
                    if (result != null) {
                        am.mHits++;
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
            int result = ActivityTaskManager.getService().startActivity(whoThread,
                    who.getBasePackageName(), who.getAttributionTag(), intent,
                    intent.resolveTypeIfNeeded(who.getContentResolver()), token,
                    target != null ? target.mEmbeddedID : null, requestCode, 0, null, options);
            checkStartActivityResult(result, intent);
        } catch (RemoteException e) {
            throw new RuntimeException("Failure from system", e);
        }
        return null;
    }