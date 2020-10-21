
    // 文件分布： frameworks/base/core/java/android/app/Instrumentation.java

    // TL;DR
    // ActivityMonitor负责Activity启动的截获

    /**
     * Information about a particular kind of Intent that is being monitored.
     * An instance of this class is added to the 
     * current instrumentation through {@link #addMonitor}; after being added, 
     * when a new activity is being started the monitor will be checked and, if 
     * matching, its hit count updated and (optionally) the call stopped and a 
     * canned result returned.
     * 
     * <p>An ActivityMonitor can also be used to look for the creation of an
     * activity, through the {@link #waitForActivity} method.  This will return
     * after a matching activity has been created with that activity object.
     */
    public static class ActivityMonitor {
        private final IntentFilter mWhich;
        private final String mClass;
        private final ActivityResult mResult;
        private final boolean mBlock;
        private final boolean mIgnoreMatchingSpecificIntents;

        // Trick: 前面的字段用途可查阅构造里的注释


        // This is protected by 'Instrumentation.this.mSync'.
        /*package*/ int mHits = 0;

        // This is protected by 'this'.
        /*package*/ Activity mLastActivity = null;




        /**
         * Create a new ActivityMonitor that looks for a particular kind of 
         * intent to be started.
         *
         * @param which The set of intents this monitor is responsible for.
         * @param result A canned result to return if the monitor is hit; can    当cancel时触发result
         *               be null.
         * @param block Controls whether the monitor should block the activity   monitor阻塞activity启动
         *              start (returning its canned result) or let the call
         *              proceed.
         *  
         * @see Instrumentation#addMonitor 
         */
        public ActivityMonitor(
            IntentFilter which, ActivityResult result, boolean block) {
            mWhich = which;
            mClass = null;
            mResult = result;
            mBlock = block;
            mIgnoreMatchingSpecificIntents = false;
        }


        /**
         * Used for intercepting any started activity.
         *
         * <p> A non-null return value here will be considered a hit for this monitor.
         * By default this will return {@code null} and subclasses can override this to return     subclass可以返回non-null
         * a non-null value if the intent needs to be intercepted.
         *
         * <p> Whenever a new activity is started, this method will be called on instances created
         * using {@link #ActivityMonitor()} to check if there is a match. In case
         * of a match, the activity start will be blocked and the returned result will be used.     大概的意思是启动流程调用时看是否match，如果match则直接返回Monitor的result?
         *
         * @param intent The intent used for starting the activity.
         * @return The {@link ActivityResult} that needs to be used in case of a match.
         */
        public ActivityResult onStartActivity(Intent intent) {
            return null;
        }


        final boolean match(Context who,
                            Activity activity,
                            Intent intent) {
            if (mIgnoreMatchingSpecificIntents) {
                return false;
            }
            synchronized (this) {
                if (mWhich != null
                    && mWhich.match(who.getContentResolver(), intent,
                                    true, "Instrumentation") < 0) {
                    return false;
                }
                if (mClass != null) {
                    String cls = null;
                    if (activity != null) {
                        cls = activity.getClass().getName();
                    } else if (intent.getComponent() != null) {
                        cls = intent.getComponent().getClassName();
                    }
                    if (cls == null || !mClass.equals(cls)) {
                        return false;
                    }
                }
                if (activity != null) {
                    mLastActivity = activity;
                    notifyAll();
                }
                return true;
            }
        }


        /**
         * @return true if this monitor is used for intercepting **any started activity** by calling 无条件拦截
         *         into {@link #onStartActivity(Intent)}, false if this monitor is only used
         *         for specific intents corresponding to the intent filter or activity class
         *         passed in the constructor.
         */
        final boolean ignoreMatchingSpecificIntents() {
            return mIgnoreMatchingSpecificIntents;
        }



    }
