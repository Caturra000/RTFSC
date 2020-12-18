    final void processNextBroadcastLocked(boolean fromMsg, boolean skipOomAdj) {
        BroadcastRecord r;

        if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST, "processNextBroadcast ["
                + mQueueName + "]: "
                + mParallelBroadcasts.size() + " parallel broadcasts; "
                + mDispatcher.describeStateLocked());

        mService.updateCpuStats();

        if (fromMsg) {
            mBroadcastsScheduled = false;
        }

        // First, deliver any non-serialized broadcasts right away.
        while (mParallelBroadcasts.size() > 0) {
            r = mParallelBroadcasts.remove(0);
            r.dispatchTime = SystemClock.uptimeMillis();
            r.dispatchClockTime = System.currentTimeMillis();

            if (Trace.isTagEnabled(Trace.TRACE_TAG_ACTIVITY_MANAGER)) {
                Trace.asyncTraceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER,
                    createBroadcastTraceTitle(r, BroadcastRecord.DELIVERY_PENDING),
                    System.identityHashCode(r));
                Trace.asyncTraceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER,
                    createBroadcastTraceTitle(r, BroadcastRecord.DELIVERY_DELIVERED),
                    System.identityHashCode(r));
            }

            final int N = r.receivers.size();
            if (DEBUG_BROADCAST_LIGHT) Slog.v(TAG_BROADCAST, "Processing parallel broadcast ["
                    + mQueueName + "] " + r);
            for (int i=0; i<N; i++) {
                Object target = r.receivers.get(i);
                if (DEBUG_BROADCAST)  Slog.v(TAG_BROADCAST,
                        "Delivering non-ordered on [" + mQueueName + "] to registered "
                        + target + ": " + r);
                deliverToRegisteredReceiverLocked(r, (BroadcastFilter)target, false, i);
            }
            addBroadcastToHistoryLocked(r);
            if (DEBUG_BROADCAST_LIGHT) Slog.v(TAG_BROADCAST, "Done with parallel broadcast ["
                    + mQueueName + "] " + r);
        }

        // Now take care of the next serialized one...

        // If we are waiting for a process to come up to handle the next
        // broadcast, then do nothing at this point.  Just in case, we
        // check that the process we're waiting for still exists.
        if (mPendingBroadcast != null) {
            if (DEBUG_BROADCAST_LIGHT) Slog.v(TAG_BROADCAST,
                    "processNextBroadcast [" + mQueueName + "]: waiting for "
                    + mPendingBroadcast.curApp);

            boolean isDead;
            if (mPendingBroadcast.curApp.pid > 0) {
                synchronized (mService.mPidsSelfLocked) {
                    ProcessRecord proc = mService.mPidsSelfLocked.get(
                            mPendingBroadcast.curApp.pid);
                    isDead = proc == null || proc.isCrashing();
                }
            } else {
                final ProcessRecord proc = mService.mProcessList.mProcessNames.get(
                        mPendingBroadcast.curApp.processName, mPendingBroadcast.curApp.uid);
                isDead = proc == null || !proc.pendingStart;
            }
            if (!isDead) {
                // It's still alive, so keep waiting
                return;
            } else {
                Slog.w(TAG, "pending app  ["
                        + mQueueName + "]" + mPendingBroadcast.curApp
                        + " died before responding to broadcast");
                mPendingBroadcast.state = BroadcastRecord.IDLE;
                mPendingBroadcast.nextReceiver = mPendingBroadcastRecvIndex;
                mPendingBroadcast = null;
            }
        }

        boolean looped = false;

        do {
            final long now = SystemClock.uptimeMillis();
            r = mDispatcher.getNextBroadcastLocked(now);

            if (r == null) {
                // No more broadcasts are deliverable right now, so all done!
                mDispatcher.scheduleDeferralCheckLocked(false);
                mService.scheduleAppGcsLocked();
                if (looped) {
                    // If we had finished the last ordered broadcast, then
                    // make sure all processes have correct oom and sched
                    // adjustments.
                    mService.updateOomAdjLocked(OomAdjuster.OOM_ADJ_REASON_START_RECEIVER);
                }

                // when we have no more ordered broadcast on this queue, stop logging
                if (mService.mUserController.mBootCompleted && mLogLatencyMetrics) {
                    mLogLatencyMetrics = false;
                }

                return;
            }

            boolean forceReceive = false;

            // Ensure that even if something goes awry with the timeout
            // detection, we catch "hung" broadcasts here, discard them,
            // and continue to make progress.
            //
            // This is only done if the system is ready so that early-stage receivers
            // don't get executed with timeouts; and of course other timeout-
            // exempt broadcasts are ignored.
            int numReceivers = (r.receivers != null) ? r.receivers.size() : 0;
            if (mService.mProcessesReady && !r.timeoutExempt && r.dispatchTime > 0) {
                if ((numReceivers > 0) &&
                        (now > r.dispatchTime + (2 * mConstants.TIMEOUT * numReceivers))) {
                    Slog.w(TAG, "Hung broadcast ["
                            + mQueueName + "] discarded after timeout failure:"
                            + " now=" + now
                            + " dispatchTime=" + r.dispatchTime
                            + " startTime=" + r.receiverTime
                            + " intent=" + r.intent
                            + " numReceivers=" + numReceivers
                            + " nextReceiver=" + r.nextReceiver
                            + " state=" + r.state);
                    broadcastTimeoutLocked(false); // forcibly finish this broadcast
                    forceReceive = true;
                    r.state = BroadcastRecord.IDLE;
                }
            }

            if (r.state != BroadcastRecord.IDLE) {
                if (DEBUG_BROADCAST) Slog.d(TAG_BROADCAST,
                        "processNextBroadcast("
                        + mQueueName + ") called when not idle (state="
                        + r.state + ")");
                return;
            }

            // Is the current broadcast is done for any reason?
            if (r.receivers == null || r.nextReceiver >= numReceivers
                    || r.resultAbort || forceReceive) {
                // Send the final result if requested
                if (r.resultTo != null) {
                    boolean sendResult = true;

                    // if this was part of a split/deferral complex, update the refcount and only
                    // send the completion when we clear all of them
                    if (r.splitToken != 0) {
                        int newCount = mSplitRefcounts.get(r.splitToken) - 1;
                        if (newCount == 0) {
                            // done!  clear out this record's bookkeeping and deliver
                            if (DEBUG_BROADCAST_DEFERRAL) {
                                Slog.i(TAG_BROADCAST,
                                        "Sending broadcast completion for split token "
                                        + r.splitToken + " : " + r.intent.getAction());
                            }
                            mSplitRefcounts.delete(r.splitToken);
                        } else {
                            // still have some split broadcast records in flight; update refcount
                            // and hold off on the callback
                            if (DEBUG_BROADCAST_DEFERRAL) {
                                Slog.i(TAG_BROADCAST,
                                        "Result refcount now " + newCount + " for split token "
                                        + r.splitToken + " : " + r.intent.getAction()
                                        + " - not sending completion yet");
                            }
                            sendResult = false;
                            mSplitRefcounts.put(r.splitToken, newCount);
                        }
                    }
                    if (sendResult) {
                        try {
                            if (DEBUG_BROADCAST) {
                                Slog.i(TAG_BROADCAST, "Finishing broadcast [" + mQueueName + "] "
                                        + r.intent.getAction() + " app=" + r.callerApp);
                            }
                            performReceiveLocked(r.callerApp, r.resultTo,
                                    new Intent(r.intent), r.resultCode,
                                    r.resultData, r.resultExtras, false, false, r.userId);
                            // Set this to null so that the reference
                            // (local and remote) isn't kept in the mBroadcastHistory.
                            r.resultTo = null;
                        } catch (RemoteException e) {
                            r.resultTo = null;
                            Slog.w(TAG, "Failure ["
                                    + mQueueName + "] sending broadcast result of "
                                    + r.intent, e);
                        }
                    }
                }

                if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST, "Cancelling BROADCAST_TIMEOUT_MSG");
                cancelBroadcastTimeoutLocked();

                if (DEBUG_BROADCAST_LIGHT) Slog.v(TAG_BROADCAST,
                        "Finished with ordered broadcast " + r);

                // ... and on to the next...
                addBroadcastToHistoryLocked(r);
                if (r.intent.getComponent() == null && r.intent.getPackage() == null
                        && (r.intent.getFlags()&Intent.FLAG_RECEIVER_REGISTERED_ONLY) == 0) {
                    // This was an implicit broadcast... let's record it for posterity.
                    mService.addBroadcastStatLocked(r.intent.getAction(), r.callerPackage,
                            r.manifestCount, r.manifestSkipCount, r.finishTime-r.dispatchTime);
                }
                mDispatcher.retireBroadcastLocked(r);
                r = null;
                looped = true;
                continue;
            }

            // Check whether the next receiver is under deferral policy, and handle that
            // accordingly.  If the current broadcast was already part of deferred-delivery
            // tracking, we know that it must now be deliverable as-is without re-deferral.
            if (!r.deferred) {
                final int receiverUid = r.getReceiverUid(r.receivers.get(r.nextReceiver));
                if (mDispatcher.isDeferringLocked(receiverUid)) {
                    if (DEBUG_BROADCAST_DEFERRAL) {
                        Slog.i(TAG_BROADCAST, "Next receiver in " + r + " uid " + receiverUid
                                + " at " + r.nextReceiver + " is under deferral");
                    }
                    // If this is the only (remaining) receiver in the broadcast, "splitting"
                    // doesn't make sense -- just defer it as-is and retire it as the
                    // currently active outgoing broadcast.
                    BroadcastRecord defer;
                    if (r.nextReceiver + 1 == numReceivers) {
                        if (DEBUG_BROADCAST_DEFERRAL) {
                            Slog.i(TAG_BROADCAST, "Sole receiver of " + r
                                    + " is under deferral; setting aside and proceeding");
                        }
                        defer = r;
                        mDispatcher.retireBroadcastLocked(r);
                    } else {
                        // Nontrivial case; split out 'uid's receivers to a new broadcast record
                        // and defer that, then loop and pick up continuing delivery of the current
                        // record (now absent those receivers).

                        // The split operation is guaranteed to match at least at 'nextReceiver'
                        defer = r.splitRecipientsLocked(receiverUid, r.nextReceiver);
                        if (DEBUG_BROADCAST_DEFERRAL) {
                            Slog.i(TAG_BROADCAST, "Post split:");
                            Slog.i(TAG_BROADCAST, "Original broadcast receivers:");
                            for (int i = 0; i < r.receivers.size(); i++) {
                                Slog.i(TAG_BROADCAST, "  " + r.receivers.get(i));
                            }
                            Slog.i(TAG_BROADCAST, "Split receivers:");
                            for (int i = 0; i < defer.receivers.size(); i++) {
                                Slog.i(TAG_BROADCAST, "  " + defer.receivers.get(i));
                            }
                        }
                        // Track completion refcount as well if relevant
                        if (r.resultTo != null) {
                            int token = r.splitToken;
                            if (token == 0) {
                                // first split of this record; refcount for 'r' and 'deferred'
                                r.splitToken = defer.splitToken = nextSplitTokenLocked();
                                mSplitRefcounts.put(r.splitToken, 2);
                                if (DEBUG_BROADCAST_DEFERRAL) {
                                    Slog.i(TAG_BROADCAST,
                                            "Broadcast needs split refcount; using new token "
                                            + r.splitToken);
                                }
                            } else {
                                // new split from an already-refcounted situation; increment count
                                final int curCount = mSplitRefcounts.get(token);
                                if (DEBUG_BROADCAST_DEFERRAL) {
                                    if (curCount == 0) {
                                        Slog.wtf(TAG_BROADCAST,
                                                "Split refcount is zero with token for " + r);
                                    }
                                }
                                mSplitRefcounts.put(token, curCount + 1);
                                if (DEBUG_BROADCAST_DEFERRAL) {
                                    Slog.i(TAG_BROADCAST, "New split count for token " + token
                                            + " is " + (curCount + 1));
                                }
                            }
                        }
                    }
                    mDispatcher.addDeferredBroadcast(receiverUid, defer);
                    r = null;
                    looped = true;
                    continue;
                }
            }
        } while (r == null);

        // Get the next receiver...
        int recIdx = r.nextReceiver++;

        // Keep track of when this receiver started, and make sure there
        // is a timeout message pending to kill it if need be.
        r.receiverTime = SystemClock.uptimeMillis();
        if (recIdx == 0) {
            r.dispatchTime = r.receiverTime;
            r.dispatchClockTime = System.currentTimeMillis();

            if (mLogLatencyMetrics) {
                FrameworkStatsLog.write(
                        FrameworkStatsLog.BROADCAST_DISPATCH_LATENCY_REPORTED,
                        r.dispatchClockTime - r.enqueueClockTime);
            }

            if (Trace.isTagEnabled(Trace.TRACE_TAG_ACTIVITY_MANAGER)) {
                Trace.asyncTraceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER,
                    createBroadcastTraceTitle(r, BroadcastRecord.DELIVERY_PENDING),
                    System.identityHashCode(r));
                Trace.asyncTraceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER,
                    createBroadcastTraceTitle(r, BroadcastRecord.DELIVERY_DELIVERED),
                    System.identityHashCode(r));
            }
            if (DEBUG_BROADCAST_LIGHT) Slog.v(TAG_BROADCAST, "Processing ordered broadcast ["
                    + mQueueName + "] " + r);
        }
        if (! mPendingBroadcastTimeoutMessage) {
            long timeoutTime = r.receiverTime + mConstants.TIMEOUT;
            if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST,
                    "Submitting BROADCAST_TIMEOUT_MSG ["
                    + mQueueName + "] for " + r + " at " + timeoutTime);
            setBroadcastTimeoutLocked(timeoutTime);
        }

        final BroadcastOptions brOptions = r.options;
        final Object nextReceiver = r.receivers.get(recIdx);

        if (nextReceiver instanceof BroadcastFilter) {
            // Simple case: this is a registered receiver who gets
            // a direct call.
            BroadcastFilter filter = (BroadcastFilter)nextReceiver;
            if (DEBUG_BROADCAST)  Slog.v(TAG_BROADCAST,
                    "Delivering ordered ["
                    + mQueueName + "] to registered "
                    + filter + ": " + r);
            deliverToRegisteredReceiverLocked(r, filter, r.ordered, recIdx);
            if (r.receiver == null || !r.ordered) {
                // The receiver has already finished, so schedule to
                // process the next one.
                if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST, "Quick finishing ["
                        + mQueueName + "]: ordered="
                        + r.ordered + " receiver=" + r.receiver);
                r.state = BroadcastRecord.IDLE;
                scheduleBroadcastsLocked();
            } else {
                if (filter.receiverList != null) {
                    maybeAddAllowBackgroundActivityStartsToken(filter.receiverList.app, r);
                    // r is guaranteed ordered at this point, so we know finishReceiverLocked()
                    // will get a callback and handle the activity start token lifecycle.
                }
                if (brOptions != null && brOptions.getTemporaryAppWhitelistDuration() > 0) {
                    scheduleTempWhitelistLocked(filter.owningUid,
                            brOptions.getTemporaryAppWhitelistDuration(), r);
                }
            }
            return;
        }

        // Hard case: need to instantiate the receiver, possibly
        // starting its application process to host it.

        ResolveInfo info =
            (ResolveInfo)nextReceiver;
        ComponentName component = new ComponentName(
                info.activityInfo.applicationInfo.packageName,
                info.activityInfo.name);

        boolean skip = false;
        if (brOptions != null &&
                (info.activityInfo.applicationInfo.targetSdkVersion
                        < brOptions.getMinManifestReceiverApiLevel() ||
                info.activityInfo.applicationInfo.targetSdkVersion
                        > brOptions.getMaxManifestReceiverApiLevel())) {
            skip = true;
        }
        if (!skip && !mService.validateAssociationAllowedLocked(r.callerPackage, r.callingUid,
                component.getPackageName(), info.activityInfo.applicationInfo.uid)) {
            Slog.w(TAG, "Association not allowed: broadcasting "
                    + r.intent.toString()
                    + " from " + r.callerPackage + " (pid=" + r.callingPid
                    + ", uid=" + r.callingUid + ") to " + component.flattenToShortString());
            skip = true;
        }
        if (!skip) {
            skip = !mService.mIntentFirewall.checkBroadcast(r.intent, r.callingUid,
                    r.callingPid, r.resolvedType, info.activityInfo.applicationInfo.uid);
            if (skip) {
                Slog.w(TAG, "Firewall blocked: broadcasting "
                        + r.intent.toString()
                        + " from " + r.callerPackage + " (pid=" + r.callingPid
                        + ", uid=" + r.callingUid + ") to " + component.flattenToShortString());
            }
        }
        int perm = mService.checkComponentPermission(info.activityInfo.permission,
                r.callingPid, r.callingUid, info.activityInfo.applicationInfo.uid,
                info.activityInfo.exported);
        if (!skip && perm != PackageManager.PERMISSION_GRANTED) {
            if (!info.activityInfo.exported) {
                Slog.w(TAG, "Permission Denial: broadcasting "
                        + r.intent.toString()
                        + " from " + r.callerPackage + " (pid=" + r.callingPid
                        + ", uid=" + r.callingUid + ")"
                        + " is not exported from uid " + info.activityInfo.applicationInfo.uid
                        + " due to receiver " + component.flattenToShortString());
            } else {
                Slog.w(TAG, "Permission Denial: broadcasting "
                        + r.intent.toString()
                        + " from " + r.callerPackage + " (pid=" + r.callingPid
                        + ", uid=" + r.callingUid + ")"
                        + " requires " + info.activityInfo.permission
                        + " due to receiver " + component.flattenToShortString());
            }
            skip = true;
        } else if (!skip && info.activityInfo.permission != null) {
            final int opCode = AppOpsManager.permissionToOpCode(info.activityInfo.permission);
            if (opCode != AppOpsManager.OP_NONE
                    && mService.getAppOpsManager().noteOpNoThrow(opCode, r.callingUid, r.callerPackage,
                    r.callerFeatureId, "") != AppOpsManager.MODE_ALLOWED) {
                Slog.w(TAG, "Appop Denial: broadcasting "
                        + r.intent.toString()
                        + " from " + r.callerPackage + " (pid="
                        + r.callingPid + ", uid=" + r.callingUid + ")"
                        + " requires appop " + AppOpsManager.permissionToOp(
                                info.activityInfo.permission)
                        + " due to registered receiver "
                        + component.flattenToShortString());
                skip = true;
            }
        }
        if (!skip && info.activityInfo.applicationInfo.uid != Process.SYSTEM_UID &&
            r.requiredPermissions != null && r.requiredPermissions.length > 0) {
            for (int i = 0; i < r.requiredPermissions.length; i++) {
                String requiredPermission = r.requiredPermissions[i];
                try {
                    perm = AppGlobals.getPackageManager().
                            checkPermission(requiredPermission,
                                    info.activityInfo.applicationInfo.packageName,
                                    UserHandle
                                            .getUserId(info.activityInfo.applicationInfo.uid));
                } catch (RemoteException e) {
                    perm = PackageManager.PERMISSION_DENIED;
                }
                if (perm != PackageManager.PERMISSION_GRANTED) {
                    Slog.w(TAG, "Permission Denial: receiving "
                            + r.intent + " to "
                            + component.flattenToShortString()
                            + " requires " + requiredPermission
                            + " due to sender " + r.callerPackage
                            + " (uid " + r.callingUid + ")");
                    skip = true;
                    break;
                }
                int appOp = AppOpsManager.permissionToOpCode(requiredPermission);
                if (appOp != AppOpsManager.OP_NONE && appOp != r.appOp
                        && mService.getAppOpsManager().noteOpNoThrow(appOp,
                        info.activityInfo.applicationInfo.uid, info.activityInfo.packageName,
                        null /* default featureId */, "")
                        != AppOpsManager.MODE_ALLOWED) {
                    Slog.w(TAG, "Appop Denial: receiving "
                            + r.intent + " to "
                            + component.flattenToShortString()
                            + " requires appop " + AppOpsManager.permissionToOp(
                            requiredPermission)
                            + " due to sender " + r.callerPackage
                            + " (uid " + r.callingUid + ")");
                    skip = true;
                    break;
                }
            }
        }
        if (!skip && r.appOp != AppOpsManager.OP_NONE
                && mService.getAppOpsManager().noteOpNoThrow(r.appOp,
                info.activityInfo.applicationInfo.uid, info.activityInfo.packageName,
                null  /* default featureId */, "")
                != AppOpsManager.MODE_ALLOWED) {
            Slog.w(TAG, "Appop Denial: receiving "
                    + r.intent + " to "
                    + component.flattenToShortString()
                    + " requires appop " + AppOpsManager.opToName(r.appOp)
                    + " due to sender " + r.callerPackage
                    + " (uid " + r.callingUid + ")");
            skip = true;
        }
        boolean isSingleton = false;
        try {
            isSingleton = mService.isSingleton(info.activityInfo.processName,
                    info.activityInfo.applicationInfo,
                    info.activityInfo.name, info.activityInfo.flags);
        } catch (SecurityException e) {
            Slog.w(TAG, e.getMessage());
            skip = true;
        }
        if ((info.activityInfo.flags&ActivityInfo.FLAG_SINGLE_USER) != 0) {
            if (ActivityManager.checkUidPermission(
                    android.Manifest.permission.INTERACT_ACROSS_USERS,
                    info.activityInfo.applicationInfo.uid)
                            != PackageManager.PERMISSION_GRANTED) {
                Slog.w(TAG, "Permission Denial: Receiver " + component.flattenToShortString()
                        + " requests FLAG_SINGLE_USER, but app does not hold "
                        + android.Manifest.permission.INTERACT_ACROSS_USERS);
                skip = true;
            }
        }
        if (!skip && info.activityInfo.applicationInfo.isInstantApp()
                && r.callingUid != info.activityInfo.applicationInfo.uid) {
            Slog.w(TAG, "Instant App Denial: receiving "
                    + r.intent
                    + " to " + component.flattenToShortString()
                    + " due to sender " + r.callerPackage
                    + " (uid " + r.callingUid + ")"
                    + " Instant Apps do not support manifest receivers");
            skip = true;
        }
        if (!skip && r.callerInstantApp
                && (info.activityInfo.flags & ActivityInfo.FLAG_VISIBLE_TO_INSTANT_APP) == 0
                && r.callingUid != info.activityInfo.applicationInfo.uid) {
            Slog.w(TAG, "Instant App Denial: receiving "
                    + r.intent
                    + " to " + component.flattenToShortString()
                    + " requires receiver have visibleToInstantApps set"
                    + " due to sender " + r.callerPackage
                    + " (uid " + r.callingUid + ")");
            skip = true;
        }
        if (r.curApp != null && r.curApp.isCrashing()) {
            // If the target process is crashing, just skip it.
            Slog.w(TAG, "Skipping deliver ordered [" + mQueueName + "] " + r
                    + " to " + r.curApp + ": process crashing");
            skip = true;
        }
        if (!skip) {
            boolean isAvailable = false;
            try {
                isAvailable = AppGlobals.getPackageManager().isPackageAvailable(
                        info.activityInfo.packageName,
                        UserHandle.getUserId(info.activityInfo.applicationInfo.uid));
            } catch (Exception e) {
                // all such failures mean we skip this receiver
                Slog.w(TAG, "Exception getting recipient info for "
                        + info.activityInfo.packageName, e);
            }
            if (!isAvailable) {
                if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST,
                        "Skipping delivery to " + info.activityInfo.packageName + " / "
                        + info.activityInfo.applicationInfo.uid
                        + " : package no longer available");
                skip = true;
            }
        }

        // If permissions need a review before any of the app components can run, we drop
        // the broadcast and if the calling app is in the foreground and the broadcast is
        // explicit we launch the review UI passing it a pending intent to send the skipped
        // broadcast.
        if (!skip) {
            if (!requestStartTargetPermissionsReviewIfNeededLocked(r,
                    info.activityInfo.packageName, UserHandle.getUserId(
                            info.activityInfo.applicationInfo.uid))) {
                skip = true;
            }
        }

        // This is safe to do even if we are skipping the broadcast, and we need
        // this information now to evaluate whether it is going to be allowed to run.
        final int receiverUid = info.activityInfo.applicationInfo.uid;
        // If it's a singleton, it needs to be the same app or a special app
        if (r.callingUid != Process.SYSTEM_UID && isSingleton
                && mService.isValidSingletonCall(r.callingUid, receiverUid)) {
            info.activityInfo = mService.getActivityInfoForUser(info.activityInfo, 0);
        }
        String targetProcess = info.activityInfo.processName;
        ProcessRecord app = mService.getProcessRecordLocked(targetProcess,
                info.activityInfo.applicationInfo.uid, false);

        if (!skip) {
            final int allowed = mService.getAppStartModeLocked(
                    info.activityInfo.applicationInfo.uid, info.activityInfo.packageName,
                    info.activityInfo.applicationInfo.targetSdkVersion, -1, true, false, false);
            if (allowed != ActivityManager.APP_START_MODE_NORMAL) {
                // We won't allow this receiver to be launched if the app has been
                // completely disabled from launches, or it was not explicitly sent
                // to it and the app is in a state that should not receive it
                // (depending on how getAppStartModeLocked has determined that).
                if (allowed == ActivityManager.APP_START_MODE_DISABLED) {
                    Slog.w(TAG, "Background execution disabled: receiving "
                            + r.intent + " to "
                            + component.flattenToShortString());
                    skip = true;
                } else if (((r.intent.getFlags()&Intent.FLAG_RECEIVER_EXCLUDE_BACKGROUND) != 0)
                        || (r.intent.getComponent() == null
                            && r.intent.getPackage() == null
                            && ((r.intent.getFlags()
                                    & Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND) == 0)
                            && !isSignaturePerm(r.requiredPermissions))) {
                    mService.addBackgroundCheckViolationLocked(r.intent.getAction(),
                            component.getPackageName());
                    Slog.w(TAG, "Background execution not allowed: receiving "
                            + r.intent + " to "
                            + component.flattenToShortString());
                    skip = true;
                }
            }
        }

        if (!skip && !Intent.ACTION_SHUTDOWN.equals(r.intent.getAction())
                && !mService.mUserController
                .isUserRunning(UserHandle.getUserId(info.activityInfo.applicationInfo.uid),
                        0 /* flags */)) {
            skip = true;
            Slog.w(TAG,
                    "Skipping delivery to " + info.activityInfo.packageName + " / "
                            + info.activityInfo.applicationInfo.uid + " : user is not running");
        }

        if (skip) {
            if (DEBUG_BROADCAST)  Slog.v(TAG_BROADCAST,
                    "Skipping delivery of ordered [" + mQueueName + "] "
                    + r + " for reason described above");
            r.delivery[recIdx] = BroadcastRecord.DELIVERY_SKIPPED;
            r.receiver = null;
            r.curFilter = null;
            r.state = BroadcastRecord.IDLE;
            r.manifestSkipCount++;
            scheduleBroadcastsLocked();
            return;
        }
        r.manifestCount++;

        r.delivery[recIdx] = BroadcastRecord.DELIVERY_DELIVERED;
        r.state = BroadcastRecord.APP_RECEIVE;
        r.curComponent = component;
        r.curReceiver = info.activityInfo;
        if (DEBUG_MU && r.callingUid > UserHandle.PER_USER_RANGE) {
            Slog.v(TAG_MU, "Updated broadcast record activity info for secondary user, "
                    + info.activityInfo + ", callingUid = " + r.callingUid + ", uid = "
                    + receiverUid);
        }

        final boolean isActivityCapable =
                (brOptions != null && brOptions.getTemporaryAppWhitelistDuration() > 0);
        if (isActivityCapable) {
            scheduleTempWhitelistLocked(receiverUid,
                    brOptions.getTemporaryAppWhitelistDuration(), r);
        }

        // Broadcast is being executed, its package can't be stopped.
        try {
            AppGlobals.getPackageManager().setPackageStoppedState(
                    r.curComponent.getPackageName(), false, r.userId);
        } catch (RemoteException e) {
        } catch (IllegalArgumentException e) {
            Slog.w(TAG, "Failed trying to unstop package "
                    + r.curComponent.getPackageName() + ": " + e);
        }

        // Is this receiver's application already running?
        if (app != null && app.thread != null && !app.killed) {
            try {
                app.addPackage(info.activityInfo.packageName,
                        info.activityInfo.applicationInfo.longVersionCode, mService.mProcessStats);
                maybeAddAllowBackgroundActivityStartsToken(app, r);
                processCurBroadcastLocked(r, app, skipOomAdj);
                return;
            } catch (RemoteException e) {
                Slog.w(TAG, "Exception when sending broadcast to "
                      + r.curComponent, e);
            } catch (RuntimeException e) {
                Slog.wtf(TAG, "Failed sending broadcast to "
                        + r.curComponent + " with " + r.intent, e);
                // If some unexpected exception happened, just skip
                // this broadcast.  At this point we are not in the call
                // from a client, so throwing an exception out from here
                // will crash the entire system instead of just whoever
                // sent the broadcast.
                logBroadcastReceiverDiscardLocked(r);
                finishReceiverLocked(r, r.resultCode, r.resultData,
                        r.resultExtras, r.resultAbort, false);
                scheduleBroadcastsLocked();
                // We need to reset the state if we failed to start the receiver.
                r.state = BroadcastRecord.IDLE;
                return;
            }

            // If a dead object exception was thrown -- fall through to
            // restart the application.
        }

        // Not running -- get it started, to be executed when the app comes up.
        if (DEBUG_BROADCAST)  Slog.v(TAG_BROADCAST,
                "Need to start app ["
                + mQueueName + "] " + targetProcess + " for broadcast " + r);
        if ((r.curApp=mService.startProcessLocked(targetProcess,
                info.activityInfo.applicationInfo, true,
                r.intent.getFlags() | Intent.FLAG_FROM_BACKGROUND,
                new HostingRecord("broadcast", r.curComponent),
                isActivityCapable ? ZYGOTE_POLICY_FLAG_LATENCY_SENSITIVE : ZYGOTE_POLICY_FLAG_EMPTY,
                (r.intent.getFlags()&Intent.FLAG_RECEIVER_BOOT_UPGRADE) != 0, false, false))
                        == null) {
            // Ah, this recipient is unavailable.  Finish it if necessary,
            // and mark the broadcast record as ready for the next.
            Slog.w(TAG, "Unable to launch app "
                    + info.activityInfo.applicationInfo.packageName + "/"
                    + receiverUid + " for broadcast "
                    + r.intent + ": process is bad");
            logBroadcastReceiverDiscardLocked(r);
            finishReceiverLocked(r, r.resultCode, r.resultData,
                    r.resultExtras, r.resultAbort, false);
            scheduleBroadcastsLocked();
            r.state = BroadcastRecord.IDLE;
            return;
        }

        maybeAddAllowBackgroundActivityStartsToken(r.curApp, r);
        mPendingBroadcast = r;
        mPendingBroadcastRecvIndex = recIdx;
    }

