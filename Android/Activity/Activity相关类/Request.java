
    // 文件分布：frameworks/base/services/core/java/com/android/server/wm/ActivityStarter.java
    // TL;DR 一个简单的Builder，只包含Starter用到的传参
    // 使用的原因推测是在R版本前的ActivityStarter#execute()传参过于庞大

    /**
     * Container for capturing initial start request details. This information is NOT reset until
     * the {@link ActivityStarter} is recycled, allowing for multiple invocations with the same
     * parameters.
     *
     * TODO(b/64750076): Investigate consolidating member variables of {@link ActivityStarter} with
     * the request object. Note that some member variables are referenced in
     * {@link #dump(PrintWriter, String)} and therefore cannot be cleared immediately after
     * execution.
     */
    @VisibleForTesting
    static class Request {
        private static final int DEFAULT_CALLING_UID = -1;
        private static final int DEFAULT_CALLING_PID = 0;
        static final int DEFAULT_REAL_CALLING_UID = -1;
        static final int DEFAULT_REAL_CALLING_PID = 0;

        IApplicationThread caller;
        Intent intent;
        NeededUriGrants intentGrants;
        // A copy of the original requested intent, in case for ephemeral app launch.
        Intent ephemeralIntent;
        String resolvedType;
        ActivityInfo activityInfo;
        ResolveInfo resolveInfo;
        IVoiceInteractionSession voiceSession;
        IVoiceInteractor voiceInteractor;
        IBinder resultTo;
        String resultWho;
        int requestCode;
        int callingPid = DEFAULT_CALLING_PID;
        int callingUid = DEFAULT_CALLING_UID;
        String callingPackage;
        @Nullable String callingFeatureId;
        int realCallingPid = DEFAULT_REAL_CALLING_PID;
        int realCallingUid = DEFAULT_REAL_CALLING_UID;
        int startFlags;
        SafeActivityOptions activityOptions;
        boolean ignoreTargetSecurity;
        boolean componentSpecified;
        boolean avoidMoveToFront;
        ActivityRecord[] outActivity;
        Task inTask;
        String reason;
        ProfilerInfo profilerInfo;
        Configuration globalConfig;
        int userId;
        WaitResult waitResult;
        int filterCallingUid;
        PendingIntentRecord originatingPendingIntent;
        boolean allowBackgroundActivityStart;


        //....省略


        /**
         * Resolve activity from the given intent for this launch.
         */
        void resolveActivity(ActivityStackSupervisor supervisor) {
            if (realCallingPid == Request.DEFAULT_REAL_CALLING_PID) {
                realCallingPid = Binder.getCallingPid();
            }
            if (realCallingUid == Request.DEFAULT_REAL_CALLING_UID) {
                realCallingUid = Binder.getCallingUid();
            }

            if (callingUid >= 0) {
                callingPid = -1;
            } else if (caller == null) {
                callingPid = realCallingPid;
                callingUid = realCallingUid;
            } else {
                callingPid = callingUid = -1;
            }

            // To determine the set of needed Uri permission grants, we need the
            // "resolved" calling UID, where we try our best to identify the
            // actual caller that is starting this activity
            int resolvedCallingUid = callingUid;
            if (caller != null) {
                synchronized (supervisor.mService.mGlobalLock) {
                    final WindowProcessController callerApp = supervisor.mService
                            .getProcessController(caller);
                    if (callerApp != null) {
                        resolvedCallingUid = callerApp.mInfo.uid;
                    }
                }
            }

            // Save a copy in case ephemeral needs it
            ephemeralIntent = new Intent(intent);
            // Don't modify the client's object!
            intent = new Intent(intent);
            if (intent.getComponent() != null
                    && !(Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getData() == null)
                    && !Intent.ACTION_INSTALL_INSTANT_APP_PACKAGE.equals(intent.getAction())
                    && !Intent.ACTION_RESOLVE_INSTANT_APP_PACKAGE.equals(intent.getAction())
                    && supervisor.mService.getPackageManagerInternalLocked()
                            .isInstantAppInstallerComponent(intent.getComponent())) {
                // Intercept intents targeted directly to the ephemeral installer the ephemeral
                // installer should never be started with a raw Intent; instead adjust the intent
                // so it looks like a "normal" instant app launch.
                intent.setComponent(null /* component */);
            }

            resolveInfo = supervisor.resolveIntent(intent, resolvedType, userId,
                    0 /* matchFlags */,
                    computeResolveFilterUid(callingUid, realCallingUid, filterCallingUid));
            if (resolveInfo == null) {
                final UserInfo userInfo = supervisor.getUserInfo(userId);
                if (userInfo != null && userInfo.isManagedProfile()) {
                    // Special case for managed profiles, if attempting to launch non-cryto aware
                    // app in a locked managed profile from an unlocked parent allow it to resolve
                    // as user will be sent via confirm credentials to unlock the profile.
                    final UserManager userManager = UserManager.get(supervisor.mService.mContext);
                    boolean profileLockedAndParentUnlockingOrUnlocked = false;
                    final long token = Binder.clearCallingIdentity();
                    try {
                        final UserInfo parent = userManager.getProfileParent(userId);
                        profileLockedAndParentUnlockingOrUnlocked = (parent != null)
                                && userManager.isUserUnlockingOrUnlocked(parent.id)
                                && !userManager.isUserUnlockingOrUnlocked(userId);
                    } finally {
                        Binder.restoreCallingIdentity(token);
                    }
                    if (profileLockedAndParentUnlockingOrUnlocked) {
                        resolveInfo = supervisor.resolveIntent(intent, resolvedType, userId,
                                PackageManager.MATCH_DIRECT_BOOT_AWARE
                                        | PackageManager.MATCH_DIRECT_BOOT_UNAWARE,
                                computeResolveFilterUid(callingUid, realCallingUid,
                                        filterCallingUid));
                    }
                }
            }

            // Collect information about the target of the Intent.
            activityInfo = supervisor.resolveActivity(intent, resolveInfo, startFlags,
                    profilerInfo);

            // Carefully collect grants without holding lock
            if (activityInfo != null) {
                intentGrants = supervisor.mService.mUgmInternal.checkGrantUriPermissionFromIntent(
                        intent, resolvedCallingUid, activityInfo.applicationInfo.packageName,
                        UserHandle.getUserId(activityInfo.applicationInfo.uid));
            }
        }
