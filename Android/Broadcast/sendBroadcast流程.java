// 文件分布：frameworks/base/core/java/android/app/ContextImpl.java

    public void sendBroadcastAsUser(Intent intent, UserHandle user,
            String receiverPermission, int appOp) {
        String resolvedType = intent.resolveTypeIfNeeded(getContentResolver());
        String[] receiverPermissions = receiverPermission == null ? null
                : new String[] {receiverPermission};
        try {
            intent.prepareToLeaveProcess(this);
            ActivityManager.getService().broadcastIntentWithFeature(
                    mMainThread.getApplicationThread(), getAttributionTag(), intent, resolvedType,
                    null, Activity.RESULT_OK, null, null, receiverPermissions, appOp, null, false,
                    false, user.getIdentifier());
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }





// 文件分布：frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java


    public final int broadcastIntentWithFeature(IApplicationThread caller, String callingFeatureId,
            Intent intent, String resolvedType, IIntentReceiver resultTo,
            int resultCode, String resultData, Bundle resultExtras,
            String[] requiredPermissions, int appOp, Bundle bOptions,
            boolean serialized, boolean sticky, int userId) {
        enforceNotIsolatedCaller("broadcastIntent");
        synchronized(this) {
            intent = verifyBroadcastLocked(intent);

            final ProcessRecord callerApp = getRecordForAppLocked(caller);
            final int callingPid = Binder.getCallingPid();
            final int callingUid = Binder.getCallingUid();

            final long origId = Binder.clearCallingIdentity();
            try {
                return broadcastIntentLocked(callerApp,
                        callerApp != null ? callerApp.info.packageName : null, callingFeatureId,
                        intent, resolvedType, resultTo, resultCode, resultData, resultExtras,
                        requiredPermissions, appOp, bOptions, serialized, sticky,
                        callingPid, callingUid, callingUid, callingPid, userId);
            } finally {
                Binder.restoreCallingIdentity(origId);
            }
        }
    }


    // 操作比较多，善用代码折叠
    // 1. 默认不发到stopped
    // 2. 不允许boot完成前拉起process
    // 3. user运行校验
    // 4. brOption
    // 5. protected广播
    // 6. 特殊广播单独处理
    // 7. sticky list处理
    // 8. 先处理无序&&动态
    @GuardedBy("this")
    final int broadcastIntentLocked(ProcessRecord callerApp, String callerPackage,
            @Nullable String callerFeatureId, Intent intent, String resolvedType,
            IIntentReceiver resultTo, int resultCode, String resultData,
            Bundle resultExtras, String[] requiredPermissions, int appOp, Bundle bOptions,
            boolean ordered, boolean sticky, int callingPid, int callingUid, int realCallingUid,
            int realCallingPid, int userId, boolean allowBackgroundActivityStarts,
            @Nullable int[] broadcastWhitelist) {
        intent = new Intent(intent);
// SKIP START
        final boolean callerInstantApp = isInstantApp(callerApp, callerPackage, callingUid);
        // Instant Apps cannot use FLAG_RECEIVER_VISIBLE_TO_INSTANT_APPS
        if (callerInstantApp) {
            intent.setFlags(intent.getFlags() & ~Intent.FLAG_RECEIVER_VISIBLE_TO_INSTANT_APPS);
        }

        if (userId == UserHandle.USER_ALL && broadcastWhitelist != null) {
                Slog.e(TAG, "broadcastWhitelist only applies when sending to individual users. "
                        + "Assuming restrictive whitelist.");
                broadcastWhitelist = new int[]{};
        }
// SKIP END
        // By default broadcasts do not go to stopped apps. 不会send到stopped的pkg
        intent.addFlags(Intent.FLAG_EXCLUDE_STOPPED_PACKAGES);

        // If we have not finished booting, don't allow this to launch new processes.
        // 如果没有完成boot，加上registered only避免被拉起process
        if (!mProcessesReady && (intent.getFlags()&Intent.FLAG_RECEIVER_BOOT_UPGRADE) == 0) {
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
        }

        if (DEBUG_BROADCAST_LIGHT) Slog.v(TAG_BROADCAST,
                (sticky ? "Broadcast sticky: ": "Broadcast: ") + intent
                + " ordered=" + ordered + " userid=" + userId);
        if ((resultTo != null) && !ordered) {
            Slog.w(TAG, "Broadcast " + intent + " not ordered but result callback requested!");
        }

        userId = mUserController.handleIncomingUser(callingPid, callingUid, userId, true,
                ALLOW_NON_FULL, "broadcast", callerPackage);

        // Make sure that the user who is receiving this broadcast or its parent is running.
        // If not, we will just skip it. Make an exception for shutdown broadcasts, upgrade steps.
        // check user是否在运行
        if (userId != UserHandle.USER_ALL && !mUserController.isUserOrItsParentRunning(userId)) {
            if ((callingUid != SYSTEM_UID
                    || (intent.getFlags() & Intent.FLAG_RECEIVER_BOOT_UPGRADE) == 0)
                    && !Intent.ACTION_SHUTDOWN.equals(intent.getAction())) {
                Slog.w(TAG, "Skipping broadcast of " + intent
                        + ": user " + userId + " and its parent (if any) are stopped");
                return ActivityManager.BROADCAST_FAILED_USER_STOPPED;
            }
        }
        // 处理broption，一般懒得看
        final String action = intent.getAction();
        BroadcastOptions brOptions = null;
        if (bOptions != null) {
            brOptions = new BroadcastOptions(bOptions);
            if (brOptions.getTemporaryAppWhitelistDuration() > 0) {
                // See if the caller is allowed to do this.  Note we are checking against
                // the actual real caller (not whoever provided the operation as say a
                // PendingIntent), because that who is actually supplied the arguments.
                if (checkComponentPermission(
                        android.Manifest.permission.CHANGE_DEVICE_IDLE_TEMP_WHITELIST,
                        realCallingPid, realCallingUid, -1, true)
                        != PackageManager.PERMISSION_GRANTED) {
                    String msg = "Permission Denial: " + intent.getAction()
                            + " broadcast from " + callerPackage + " (pid=" + callingPid
                            + ", uid=" + callingUid + ")"
                            + " requires "
                            + android.Manifest.permission.CHANGE_DEVICE_IDLE_TEMP_WHITELIST;
                    Slog.w(TAG, msg);
                    throw new SecurityException(msg);
                }
            }
            if (brOptions.isDontSendToRestrictedApps()
                    && !isUidActiveLocked(callingUid)
                    && isBackgroundRestrictedNoCheck(callingUid, callerPackage)) {
                Slog.i(TAG, "Not sending broadcast " + action + " - app " + callerPackage
                        + " has background restrictions");
                return ActivityManager.START_CANCELED;
            }
            if (brOptions.allowsBackgroundActivityStarts()) {
                // See if the caller is allowed to do this.  Note we are checking against
                // the actual real caller (not whoever provided the operation as say a
                // PendingIntent), because that who is actually supplied the arguments.
                if (checkComponentPermission(
                        android.Manifest.permission.START_ACTIVITIES_FROM_BACKGROUND,
                        realCallingPid, realCallingUid, -1, true)
                        != PackageManager.PERMISSION_GRANTED) {
                    String msg = "Permission Denial: " + intent.getAction()
                            + " broadcast from " + callerPackage + " (pid=" + callingPid
                            + ", uid=" + callingUid + ")"
                            + " requires "
                            + android.Manifest.permission.START_ACTIVITIES_FROM_BACKGROUND;
                    Slog.w(TAG, msg);
                    throw new SecurityException(msg);
                } else {
                    allowBackgroundActivityStarts = true;
                }
            }
        }

        // Verify that protected broadcasts are only being sent by system code,
        // and that system code is only sending protected broadcasts.
        // check 是不是protected广播
        final boolean isProtectedBroadcast;
        try {
            isProtectedBroadcast = AppGlobals.getPackageManager().isProtectedBroadcast(action);
        } catch (RemoteException e) {
            Slog.w(TAG, "Remote exception", e);
            return ActivityManager.BROADCAST_SUCCESS;
        }
        // check 是不是system
        final boolean isCallerSystem;
        switch (UserHandle.getAppId(callingUid)) {
            case ROOT_UID:
            case SYSTEM_UID:
            case PHONE_UID:
            case BLUETOOTH_UID:
            case NFC_UID:
            case SE_UID:
            case NETWORK_STACK_UID:
                isCallerSystem = true;
                break;
            default:
                isCallerSystem = (callerApp != null) && callerApp.isPersistent();
                break;
        }

        // First line security check before anything else: stop non-system apps from
        // sending protected broadcasts.
        // 处理protected广播
        if (!isCallerSystem) {
            if (isProtectedBroadcast) {
                String msg = "Permission Denial: not allowed to send broadcast "
                        + action + " from pid="
                        + callingPid + ", uid=" + callingUid;
                Slog.w(TAG, msg);
                throw new SecurityException(msg);

            } else if (AppWidgetManager.ACTION_APPWIDGET_CONFIGURE.equals(action)
                    || AppWidgetManager.ACTION_APPWIDGET_UPDATE.equals(action)) {
                // Special case for compatibility: we don't want apps to send this,
                // but historically it has not been protected and apps may be using it
                // to poke their own app widget.  So, instead of making it protected,
                // just limit it to the caller.
                if (callerPackage == null) {
                    String msg = "Permission Denial: not allowed to send broadcast "
                            + action + " from unknown caller.";
                    Slog.w(TAG, msg);
                    throw new SecurityException(msg);
                } else if (intent.getComponent() != null) {
                    // They are good enough to send to an explicit component...  verify
                    // it is being sent to the calling app.
                    if (!intent.getComponent().getPackageName().equals(
                            callerPackage)) {
                        String msg = "Permission Denial: not allowed to send broadcast "
                                + action + " to "
                                + intent.getComponent().getPackageName() + " from "
                                + callerPackage;
                        Slog.w(TAG, msg);
                        throw new SecurityException(msg);
                    }
                } else {
                    // Limit broadcast to their own package.
                    intent.setPackage(callerPackage);
                }
            }
        }

        boolean timeoutExempt = false;

        if (action != null) {
            if (getBackgroundLaunchBroadcasts().contains(action)) {
                if (DEBUG_BACKGROUND_CHECK) {
                    Slog.i(TAG, "Broadcast action " + action + " forcing include-background");
                }
                intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
            }
// SKIP START
            // 处理特殊广播，折叠
            switch (action) {
                case Intent.ACTION_UID_REMOVED:
                case Intent.ACTION_PACKAGE_REMOVED:
                case Intent.ACTION_PACKAGE_CHANGED:
                case Intent.ACTION_EXTERNAL_APPLICATIONS_UNAVAILABLE:
                case Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE:
                case Intent.ACTION_PACKAGES_SUSPENDED:
                case Intent.ACTION_PACKAGES_UNSUSPENDED:
                    // Handle special intents: if this broadcast is from the package
                    // manager about a package being removed, we need to remove all of
                    // its activities from the history stack.
                    if (checkComponentPermission(
                            android.Manifest.permission.BROADCAST_PACKAGE_REMOVED,
                            callingPid, callingUid, -1, true)
                            != PackageManager.PERMISSION_GRANTED) {
                        String msg = "Permission Denial: " + intent.getAction()
                                + " broadcast from " + callerPackage + " (pid=" + callingPid
                                + ", uid=" + callingUid + ")"
                                + " requires "
                                + android.Manifest.permission.BROADCAST_PACKAGE_REMOVED;
                        Slog.w(TAG, msg);
                        throw new SecurityException(msg);
                    }
                    switch (action) {
                        case Intent.ACTION_UID_REMOVED:
                            final int uid = getUidFromIntent(intent);
                            if (uid >= 0) {
                                mBatteryStatsService.removeUid(uid);
                                if (intent.getBooleanExtra(Intent.EXTRA_REPLACING, false)) {
                                    mAppOpsService.resetAllModes(UserHandle.getUserId(uid),
                                            intent.getStringExtra(Intent.EXTRA_PACKAGE_NAME));
                                } else {
                                    mAppOpsService.uidRemoved(uid);
                                }
                            }
                            break;
                        case Intent.ACTION_EXTERNAL_APPLICATIONS_UNAVAILABLE:
                            // If resources are unavailable just force stop all those packages
                            // and flush the attribute cache as well.
                            String list[] =
                                    intent.getStringArrayExtra(Intent.EXTRA_CHANGED_PACKAGE_LIST);
                            if (list != null && list.length > 0) {
                                for (int i = 0; i < list.length; i++) {
                                    forceStopPackageLocked(list[i], -1, false, true, true,
                                            false, false, userId, "storage unmount");
                                }
                                mAtmInternal.cleanupRecentTasksForUser(UserHandle.USER_ALL);
                                sendPackageBroadcastLocked(
                                        ApplicationThreadConstants.EXTERNAL_STORAGE_UNAVAILABLE,
                                        list, userId);
                            }
                            break;
                        case Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE:
                            mAtmInternal.cleanupRecentTasksForUser(UserHandle.USER_ALL);
                            break;
                        case Intent.ACTION_PACKAGE_REMOVED:
                        case Intent.ACTION_PACKAGE_CHANGED:
                            Uri data = intent.getData();
                            String ssp;
                            if (data != null && (ssp=data.getSchemeSpecificPart()) != null) {
                                boolean removed = Intent.ACTION_PACKAGE_REMOVED.equals(action);
                                final boolean replacing =
                                        intent.getBooleanExtra(Intent.EXTRA_REPLACING, false);
                                final boolean killProcess =
                                        !intent.getBooleanExtra(Intent.EXTRA_DONT_KILL_APP, false);
                                final boolean fullUninstall = removed && !replacing;
                                if (removed) {
                                    if (killProcess) {
                                        forceStopPackageLocked(ssp, UserHandle.getAppId(
                                                intent.getIntExtra(Intent.EXTRA_UID, -1)),
                                                false, true, true, false, fullUninstall, userId,
                                                removed ? "pkg removed" : "pkg changed");
                                    } else {
                                        // Kill any app zygotes always, since they can't fork new
                                        // processes with references to the old code
                                        forceStopAppZygoteLocked(ssp, UserHandle.getAppId(
                                                intent.getIntExtra(Intent.EXTRA_UID, -1)),
                                                userId);
                                    }
                                    final int cmd = killProcess
                                            ? ApplicationThreadConstants.PACKAGE_REMOVED
                                            : ApplicationThreadConstants.PACKAGE_REMOVED_DONT_KILL;
                                    sendPackageBroadcastLocked(cmd,
                                            new String[] {ssp}, userId);
                                    if (fullUninstall) {
                                        mAppOpsService.packageRemoved(
                                                intent.getIntExtra(Intent.EXTRA_UID, -1), ssp);

                                        // Remove all permissions granted from/to this package
                                        mUgmInternal.removeUriPermissionsForPackage(ssp, userId,
                                                true, false);

                                        mAtmInternal.removeRecentTasksByPackageName(ssp, userId);

                                        mServices.forceStopPackageLocked(ssp, userId);
                                        mAtmInternal.onPackageUninstalled(ssp);
                                        mBatteryStatsService.notePackageUninstalled(ssp);
                                    }
                                } else {
                                    if (killProcess) {
                                        final int extraUid = intent.getIntExtra(Intent.EXTRA_UID,
                                                -1);
                                        mProcessList.killPackageProcessesLocked(ssp,
                                                UserHandle.getAppId(extraUid),
                                                userId, ProcessList.INVALID_ADJ,
                                                ApplicationExitInfo.REASON_USER_REQUESTED,
                                                ApplicationExitInfo.SUBREASON_UNKNOWN,
                                                "change " + ssp);
                                    }
                                    cleanupDisabledPackageComponentsLocked(ssp, userId,
                                            intent.getStringArrayExtra(
                                                    Intent.EXTRA_CHANGED_COMPONENT_NAME_LIST));
                                }
                            }
                            break;
                        case Intent.ACTION_PACKAGES_SUSPENDED:
                        case Intent.ACTION_PACKAGES_UNSUSPENDED:
                            final boolean suspended = Intent.ACTION_PACKAGES_SUSPENDED.equals(
                                    intent.getAction());
                            final String[] packageNames = intent.getStringArrayExtra(
                                    Intent.EXTRA_CHANGED_PACKAGE_LIST);
                            final int userIdExtra = intent.getIntExtra(
                                    Intent.EXTRA_USER_HANDLE, UserHandle.USER_NULL);

                            mAtmInternal.onPackagesSuspendedChanged(packageNames, suspended,
                                    userIdExtra);
                            break;
                    }
                    break;
                case Intent.ACTION_PACKAGE_REPLACED:
                {
                    final Uri data = intent.getData();
                    final String ssp;
                    if (data != null && (ssp = data.getSchemeSpecificPart()) != null) {
                        ApplicationInfo aInfo = null;
                        try {
                            aInfo = AppGlobals.getPackageManager()
                                    .getApplicationInfo(ssp, STOCK_PM_FLAGS, userId);
                        } catch (RemoteException ignore) {}
                        if (aInfo == null) {
                            Slog.w(TAG, "Dropping ACTION_PACKAGE_REPLACED for non-existent pkg:"
                                    + " ssp=" + ssp + " data=" + data);
                            return ActivityManager.BROADCAST_SUCCESS;
                        }
                        updateAssociationForApp(aInfo);
                        mAtmInternal.onPackageReplaced(aInfo);
                        mServices.updateServiceApplicationInfoLocked(aInfo);
                        sendPackageBroadcastLocked(ApplicationThreadConstants.PACKAGE_REPLACED,
                                new String[] {ssp}, userId);
                    }
                    break;
                }
                case Intent.ACTION_PACKAGE_ADDED:
                {
                    // Special case for adding a package: by default turn on compatibility mode.
                    Uri data = intent.getData();
                    String ssp;
                    if (data != null && (ssp = data.getSchemeSpecificPart()) != null) {
                        final boolean replacing =
                                intent.getBooleanExtra(Intent.EXTRA_REPLACING, false);
                        mAtmInternal.onPackageAdded(ssp, replacing);

                        try {
                            ApplicationInfo ai = AppGlobals.getPackageManager().
                                    getApplicationInfo(ssp, STOCK_PM_FLAGS, 0);
                            mBatteryStatsService.notePackageInstalled(ssp,
                                    ai != null ? ai.longVersionCode : 0);
                        } catch (RemoteException e) {
                        }
                    }
                    break;
                }
                case Intent.ACTION_PACKAGE_DATA_CLEARED:
                {
                    Uri data = intent.getData();
                    String ssp;
                    if (data != null && (ssp = data.getSchemeSpecificPart()) != null) {
                        mAtmInternal.onPackageDataCleared(ssp);
                    }
                    break;
                }
                case Intent.ACTION_TIMEZONE_CHANGED:
                    // If this is the time zone changed action, queue up a message that will reset
                    // the timezone of all currently running processes. This message will get
                    // queued up before the broadcast happens.
                    mHandler.sendEmptyMessage(UPDATE_TIME_ZONE);
                    break;
                case Intent.ACTION_TIME_CHANGED:
                    // EXTRA_TIME_PREF_24_HOUR_FORMAT is optional so we must distinguish between
                    // the tri-state value it may contain and "unknown".
                    // For convenience we re-use the Intent extra values.
                    final int NO_EXTRA_VALUE_FOUND = -1;
                    final int timeFormatPreferenceMsgValue = intent.getIntExtra(
                            Intent.EXTRA_TIME_PREF_24_HOUR_FORMAT,
                            NO_EXTRA_VALUE_FOUND /* defaultValue */);
                    // Only send a message if the time preference is available.
                    if (timeFormatPreferenceMsgValue != NO_EXTRA_VALUE_FOUND) {
                        Message updateTimePreferenceMsg =
                                mHandler.obtainMessage(UPDATE_TIME_PREFERENCE_MSG,
                                        timeFormatPreferenceMsgValue, 0);
                        mHandler.sendMessage(updateTimePreferenceMsg);
                    }
                    BatteryStatsImpl stats = mBatteryStatsService.getActiveStatistics();
                    synchronized (stats) {
                        stats.noteCurrentTimeChangedLocked();
                    }
                    break;
                case Intent.ACTION_CLEAR_DNS_CACHE:
                    mHandler.sendEmptyMessage(CLEAR_DNS_CACHE_MSG);
                    break;
                case Proxy.PROXY_CHANGE_ACTION:
                    mHandler.sendMessage(mHandler.obtainMessage(UPDATE_HTTP_PROXY_MSG));
                    break;
                case android.hardware.Camera.ACTION_NEW_PICTURE:
                case android.hardware.Camera.ACTION_NEW_VIDEO:
                    // In N we just turned these off; in O we are turing them back on partly,
                    // only for registered receivers.  This will still address the main problem
                    // (a spam of apps waking up when a picture is taken putting significant
                    // memory pressure on the system at a bad point), while still allowing apps
                    // that are already actively running to know about this happening.
                    intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
                    break;
                case android.security.KeyChain.ACTION_TRUST_STORE_CHANGED:
                    mHandler.sendEmptyMessage(HANDLE_TRUST_STORAGE_UPDATE_MSG);
                    break;
                case "com.android.launcher.action.INSTALL_SHORTCUT":
                    // As of O, we no longer support this broadcasts, even for pre-O apps.
                    // Apps should now be using ShortcutManager.pinRequestShortcut().
                    Log.w(TAG, "Broadcast " + action
                            + " no longer supported. It will not be delivered.");
                    return ActivityManager.BROADCAST_SUCCESS;
                case Intent.ACTION_PRE_BOOT_COMPLETED:
                    timeoutExempt = true;
                    break;
            }
// SKIP END
            if (Intent.ACTION_PACKAGE_ADDED.equals(action) ||
                    Intent.ACTION_PACKAGE_REMOVED.equals(action) ||
                    Intent.ACTION_PACKAGE_REPLACED.equals(action)) {
                final int uid = getUidFromIntent(intent);
                if (uid != -1) {
                    final UidRecord uidRec = mProcessList.getUidRecordLocked(uid);
                    if (uidRec != null) {
                        uidRec.updateHasInternetPermission();
                    }
                }
            }
        }

        // Add to the sticky list if requested.
        // sticky处理
        if (sticky) {
            if (checkPermission(android.Manifest.permission.BROADCAST_STICKY,
                    callingPid, callingUid)
                    != PackageManager.PERMISSION_GRANTED) {
                String msg = "Permission Denial: broadcastIntent() requesting a sticky broadcast from pid="
                        + callingPid + ", uid=" + callingUid
                        + " requires " + android.Manifest.permission.BROADCAST_STICKY;
                Slog.w(TAG, msg);
                throw new SecurityException(msg);
            }
            if (requiredPermissions != null && requiredPermissions.length > 0) {
                Slog.w(TAG, "Can't broadcast sticky intent " + intent
                        + " and enforce permissions " + Arrays.toString(requiredPermissions));
                return ActivityManager.BROADCAST_STICKY_CANT_HAVE_PERMISSION;
            }
            // sticky不允许指定特定的接收对象
            if (intent.getComponent() != null) {
                throw new SecurityException(
                        "Sticky broadcasts can't target a specific component");
            }
            // We use userId directly here, since the "all" target is maintained
            // as a separate set of sticky broadcasts.
            if (userId != UserHandle.USER_ALL) {
                // But first, if this is not a broadcast to all users, then
                // make sure it doesn't conflict with an existing broadcast to
                // all users.
                ArrayMap<String, ArrayList<Intent>> stickies = mStickyBroadcasts.get(
                        UserHandle.USER_ALL);
                if (stickies != null) {
                    ArrayList<Intent> list = stickies.get(intent.getAction());
                    if (list != null) {
                        int N = list.size();
                        int i;
                        for (i=0; i<N; i++) {
                            if (intent.filterEquals(list.get(i))) {
                                throw new IllegalArgumentException(
                                        "Sticky broadcast " + intent + " for user "
                                        + userId + " conflicts with existing global broadcast");
                            }
                        }
                    }
                }
            }
            ArrayMap<String, ArrayList<Intent>> stickies = mStickyBroadcasts.get(userId);
            if (stickies == null) {
                stickies = new ArrayMap<>();
                mStickyBroadcasts.put(userId, stickies);
            }
            ArrayList<Intent> list = stickies.get(intent.getAction());
            if (list == null) {
                list = new ArrayList<>();
                stickies.put(intent.getAction(), list);
            }
            final int stickiesCount = list.size();
            int i;
            for (i = 0; i < stickiesCount; i++) {
                if (intent.filterEquals(list.get(i))) {
                    // This sticky already exists, replace it.
                    list.set(i, new Intent(intent));
                    break;
                }
            }
            if (i >= stickiesCount) {
                list.add(new Intent(intent));
            }
        }

        int[] users;
        if (userId == UserHandle.USER_ALL) {
            // Caller wants broadcast to go to all started users.
            users = mUserController.getStartedUserArray();
        } else {
            // Caller wants broadcast to go to one specific user.
            users = new int[] {userId};
        }

        // Figure out who all will receive this broadcast.
        List receivers = null;
        List<BroadcastFilter> registeredReceivers = null;
        // Need to resolve the intent to interested receivers...
        if ((intent.getFlags()&Intent.FLAG_RECEIVER_REGISTERED_ONLY)
                 == 0) {
            receivers = collectReceiverComponents(
                    intent, resolvedType, callingUid, users, broadcastWhitelist);
        }
        if (intent.getComponent() == null) {
            if (userId == UserHandle.USER_ALL && callingUid == SHELL_UID) {
                // Query one target user at a time, excluding shell-restricted users
                for (int i = 0; i < users.length; i++) {
                    if (mUserController.hasUserRestriction(
                            UserManager.DISALLOW_DEBUGGING_FEATURES, users[i])) {
                        continue;
                    }
                    List<BroadcastFilter> registeredReceiversForUser =
                            mReceiverResolver.queryIntent(intent,
                                    resolvedType, false /*defaultOnly*/, users[i]);
                    if (registeredReceivers == null) {
                        registeredReceivers = registeredReceiversForUser;
                    } else if (registeredReceiversForUser != null) {
                        registeredReceivers.addAll(registeredReceiversForUser);
                    }
                }
            } else {
                registeredReceivers = mReceiverResolver.queryIntent(intent,
                        resolvedType, false /*defaultOnly*/, userId);
            }
        }

        final boolean replacePending =
                (intent.getFlags()&Intent.FLAG_RECEIVER_REPLACE_PENDING) != 0;

        if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST, "Enqueueing broadcast: " + intent.getAction()
                + " replacePending=" + replacePending);
        if (registeredReceivers != null && broadcastWhitelist != null) {
            // if a uid whitelist was provided, remove anything in the application space that wasn't
            // in it.
            for (int i = registeredReceivers.size() - 1; i >= 0; i--) {
                final int owningAppId = UserHandle.getAppId(registeredReceivers.get(i).owningUid);
                if (owningAppId >= Process.FIRST_APPLICATION_UID
                        && Arrays.binarySearch(broadcastWhitelist, owningAppId) < 0) {
                    registeredReceivers.remove(i);
                }
            }
        }

        int NR = registeredReceivers != null ? registeredReceivers.size() : 0;
        // 处理无序且动态注册的receiver，处理完后receiver清空
        if (!ordered && NR > 0) {
            // If we are not serializing this broadcast, then send the
            // registered receivers separately so they don't wait for the
            // components to be launched.
            if (isCallerSystem) {
                checkBroadcastFromSystem(intent, callerApp, callerPackage, callingUid,
                        isProtectedBroadcast, registeredReceivers);
            }
            final BroadcastQueue queue = broadcastQueueForIntent(intent);
            BroadcastRecord r = new BroadcastRecord(queue, intent, callerApp, callerPackage,
                    callerFeatureId, callingPid, callingUid, callerInstantApp, resolvedType,
                    requiredPermissions, appOp, brOptions, registeredReceivers, resultTo,
                    resultCode, resultData, resultExtras, ordered, sticky, false, userId,
                    allowBackgroundActivityStarts, timeoutExempt);
            if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST, "Enqueueing parallel broadcast " + r);
            final boolean replaced = replacePending
                    && (queue.replaceParallelBroadcastLocked(r) != null);
            // Note: We assume resultTo is null for non-ordered broadcasts.
            if (!replaced) {
                queue.enqueueParallelBroadcastLocked(r);
                queue.scheduleBroadcastsLocked();
            }
            registeredReceivers = null;
            NR = 0;
        }

        // Merge into one list.
        int ir = 0;
        if (receivers != null) {
            // A special case for PACKAGE_ADDED: do not allow the package
            // being added to see this broadcast.  This prevents them from
            // using this as a back door to get run as soon as they are
            // installed.  Maybe in the future we want to have a special install
            // broadcast or such for apps, but we'd like to deliberately make
            // this decision.
            String skipPackages[] = null;
            if (Intent.ACTION_PACKAGE_ADDED.equals(intent.getAction())
                    || Intent.ACTION_PACKAGE_RESTARTED.equals(intent.getAction())
                    || Intent.ACTION_PACKAGE_DATA_CLEARED.equals(intent.getAction())) {
                Uri data = intent.getData();
                if (data != null) {
                    String pkgName = data.getSchemeSpecificPart();
                    if (pkgName != null) {
                        skipPackages = new String[] { pkgName };
                    }
                }
            } else if (Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE.equals(intent.getAction())) {
                skipPackages = intent.getStringArrayExtra(Intent.EXTRA_CHANGED_PACKAGE_LIST);
            }
            if (skipPackages != null && (skipPackages.length > 0)) {
                for (String skipPackage : skipPackages) {
                    if (skipPackage != null) {
                        int NT = receivers.size();
                        for (int it=0; it<NT; it++) {
                            ResolveInfo curt = (ResolveInfo)receivers.get(it);
                            if (curt.activityInfo.packageName.equals(skipPackage)) {
                                receivers.remove(it);
                                it--;
                                NT--;
                            }
                        }
                    }
                }
            }

            int NT = receivers != null ? receivers.size() : 0;
            int it = 0;
            ResolveInfo curt = null;
            BroadcastFilter curr = null;
            while (it < NT && ir < NR) {
                if (curt == null) {
                    curt = (ResolveInfo)receivers.get(it);
                }
                if (curr == null) {
                    curr = registeredReceivers.get(ir);
                }
                if (curr.getPriority() >= curt.priority) {
                    // Insert this broadcast record into the final list.
                    receivers.add(it, curr);
                    ir++;
                    curr = null;
                    it++;
                    NT++;
                } else {
                    // Skip to the next ResolveInfo in the final list.
                    it++;
                    curt = null;
                }
            }
        }
        while (ir < NR) {
            if (receivers == null) {
                receivers = new ArrayList();
            }
            receivers.add(registeredReceivers.get(ir));
            ir++;
        }

        if (isCallerSystem) {
            checkBroadcastFromSystem(intent, callerApp, callerPackage, callingUid,
                    isProtectedBroadcast, receivers);
        }

        if ((receivers != null && receivers.size() > 0)
                || resultTo != null) {
            BroadcastQueue queue = broadcastQueueForIntent(intent);
            BroadcastRecord r = new BroadcastRecord(queue, intent, callerApp, callerPackage,
                    callerFeatureId, callingPid, callingUid, callerInstantApp, resolvedType,
                    requiredPermissions, appOp, brOptions, receivers, resultTo, resultCode,
                    resultData, resultExtras, ordered, sticky, false, userId,
                    allowBackgroundActivityStarts, timeoutExempt);

            if (DEBUG_BROADCAST) Slog.v(TAG_BROADCAST, "Enqueueing ordered broadcast " + r);

            final BroadcastRecord oldRecord =
                    replacePending ? queue.replaceOrderedBroadcastLocked(r) : null;
            if (oldRecord != null) {
                // Replaced, fire the result-to receiver.
                if (oldRecord.resultTo != null) {
                    final BroadcastQueue oldQueue = broadcastQueueForIntent(oldRecord.intent);
                    try {
                        oldQueue.performReceiveLocked(oldRecord.callerApp, oldRecord.resultTo,
                                oldRecord.intent,
                                Activity.RESULT_CANCELED, null, null,
                                false, false, oldRecord.userId);
                    } catch (RemoteException e) {
                        Slog.w(TAG, "Failure ["
                                + queue.mQueueName + "] sending broadcast result of "
                                + intent, e);

                    }
                }
            } else {
                queue.enqueueOrderedBroadcastLocked(r);
                queue.scheduleBroadcastsLocked();
            }
        } else {
            // There was nobody interested in the broadcast, but we still want to record
            // that it happened.
            if (intent.getComponent() == null && intent.getPackage() == null
                    && (intent.getFlags()&Intent.FLAG_RECEIVER_REGISTERED_ONLY) == 0) {
                // This was an implicit broadcast... let's record it for posterity.
                addBroadcastStatLocked(intent.getAction(), callerPackage, 0, 0, 0);
            }
        }

        return ActivityManager.BROADCAST_SUCCESS;
    }
