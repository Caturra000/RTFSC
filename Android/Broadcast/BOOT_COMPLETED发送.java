
    // 文件分布：frameworks/base/core/java/android/app/ActivityManagerInternal.java

    final void finishBooting() {
        TimingsTraceAndSlog t = new TimingsTraceAndSlog(TAG + "Timing",
                Trace.TRACE_TAG_ACTIVITY_MANAGER);
        t.traceBegin("FinishBooting");

        synchronized (this) {
            if (!mBootAnimationComplete) {
                mCallFinishBooting = true;
                return;
            }
            mCallFinishBooting = false;
        }

        // Let the ART runtime in zygote and system_server know that the boot completed.
        ZYGOTE_PROCESS.bootCompleted();
        VMRuntime.bootCompleted();

        IntentFilter pkgFilter = new IntentFilter();
        pkgFilter.addAction(Intent.ACTION_QUERY_PACKAGE_RESTART);
        pkgFilter.addDataScheme("package");
        mContext.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String[] pkgs = intent.getStringArrayExtra(Intent.EXTRA_PACKAGES);
                if (pkgs != null) {
                    for (String pkg : pkgs) {
                        synchronized (ActivityManagerService.this) {
                            if (forceStopPackageLocked(pkg, -1, false, false, false, false, false,
                                    0, "query restart")) {
                                setResultCode(Activity.RESULT_OK);
                                return;
                            }
                        }
                    }
                }
            }
        }, pkgFilter);

        // Inform checkpointing systems of success
        try {
            // This line is needed to CTS test for the correct exception handling
            // See b/138952436#comment36 for context
            Slog.i(TAG, "About to commit checkpoint");
            IStorageManager storageManager = PackageHelper.getStorageManager();
            storageManager.commitChanges();
        } catch (Exception e) {
            PowerManager pm = (PowerManager)
                     mInjector.getContext().getSystemService(Context.POWER_SERVICE);
            pm.reboot("Checkpoint commit failed");
        }

        // Let system services know.
        mSystemServiceManager.startBootPhase(t, SystemService.PHASE_BOOT_COMPLETED);

        synchronized (this) {
            // Ensure that any processes we had put on hold are now started
            // up.
            final int NP = mProcessesOnHold.size();
            if (NP > 0) {
                ArrayList<ProcessRecord> procs =
                    new ArrayList<ProcessRecord>(mProcessesOnHold);
                for (int ip=0; ip<NP; ip++) {
                    if (DEBUG_PROCESSES) Slog.v(TAG_PROCESSES, "Starting process on hold: "
                            + procs.get(ip));
                    mProcessList.startProcessLocked(procs.get(ip),
                            new HostingRecord("on-hold"),
                            ZYGOTE_POLICY_FLAG_BATCH_LAUNCH);
                }
            }
            if (mFactoryTest == FactoryTest.FACTORY_TEST_LOW_LEVEL) {
                return;
            }
            // Start looking for apps that are abusing wake locks.
            Message nmsg = mHandler.obtainMessage(CHECK_EXCESSIVE_POWER_USE_MSG);
            mHandler.sendMessageDelayed(nmsg, mConstants.POWER_CHECK_INTERVAL);
            // Check if we are performing userspace reboot before setting sys.boot_completed to
            // avoid race with init reseting sys.init.userspace_reboot.in_progress once sys
            // .boot_completed is 1.
            if (InitProperties.userspace_reboot_in_progress().orElse(false)) {
                UserspaceRebootLogger.noteUserspaceRebootSuccess();
            }
            // Tell anyone interested that we are done booting!
            SystemProperties.set("sys.boot_completed", "1");

            // And trigger dev.bootcomplete if we are not showing encryption progress
            if (!"trigger_restart_min_framework".equals(VoldProperties.decrypt().orElse(""))
                    || "".equals(VoldProperties.encrypt_progress().orElse(""))) {
                SystemProperties.set("dev.bootcomplete", "1");
            }
// TRACK BEGIN
            mUserController.sendBootCompleted(
                    new IIntentReceiver.Stub() {
                        @Override
                        public void performReceive(Intent intent, int resultCode,
                                String data, Bundle extras, boolean ordered,
                                boolean sticky, int sendingUser) {
                            synchronized (ActivityManagerService.this) {
                                mOomAdjuster.mCachedAppOptimizer.compactAllSystem();
                                requestPssAllProcsLocked(SystemClock.uptimeMillis(), true, false);
                            }
                        }
                    });
// TRACK END
            maybeLogUserspaceRebootEvent();
            mUserController.scheduleStartProfiles();
        }
        // UART is on if init's console service is running, send a warning notification.
        showConsoleNotificationIfActive();

        t.traceEnd();
    }



    // 略



    // 文件分布：frameworks/base/services/core/java/com/android/server/am/UserController.java

    // TL;DR ordered = true

    private void finishUserUnlockedCompleted(UserState uss) {
        final int userId = uss.mHandle.getIdentifier();
        EventLog.writeEvent(EventLogTags.UC_FINISH_USER_UNLOCKED_COMPLETED, userId);
        synchronized (mLock) {
            // Bail if we ended up with a stale user
            if (mStartedUsers.get(uss.mHandle.getIdentifier()) != uss) return;
        }
        UserInfo userInfo = getUserInfo(userId);
        if (userInfo == null) {
            return;
        }
        // Only keep marching forward if user is actually unlocked
        if (!StorageManager.isUserKeyUnlocked(userId)) return;

        // Remember that we logged in
        mInjector.getUserManager().onUserLoggedIn(userId);

        if (!userInfo.isInitialized()) {
            if (userId != UserHandle.USER_SYSTEM) {
                Slog.d(TAG, "Initializing user #" + userId);
                Intent intent = new Intent(Intent.ACTION_USER_INITIALIZE);
                intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND
                        | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
                mInjector.broadcastIntent(intent, null,
                        new IIntentReceiver.Stub() {
                            @Override
                            public void performReceive(Intent intent, int resultCode,
                                    String data, Bundle extras, boolean ordered,
                                    boolean sticky, int sendingUser) {
                                // Note: performReceive is called with mService lock held
                                mInjector.getUserManager().makeInitialized(userInfo.id);
                            }
                        }, 0, null, null, null, AppOpsManager.OP_NONE,
                        null, true, false, MY_PID, SYSTEM_UID, Binder.getCallingUid(),
                        Binder.getCallingPid(), userId);
            }
        }

        if (userInfo.preCreated) {
            Slog.i(TAG, "Stopping pre-created user " + userInfo.toFullString());
            // Pre-created user was started right after creation so services could properly
            // intialize it; it should be stopped right away as it's not really a "real" user.
            // TODO(b/143092698): in the long-term, it might be better to add a onCreateUser()
            // callback on SystemService instead.
            stopUser(userInfo.id, /* force= */ true, /* allowDelayedLocking= */ false,
                    /* stopUserCallback= */ null, /* keyEvictedCallback= */ null);
            return;
        }

        // Spin up app widgets prior to boot-complete, so they can be ready promptly
        mInjector.startUserWidgets(userId);

        mHandler.obtainMessage(USER_UNLOCKED_MSG, userId, 0).sendToTarget();

        Slog.i(TAG, "Posting BOOT_COMPLETED user #" + userId);
        // Do not report secondary users, runtime restarts or first boot/upgrade
        if (userId == UserHandle.USER_SYSTEM
                && !mInjector.isRuntimeRestarted() && !mInjector.isFirstBootOrUpgrade()) {
            final long elapsedTimeMs = SystemClock.elapsedRealtime();
            FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED,
                    FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME__EVENT__FRAMEWORK_BOOT_COMPLETED,
                    elapsedTimeMs);
        }
        // 这个intent就是ACTION_BOOT_COMPLETED
        final Intent bootIntent = new Intent(Intent.ACTION_BOOT_COMPLETED, null);
        bootIntent.putExtra(Intent.EXTRA_USER_HANDLE, userId);
        bootIntent.addFlags(Intent.FLAG_RECEIVER_NO_ABORT
                | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND
                | Intent.FLAG_RECEIVER_OFFLOAD);
        // Widget broadcasts are outbound via FgThread, so to guarantee sequencing
        // we also send the boot_completed broadcast from that thread.
        final int callingUid = Binder.getCallingUid();
        final int callingPid = Binder.getCallingPid();
        FgThread.getHandler().post(() -> {
            mInjector.broadcastIntent(bootIntent, null,
                    new IIntentReceiver.Stub() {
                        @Override
                        public void performReceive(Intent intent, int resultCode, String data,
                                Bundle extras, boolean ordered, boolean sticky, int sendingUser)
                                        throws RemoteException {
                            Slog.i(UserController.TAG, "Finished processing BOOT_COMPLETED for u"
                                    + userId);
                            mBootCompleted = true;
                        }
                    }, 0, null, null,
                    new String[]{android.Manifest.permission.RECEIVE_BOOT_COMPLETED},
                    AppOpsManager.OP_NONE, null, true, false, MY_PID, SYSTEM_UID, // ordered = true
                    callingUid, callingPid, userId);
        });
    }
