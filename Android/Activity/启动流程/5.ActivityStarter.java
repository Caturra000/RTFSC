// 文件：frameworks/base/services/core/java/com/android/server/wm/ActivityStarter.java

    /**
     * Resolve necessary information according the request parameters provided earlier, and execute
     * the request which begin the journey of starting an activity.
     * @return The starter result.
     */
    int execute() {
        try {
            // Refuse possible leaked file descriptors
            if (mRequest.intent != null && mRequest.intent.hasFileDescriptors()) {
                throw new IllegalArgumentException("File descriptors passed in Intent");
            }

            final LaunchingState launchingState;
            synchronized (mService.mGlobalLock) {
                // mRequest.resultTo是一个IBinder接口，也是token
                // TODO token -> ActivityRecord
                final ActivityRecord caller = ActivityRecord.forTokenLocked(mRequest.resultTo);
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

            int res;
            synchronized (mService.mGlobalLock) {
                final boolean globalConfigWillChange = mRequest.globalConfig != null
                        && mService.getGlobalConfiguration().diff(mRequest.globalConfig) != 0;
                final ActivityStack stack = mRootWindowContainer.getTopDisplayFocusedStack();
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










    /**
     * Executing activity start request and starts the journey of starting an activity. Here
     * begins with performing several preliminary checks. The normally activity launch flow will
     * go through {@link #startActivityUnchecked} to {@link #startActivityInner}.
     */
    private int executeRequest(Request request) {
        if (TextUtils.isEmpty(request.reason)) {
            throw new IllegalArgumentException("Need to specify a reason.");
        }
        mLastStartReason = request.reason;
        mLastStartActivityTimeMs = System.currentTimeMillis();
        mLastStartActivityRecord = null;

        final IApplicationThread caller = request.caller;
        Intent intent = request.intent;
        NeededUriGrants intentGrants = request.intentGrants;
        String resolvedType = request.resolvedType;
        ActivityInfo aInfo = request.activityInfo;
        ResolveInfo rInfo = request.resolveInfo;
        final IVoiceInteractionSession voiceSession = request.voiceSession;
        final IBinder resultTo = request.resultTo;
        String resultWho = request.resultWho;
        int requestCode = request.requestCode;
        int callingPid = request.callingPid;
        int callingUid = request.callingUid;
        String callingPackage = request.callingPackage;
        String callingFeatureId = request.callingFeatureId;
        final int realCallingPid = request.realCallingPid;
        final int realCallingUid = request.realCallingUid;
        final int startFlags = request.startFlags;
        final SafeActivityOptions options = request.activityOptions;
        Task inTask = request.inTask;

        int err = ActivityManager.START_SUCCESS;
        // Pull the optional Ephemeral Installer-only bundle out of the options early.
        final Bundle verificationBundle =
                options != null ? options.popAppVerificationBundle() : null;

        // WindowProcessController在WM中保存的应用进程信息，对应于AMS中的ProcessRecord
        WindowProcessController callerApp = null;
        if (caller != null) {
            callerApp = mService.getProcessController(caller);
            if (callerApp != null) {
                callingPid = callerApp.getPid();
                callingUid = callerApp.mInfo.uid;
            } else {
                Slog.w(TAG, "Unable to find app for caller " + caller + " (pid=" + callingPid
                        + ") when starting: " + intent.toString());
                err = ActivityManager.START_PERMISSION_DENIED;
            }
        }

        // 目标Activity的userId
        final int userId = aInfo != null && aInfo.applicationInfo != null
                ? UserHandle.getUserId(aInfo.applicationInfo.uid) : 0;
        if (err == ActivityManager.START_SUCCESS) {
            Slog.i(TAG, "START u" + userId + " {" + intent.toShortString(true, true, true, false)
                    + "} from uid " + callingUid);
        }

        // 表示启动源ActivityRecord对象
        ActivityRecord sourceRecord = null;
        // 表示目标Activity启动完成后，接收结果的Activity的ActivityRecord对象，正常情况下sourceRecord=resultRecord
        ActivityRecord resultRecord = null;
        // 如果requestCode存在，resultTo就不为null
        if (resultTo != null) {
            // 查找ActivityStack中是否存在此启动源ActivityRecord
            sourceRecord = mRootWindowContainer.isInAnyStack(resultTo);
            if (DEBUG_RESULTS) {
                Slog.v(TAG_RESULTS, "Will send result to " + resultTo + " " + sourceRecord);
            }
            if (sourceRecord != null) {
                // TODO finishing标记
                if (requestCode >= 0 && !sourceRecord.finishing) {
                    resultRecord = sourceRecord;
                }
            }
        }

        final int launchFlags = intent.getFlags();
        // TODO FLAG_ACTIVITY_FORWARD_RESULT
        if ((launchFlags & Intent.FLAG_ACTIVITY_FORWARD_RESULT) != 0 && sourceRecord != null) {
            // Transfer the result target from the source activity to the new one being started,
            // including any failures.
            if (requestCode >= 0) {
                SafeActivityOptions.abort(options);
                return ActivityManager.START_FORWARD_AND_REQUEST_CONFLICT;
            }
            resultRecord = sourceRecord.resultTo;
            if (resultRecord != null && !resultRecord.isInStackLocked()) {
                resultRecord = null;
            }
            resultWho = sourceRecord.resultWho;
            requestCode = sourceRecord.requestCode;
            sourceRecord.resultTo = null;
            if (resultRecord != null) {
                resultRecord.removeResultsLocked(sourceRecord, resultWho, requestCode);
            }
            if (sourceRecord.launchedFromUid == callingUid) {
                // The new activity is being launched from the same uid as the previous activity
                // in the flow, and asking to forward its result back to the previous.  In this
                // case the activity is serving as a trampoline between the two, so we also want
                // to update its launchedFromPackage to be the same as the previous activity.
                // Note that this is safe, since we know these two packages come from the same
                // uid; the caller could just as well have supplied that same package name itself
                // . This specifially deals with the case of an intent picker/chooser being
                // launched in the app flow to redirect to an activity picked by the user, where
                // we want the final activity to consider it to have been launched by the
                // previous app activity.
                callingPackage = sourceRecord.launchedFromPackage;
                callingFeatureId = sourceRecord.launchedFromFeatureId;
            }
        }

        if (err == ActivityManager.START_SUCCESS && intent.getComponent() == null) {
            // We couldn't find a class that can handle the given Intent.
            // That's the end of that!
            err = ActivityManager.START_INTENT_NOT_RESOLVED;
        }

        if (err == ActivityManager.START_SUCCESS && aInfo == null) {
            // We couldn't find the specific class specified in the Intent.
            // Also the end of the line.
            err = ActivityManager.START_CLASS_NOT_FOUND;
        }

        if (err == ActivityManager.START_SUCCESS && sourceRecord != null
                && sourceRecord.getTask().voiceSession != null) {
            // If this activity is being launched as part of a voice session, we need to ensure
            // that it is safe to do so.  If the upcoming activity will also be part of the voice
            // session, we can only launch it if it has explicitly said it supports the VOICE
            // category, or it is a part of the calling app.
            if ((launchFlags & FLAG_ACTIVITY_NEW_TASK) == 0
                    && sourceRecord.info.applicationInfo.uid != aInfo.applicationInfo.uid) {
                try {
                    intent.addCategory(Intent.CATEGORY_VOICE);
                    if (!mService.getPackageManager().activitySupportsIntent(
                            intent.getComponent(), intent, resolvedType)) {
                        Slog.w(TAG, "Activity being started in current voice task does not support "
                                + "voice: " + intent);
                        err = ActivityManager.START_NOT_VOICE_COMPATIBLE;
                    }
                } catch (RemoteException e) {
                    Slog.w(TAG, "Failure checking voice capabilities", e);
                    err = ActivityManager.START_NOT_VOICE_COMPATIBLE;
                }
            }
        }

        if (err == ActivityManager.START_SUCCESS && voiceSession != null) {
            // If the caller is starting a new voice session, just make sure the target
            // is actually allowing it to run this way.
            try {
                if (!mService.getPackageManager().activitySupportsIntent(intent.getComponent(),
                        intent, resolvedType)) {
                    Slog.w(TAG,
                            "Activity being started in new voice task does not support: " + intent);
                    err = ActivityManager.START_NOT_VOICE_COMPATIBLE;
                }
            } catch (RemoteException e) {
                Slog.w(TAG, "Failure checking voice capabilities", e);
                err = ActivityManager.START_NOT_VOICE_COMPATIBLE;
            }
        }

        final ActivityStack resultStack = resultRecord == null
                ? null : resultRecord.getRootTask();

        if (err != START_SUCCESS) {
            if (resultRecord != null) {
                resultRecord.sendResult(INVALID_UID, resultWho, requestCode, RESULT_CANCELED,
                        null /* data */, null /* dataGrants */);
            }
            SafeActivityOptions.abort(options);
            return err;
        }

        boolean abort = !mSupervisor.checkStartAnyActivityPermission(intent, aInfo, resultWho,
                requestCode, callingPid, callingUid, callingPackage, callingFeatureId,
                request.ignoreTargetSecurity, inTask != null, callerApp, resultRecord, resultStack);
        abort |= !mService.mIntentFirewall.checkStartActivity(intent, callingUid,
                callingPid, resolvedType, aInfo.applicationInfo);
        abort |= !mService.getPermissionPolicyInternal().checkStartActivity(intent, callingUid,
                callingPackage);

        boolean restrictedBgActivity = false;
        if (!abort) {
            try {
                Trace.traceBegin(Trace.TRACE_TAG_WINDOW_MANAGER,
                        "shouldAbortBackgroundActivityStart");
                restrictedBgActivity = shouldAbortBackgroundActivityStart(callingUid,
                        callingPid, callingPackage, realCallingUid, realCallingPid, callerApp,
                        request.originatingPendingIntent, request.allowBackgroundActivityStart,
                        intent);
            } finally {
                Trace.traceEnd(Trace.TRACE_TAG_WINDOW_MANAGER);
            }
        }

        // Merge the two options bundles, while realCallerOptions takes precedence.
        ActivityOptions checkedOptions = options != null
                ? options.getOptions(intent, aInfo, callerApp, mSupervisor) : null;
        if (request.allowPendingRemoteAnimationRegistryLookup) {
            checkedOptions = mService.getActivityStartController()
                    .getPendingRemoteAnimationRegistry()
                    .overrideOptionsIfNeeded(callingPackage, checkedOptions);
        }
        if (mService.mController != null) {
            try {
                // The Intent we give to the watcher has the extra data stripped off, since it
                // can contain private information.
                Intent watchIntent = intent.cloneFilter();
                abort |= !mService.mController.activityStarting(watchIntent,
                        aInfo.applicationInfo.packageName);
            } catch (RemoteException e) {
                mService.mController = null;
            }
        }

        mInterceptor.setStates(userId, realCallingPid, realCallingUid, startFlags, callingPackage,
                callingFeatureId);
        if (mInterceptor.intercept(intent, rInfo, aInfo, resolvedType, inTask, callingPid,
                callingUid, checkedOptions)) {
            // activity start was intercepted, e.g. because the target user is currently in quiet
            // mode (turn off work) or the target application is suspended
            intent = mInterceptor.mIntent;
            rInfo = mInterceptor.mRInfo;
            aInfo = mInterceptor.mAInfo;
            resolvedType = mInterceptor.mResolvedType;
            inTask = mInterceptor.mInTask;
            callingPid = mInterceptor.mCallingPid;
            callingUid = mInterceptor.mCallingUid;
            checkedOptions = mInterceptor.mActivityOptions;

            // The interception target shouldn't get any permission grants
            // intended for the original destination
            intentGrants = null;
        }

        if (abort) {
            if (resultRecord != null) {
                resultRecord.sendResult(INVALID_UID, resultWho, requestCode, RESULT_CANCELED,
                        null /* data */, null /* dataGrants */);
            }
            // We pretend to the caller that it was really started, but they will just get a
            // cancel result.
            ActivityOptions.abort(checkedOptions);
            return START_ABORTED;
        }

        // If permissions need a review before any of the app components can run, we
        // launch the review activity and pass a pending intent to start the activity
        // we are to launching now after the review is completed.
        if (aInfo != null) {
            if (mService.getPackageManagerInternalLocked().isPermissionsReviewRequired(
                    aInfo.packageName, userId)) {
                final IIntentSender target = mService.getIntentSenderLocked(
                        ActivityManager.INTENT_SENDER_ACTIVITY, callingPackage, callingFeatureId,
                        callingUid, userId, null, null, 0, new Intent[]{intent},
                        new String[]{resolvedType}, PendingIntent.FLAG_CANCEL_CURRENT
                                | PendingIntent.FLAG_ONE_SHOT, null);

                Intent newIntent = new Intent(Intent.ACTION_REVIEW_PERMISSIONS);

                int flags = intent.getFlags();
                flags |= Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS;

                /*
                 * Prevent reuse of review activity: Each app needs their own review activity. By
                 * default activities launched with NEW_TASK or NEW_DOCUMENT try to reuse activities
                 * with the same launch parameters (extras are ignored). Hence to avoid possible
                 * reuse force a new activity via the MULTIPLE_TASK flag.
                 *
                 * Activities that are not launched with NEW_TASK or NEW_DOCUMENT are not re-used,
                 * hence no need to add the flag in this case.
                 */
                if ((flags & (FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_NEW_DOCUMENT)) != 0) {
                    flags |= Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
                }
                newIntent.setFlags(flags);

                newIntent.putExtra(Intent.EXTRA_PACKAGE_NAME, aInfo.packageName);
                newIntent.putExtra(Intent.EXTRA_INTENT, new IntentSender(target));
                if (resultRecord != null) {
                    newIntent.putExtra(Intent.EXTRA_RESULT_NEEDED, true);
                }
                intent = newIntent;

                // The permissions review target shouldn't get any permission
                // grants intended for the original destination
                intentGrants = null;

                resolvedType = null;
                callingUid = realCallingUid;
                callingPid = realCallingPid;

                // 通过PKMS解析Intent，得到ResolveInfo，启动目标Activity的所有信息都在此对象中保存
                rInfo = mSupervisor.resolveIntent(intent, resolvedType, userId, 0,
                        computeResolveFilterUid(
                                callingUid, realCallingUid, request.filterCallingUid));
                aInfo = mSupervisor.resolveActivity(intent, rInfo, startFlags,
                        null /*profilerInfo*/);

                if (DEBUG_PERMISSIONS_REVIEW) {
                    final ActivityStack focusedStack =
                            mRootWindowContainer.getTopDisplayFocusedStack();
                    Slog.i(TAG, "START u" + userId + " {" + intent.toShortString(true, true,
                            true, false) + "} from uid " + callingUid + " on display "
                            + (focusedStack == null ? DEFAULT_DISPLAY
                                    : focusedStack.getDisplayId()));
                }
            }
        }

        // If we have an ephemeral app, abort the process of launching the resolved intent.
        // Instead, launch the ephemeral installer. Once the installer is finished, it
        // starts either the intent we resolved here [on install error] or the ephemeral
        // app [on install success].
        if (rInfo != null && rInfo.auxiliaryInfo != null) {
            intent = createLaunchIntent(rInfo.auxiliaryInfo, request.ephemeralIntent,
                    callingPackage, callingFeatureId, verificationBundle, resolvedType, userId);
            resolvedType = null;
            callingUid = realCallingUid;
            callingPid = realCallingPid;

            // The ephemeral installer shouldn't get any permission grants
            // intended for the original destination
            intentGrants = null;

            aInfo = mSupervisor.resolveActivity(intent, rInfo, startFlags, null /*profilerInfo*/);
        }

        // 构造ActivityRecord
        final ActivityRecord r = new ActivityRecord(mService, callerApp, callingPid, callingUid,
                callingPackage, callingFeatureId, intent, resolvedType, aInfo,
                mService.getGlobalConfiguration(), resultRecord, resultWho, requestCode,
                request.componentSpecified, voiceSession != null, mSupervisor, checkedOptions,
                sourceRecord);
        mLastStartActivityRecord = r;

        if (r.appTimeTracker == null && sourceRecord != null) {
            // If the caller didn't specify an explicit time tracker, we want to continue
            // tracking under any it has.
            r.appTimeTracker = sourceRecord.appTimeTracker;
        }

        // 根窗口容器顶部的ActivityStack
        final ActivityStack stack = mRootWindowContainer.getTopDisplayFocusedStack();

        // If we are starting an activity that is not from the same uid as the currently resumed
        // one, check whether app switches are allowed.
        if (voiceSession == null && stack != null && (stack.getResumedActivity() == null
                || stack.getResumedActivity().info.applicationInfo.uid != realCallingUid)) {
            if (!mService.checkAppSwitchAllowedLocked(callingPid, callingUid,
                    realCallingPid, realCallingUid, "Activity start")) {
                if (!(restrictedBgActivity && handleBackgroundActivityAbort(r))) {
                    mController.addPendingActivityLaunch(new PendingActivityLaunch(r,
                            sourceRecord, startFlags, stack, callerApp, intentGrants));
                }
                ActivityOptions.abort(checkedOptions);
                return ActivityManager.START_SWITCHES_CANCELED;
            }
        }

        mService.onStartActivitySetDidAppSwitch();
        mController.doPendingActivityLaunches(false);

        mLastStartActivityResult = startActivityUnchecked(r, sourceRecord, voiceSession,
                request.voiceInteractor, startFlags, true /* doResume */, checkedOptions, inTask,
                restrictedBgActivity, intentGrants);

        if (request.outActivity != null) {
            request.outActivity[0] = mLastStartActivityRecord;
        }

        return mLastStartActivityResult;
    }













    /**
     * Start an activity while most of preliminary checks has been done and caller has been
     * confirmed that holds necessary permissions to do so.
     * Here also ensures that the starting activity is removed if the start wasn't successful.
     */
    private int startActivityUnchecked(final ActivityRecord r, ActivityRecord sourceRecord,
                IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
                int startFlags, boolean doResume, ActivityOptions options, Task inTask,
                boolean restrictedBgActivity, NeededUriGrants intentGrants) {
        int result = START_CANCELED;
        final ActivityStack startedActivityStack;
        try {
            mService.deferWindowLayout();
            Trace.traceBegin(Trace.TRACE_TAG_WINDOW_MANAGER, "startActivityInner");
            result = startActivityInner(r, sourceRecord, voiceSession, voiceInteractor,
                    startFlags, doResume, options, inTask, restrictedBgActivity, intentGrants);
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_WINDOW_MANAGER);
            startedActivityStack = handleStartResult(r, result);
            mService.continueWindowLayout();
        }

        postStartActivityProcessing(r, result, startedActivityStack);

        return result;
    }















    /**
     * Start an activity and determine if the activity should be adding to the top of an existing
     * task or delivered new intent to an existing activity. Also manipulating the activity task
     * onto requested or valid stack/display.
     *
     * Note: This method should only be called from {@link #startActivityUnchecked}.
     */

    // TODO(b/152429287): Make it easier to exercise code paths through startActivityInner
    @VisibleForTesting
    int startActivityInner(final ActivityRecord r, ActivityRecord sourceRecord,
            IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
            int startFlags, boolean doResume, ActivityOptions options, Task inTask,
            boolean restrictedBgActivity, NeededUriGrants intentGrants) {
        setInitialState(r, options, inTask, doResume, startFlags, sourceRecord, voiceSession,
                voiceInteractor, restrictedBgActivity);

        computeLaunchingTaskFlags();

        computeSourceStack();

        mIntent.setFlags(mLaunchFlags);

        final Task reusedTask = getReusableTask();

        // If requested, freeze the task list
        if (mOptions != null && mOptions.freezeRecentTasksReordering()
                && mSupervisor.mRecentTasks.isCallerRecents(r.launchedFromUid)
                && !mSupervisor.mRecentTasks.isFreezeTaskListReorderingSet()) {
            mFrozeTaskList = true;
            mSupervisor.mRecentTasks.setFreezeTaskListReordering();
        }

        // Compute if there is an existing task that should be used for.
        final Task targetTask = reusedTask != null ? reusedTask : computeTargetTask();
        final boolean newTask = targetTask == null;
        mTargetTask = targetTask;

        computeLaunchParams(r, sourceRecord, targetTask);

        // Check if starting activity on given task or on a new task is allowed.
        int startResult = isAllowedToStart(r, newTask, targetTask);
        if (startResult != START_SUCCESS) {
            return startResult;
        }

        final ActivityRecord targetTaskTop = newTask
                ? null : targetTask.getTopNonFinishingActivity();
        if (targetTaskTop != null) {
            // Recycle the target task for this launch.
            startResult = recycleTask(targetTask, targetTaskTop, reusedTask, intentGrants);
            if (startResult != START_SUCCESS) {
                return startResult;
            }
        } else {
            mAddingToTask = true;
        }

        // If the activity being launched is the same as the one currently at the top, then
        // we need to check if it should only be launched once.
        final ActivityStack topStack = mRootWindowContainer.getTopDisplayFocusedStack();
        if (topStack != null) {
            startResult = deliverToCurrentTopIfNeeded(topStack, intentGrants);
            if (startResult != START_SUCCESS) {
                return startResult;
            }
        }

        if (mTargetStack == null) {
            mTargetStack = getLaunchStack(mStartActivity, mLaunchFlags, targetTask, mOptions);
        }
        if (newTask) {
            final Task taskToAffiliate = (mLaunchTaskBehind && mSourceRecord != null)
                    ? mSourceRecord.getTask() : null;
            setNewTask(taskToAffiliate);
            if (mService.getLockTaskController().isLockTaskModeViolation(
                    mStartActivity.getTask())) {
                Slog.e(TAG, "Attempted Lock Task Mode violation mStartActivity=" + mStartActivity);
                return START_RETURN_LOCK_TASK_MODE_VIOLATION;
            }
        } else if (mAddingToTask) {
            addOrReparentStartingActivity(targetTask, "adding to task");
        }

        if (!mAvoidMoveToFront && mDoResume) {
            mTargetStack.getStack().moveToFront("reuseOrNewTask", targetTask);
            if (mOptions != null) {
                if (mOptions.getTaskAlwaysOnTop()) {
                    mTargetStack.setAlwaysOnTop(true);
                }
            }
            if (!mTargetStack.isTopStackInDisplayArea() && mService.mInternal.isDreaming()) {
                // Launching underneath dream activity (fullscreen, always-on-top). Run the launch-
                // -behind transition so the Activity gets created and starts in visible state.
                mLaunchTaskBehind = true;
                r.mLaunchTaskBehind = true;
            }
        }

        mService.mUgmInternal.grantUriPermissionUncheckedFromIntent(intentGrants,
                mStartActivity.getUriPermissionsLocked());
        if (mStartActivity.resultTo != null && mStartActivity.resultTo.info != null) {
            // we need to resolve resultTo to a uid as grantImplicitAccess deals explicitly in UIDs
            final PackageManagerInternal pmInternal =
                    mService.getPackageManagerInternalLocked();
            final int resultToUid = pmInternal.getPackageUidInternal(
                            mStartActivity.resultTo.info.packageName, 0, mStartActivity.mUserId);
            pmInternal.grantImplicitAccess(mStartActivity.mUserId, mIntent,
                    UserHandle.getAppId(mStartActivity.info.applicationInfo.uid) /*recipient*/,
                    resultToUid /*visible*/, true /*direct*/);
        }
        if (newTask) {
            EventLogTags.writeWmCreateTask(mStartActivity.mUserId,
                    mStartActivity.getTask().mTaskId);
        }
        mStartActivity.logStartActivity(
                EventLogTags.WM_CREATE_ACTIVITY, mStartActivity.getTask());

        mTargetStack.mLastPausedActivity = null;

        mRootWindowContainer.sendPowerHintForLaunchStartIfNeeded(
                false /* forceSend */, mStartActivity);

        mTargetStack.startActivityLocked(mStartActivity,
                topStack != null ? topStack.getTopNonFinishingActivity() : null, newTask,
                mKeepCurTransition, mOptions);
        if (mDoResume) {
            final ActivityRecord topTaskActivity =
                    mStartActivity.getTask().topRunningActivityLocked();
            if (!mTargetStack.isTopActivityFocusable()
                    || (topTaskActivity != null && topTaskActivity.isTaskOverlay()
                    && mStartActivity != topTaskActivity)) {
                // If the activity is not focusable, we can't resume it, but still would like to
                // make sure it becomes visible as it starts (this will also trigger entry
                // animation). An example of this are PIP activities.
                // Also, we don't want to resume activities in a task that currently has an overlay
                // as the starting activity just needs to be in the visible paused state until the
                // over is removed.
                // Passing {@code null} as the start parameter ensures all activities are made
                // visible.
                mTargetStack.ensureActivitiesVisible(null /* starting */,
                        0 /* configChanges */, !PRESERVE_WINDOWS);
                // Go ahead and tell window manager to execute app transition for this activity
                // since the app transition will not be triggered through the resume channel.
                mTargetStack.getDisplay().mDisplayContent.executeAppTransition();
            } else {
                // If the target stack was not previously focusable (previous top running activity
                // on that stack was not visible) then any prior calls to move the stack to the
                // will not update the focused stack.  If starting the new activity now allows the
                // task stack to be focusable, then ensure that we now update the focused stack
                // accordingly.
                if (mTargetStack.isTopActivityFocusable()
                        && !mRootWindowContainer.isTopDisplayFocusedStack(mTargetStack)) {
                    mTargetStack.moveToFront("startActivityInner");
                }
                mRootWindowContainer.resumeFocusedStacksTopActivities(
                        mTargetStack, mStartActivity, mOptions);
            }
        }
        mRootWindowContainer.updateUserStack(mStartActivity.mUserId, mTargetStack);

        // Update the recent tasks list immediately when the activity starts
        mSupervisor.mRecentTasks.add(mStartActivity.getTask());
        mSupervisor.handleNonResizableTaskIfNeeded(mStartActivity.getTask(),
                mPreferredWindowingMode, mPreferredTaskDisplayArea, mTargetStack);

        return START_SUCCESS;
    }