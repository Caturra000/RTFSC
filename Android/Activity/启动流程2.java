    /**
     * Start an activity while most of preliminary checks has been done and caller has been 已经由上层的executeRequest处理完
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
            // 一个优化的方法，对应continueWindowLayout
            mService.deferWindowLayout();
            Trace.traceBegin(Trace.TRACE_TAG_WINDOW_MANAGER, "startActivityInner");
// TRACK BEGIN
            result = startActivityInner(r, sourceRecord, voiceSession, voiceInteractor,
                    startFlags, doResume, options, inTask, restrictedBgActivity, intentGrants);
// TRACK END
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_WINDOW_MANAGER);
            // 该方法的注释：If the start result is success, ensure that the configuration of the started activity matches
            // the current display. Otherwise clean up unassociated containers to avoid leakage.
            // @return the stack where the successful started activity resides.
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

        // mLaunchFlags处理
        computeLaunchingTaskFlags();

        // 由mSourceRecord获得mSourceStack
        computeSourceStack();

        mIntent.setFlags(mLaunchFlags);

        // Activity不插入已存在的task则返回null
        final Task reusedTask = getReusableTask(); 
        // If requested, freeze the task list
        if (mOptions != null && mOptions.freezeRecentTasksReordering()                      // TODO
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

        mTargetStack.startActivityLocked(mStartActivity, topStack.getTopNonFinishingActivity(),
                newTask, mKeepCurTransition, mOptions);
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
// TRACK START
                mRootWindowContainer.resumeFocusedStacksTopActivities(
                        mTargetStack, mStartActivity, mOptions);
// TRACK END
            }
        }
        mRootWindowContainer.updateUserStack(mStartActivity.mUserId, mTargetStack);

        // Update the recent tasks list immediately when the activity starts
        mSupervisor.mRecentTasks.add(mStartActivity.getTask());
        mSupervisor.handleNonResizableTaskIfNeeded(mStartActivity.getTask(),
                mPreferredWindowingMode, mPreferredTaskDisplayArea, mTargetStack);

        return START_SUCCESS;
    }
